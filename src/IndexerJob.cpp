#include "IndexerJob.h"
#include "Timer.h"
#include "MemoryMonitor.h"
#include "Server.h"
#include "EventLoop.h"

struct DumpUserData {
    int indent;
    IndexerJob *job;
};

struct FindImplicitEqualsConstructorUserData {
    CXCursor &ref;
    bool &success;
};

struct VerboseVisitorUserData {
    int indent;
    ByteArray out;
    IndexerJob *job;
};

IndexerJob::IndexerJob(const shared_ptr<Indexer> &indexer, unsigned flags, const Path &p, const List<ByteArray> &arguments)
    : Job(0, indexer->project()),
      mFlags(flags), mTimeStamp(0), mPath(p), mFileId(Location::insertFile(p)),
      mArgs(arguments), mIndexer(indexer), mUnit(0), mIndex(0),
      mIgnoreConstructorRefs(false)
{
}

IndexerJob::IndexerJob(const QueryMessage &msg, const shared_ptr<Project> &project,
                       const Path &input, const List<ByteArray> &arguments)
    : Job(msg, WriteUnfiltered|WriteBuffered, project), mFlags(0), mTimeStamp(0), mPath(input), mFileId(Location::insertFile(input)),
      mArgs(arguments), mUnit(0), mIndex(0), mIgnoreConstructorRefs(false)
{
}

void IndexerJob::inclusionVisitor(CXFile includedFile,
                                  CXSourceLocation *includeStack,
                                  unsigned includeLen,
                                  CXClientData userData)
{
    IndexerJob *job = static_cast<IndexerJob*>(userData);
    const Location l(includedFile, 0);

    const Path path = l.path();
    job->mData->symbolNames[path].insert(l);
    const char *fn = path.fileName();
    job->mData->symbolNames[ByteArray(fn, strlen(fn))].insert(l);

    const uint32_t fileId = l.fileId();
    if (!includeLen) {
        job->mData->dependencies[fileId].insert(fileId);
    } else {
        for (unsigned i=0; i<includeLen; ++i) {
            CXFile originatingFile;
            clang_getSpellingLocation(includeStack[i], &originatingFile, 0, 0, 0);
            Location loc(originatingFile, 0);
            const uint32_t f = loc.fileId();
            if (f)
                job->mData->dependencies[fileId].insert(f);
        }
    }
}

static inline void addToSymbolNames(const ByteArray &arg, bool hasTemplates, const Location &location, SymbolNameMap &symbolNames)
{
    symbolNames[arg].insert(location);
    if (hasTemplates) {
        ByteArray copy = arg;
        const int lt = arg.indexOf('<');
        if (lt == -1)
            return;
        const int gt = arg.indexOf('>', lt + 1);
        if (gt == -1)
            return;
        if (gt + 1 == arg.size()) {
            copy.truncate(lt);
        } else {
            copy.remove(lt, gt - lt + 1);
        }

        symbolNames[copy].insert(location);
    }
}

static const CXCursor nullCursor = clang_getNullCursor();
ByteArray IndexerJob::addNamePermutations(const CXCursor &cursor, const Location &location, bool addToDB)
{
    ByteArray ret, qname, qparam, qnoparam;

    CXCursor cur = cursor;
    CXCursorKind kind;
    bool first = true;
    for (;;) {
        if (clang_equalCursors(cur, nullCursor))
            break;
        kind = clang_getCursorKind(cur);
        if (!first) {
            bool ok = false;
            switch (kind) {
            case CXCursor_Namespace:
            case CXCursor_ClassDecl:
            case CXCursor_ClassTemplate:
            case CXCursor_StructDecl:
            case CXCursor_CXXMethod:
            case CXCursor_Constructor:
            case CXCursor_Destructor:
            case CXCursor_FunctionDecl:
                ok = true;
                break;
            default:
                break;
            }
            if (!ok)
                break;
        }

        CXStringScope displayName(clang_getCursorDisplayName(cur));
        const char *name = clang_getCString(displayName.string);
        if (!name || !strlen(name)) {
            break;
        }
        qname = ByteArray(name);
        if (ret.isEmpty()) {
            ret = qname;
            if (!addToDB)
                return ret;
        }
        if (qparam.isEmpty()) {
            qparam = qname;
            const int sp = qparam.indexOf('(');
            if (sp != -1)
                qnoparam = qparam.left(sp);
        } else {
            qparam.prepend(qname + "::");
            if (!qnoparam.isEmpty())
                qnoparam.prepend(qname + "::");
        }

        assert(!qparam.isEmpty());
        bool hasTemplates = false;
        switch (kind) {
        case CXCursor_ClassTemplate:
        case CXCursor_Constructor:
        case CXCursor_Destructor:
            hasTemplates = qnoparam.contains('<');
            break;
        default:
            break;
        }

        addToSymbolNames(qparam, hasTemplates, location, mData->symbolNames);
        if (!qnoparam.isEmpty()) {
            assert(!qnoparam.isEmpty());
            addToSymbolNames(qnoparam, hasTemplates, location, mData->symbolNames);
        }

        if (first) {
            first = false;
            switch (kind) {
            case CXCursor_Namespace:
            case CXCursor_ClassDecl:
            case CXCursor_StructDecl:
            case CXCursor_CXXMethod:
            case CXCursor_Constructor:
            case CXCursor_FunctionDecl:
            case CXCursor_Destructor:
            case CXCursor_VarDecl:
            case CXCursor_ParmDecl:
            case CXCursor_FieldDecl:
            case CXCursor_ClassTemplate:
                break;
            default:
                // these don't need the scope
                return ret;
            }
        }

        cur = clang_getCursorSemanticParent(cur);
    }
    return ret;
}

static const CXSourceLocation nullLocation = clang_getNullLocation();
Location IndexerJob::createLocation(const CXCursor &cursor, bool *blocked)
{
    CXSourceLocation location = clang_getCursorLocation(cursor);
    Location ret;
    if (blocked)
        *blocked = false;
    if (!clang_equalLocations(location, nullLocation)) {
        CXFile file;
        unsigned start;
        clang_getSpellingLocation(location, &file, 0, 0, &start);
        if (file) {
            ByteArray fileName = RTags::eatString(clang_getFileName(file));
            uint32_t &fileId = mFileIds[fileName];
            if (!fileId)
                fileId = Location::insertFile(Path::resolved(fileName));
            ret = Location(fileId, start);
            if (blocked) {
                PathState &state = mPaths[fileId];
                if (state == Unset) {
                    shared_ptr<Indexer> indexer = mIndexer.lock();
                    state = indexer && indexer->visitFile(fileId, this) ? Index : DontIndex;
                }
                if (state != Index) {
                    *blocked = true;
                    return Location();
                }
            }
        }
    }
    return ret;
}

void IndexerJob::addOverriddenCursors(const CXCursor& cursor, const Location& location, List<CursorInfo*>& infos)
{
    CXCursor *overridden;
    unsigned count;
    clang_getOverriddenCursors(cursor, &overridden, &count);
    if (!overridden)
        return;
    for (unsigned i=0; i<count; ++i) {
        Location loc = createLocation(overridden[i], 0);
        CursorInfo &o = mData->symbols[loc];

        //error() << "adding overridden (1) " << location << " to " << o;
        o.references.insert(location);
        List<CursorInfo*>::const_iterator inf = infos.begin();
        const List<CursorInfo*>::const_iterator infend = infos.end();
        while (inf != infend) {
            //error() << "adding overridden (2) " << loc << " to " << *(*inf);
            (*inf)->references.insert(loc);
            ++inf;
        }

        infos.append(&o);
        addOverriddenCursors(overridden[i], loc, infos);
        infos.removeLast();
    }
    clang_disposeOverriddenCursors(overridden);
}

void IndexerJob::parse()
{
    mHeaderMap.clear();
    if (!mIndex) {
        mIndex = clang_createIndex(0, 1);
        if (!mIndex) {
            abort();
            return;
        }
    }

    mTimeStamp = time(0);
    List<const char*> clangArgs(mArgs.size(), 0);
    mClangLine = Server::instance()->clangPath();
    mClangLine += ' ';

    int idx = 0;
    const int count = mArgs.size();
    for (int i=0; i<count; ++i) {
        ByteArray arg = mArgs.at(i);
        if (arg.isEmpty())
            continue;

        clangArgs[idx++] = arg.constData();
        arg.replace("\"", "\\\"");
        mClangLine += arg;
        mClangLine += ' ';
    }

    mClangLine += mPath;

    CXIndexAction action = clang_IndexAction_create(mIndex);

    IndexerCallbacks cb;
    memset(&cb, 0, sizeof(IndexerCallbacks));
    cb.indexDeclaration = IndexerJob::indexDeclarations;
    cb.indexEntityReference = IndexerJob::indexEntityReferences;

    clang_indexSourceFile(action, this, &cb, sizeof(IndexerCallbacks),
                          CXIndexOpt_IndexFunctionLocalSymbols
                          // |CXIndexOpt_SuppressRedundantRefs // this one eats too much
                          |CXIndexOpt_IndexImplicitTemplateInstantiations,
                          mPath.constData(),
                          clangArgs.constData(), clangArgs.size(),
                          0, 0, &mUnit, clang_defaultEditingTranslationUnitOptions());

    clang_IndexAction_dispose(action);

    warning() << "loading unit " << mClangLine << " " << (mUnit != 0);
    if (!mUnit) {
        error() << "got failure" << mClangLine;
        mData->dependencies[mFileId].insert(mFileId);
    }
}

void IndexerJob::diagnose()
{
    const unsigned diagnosticCount = clang_getNumDiagnostics(mUnit);
    bool hasCompilationErrors = false;
    for (unsigned i=0; i<diagnosticCount; ++i) {
        CXDiagnostic diagnostic = clang_getDiagnostic(mUnit, i);
        int logLevel = INT_MAX;
        const CXDiagnosticSeverity severity = clang_getDiagnosticSeverity(diagnostic);
        switch (severity) {
        case CXDiagnostic_Fatal:
        case CXDiagnostic_Error:
            logLevel = Error;
            hasCompilationErrors = true;
            break;
        case CXDiagnostic_Warning:
            logLevel = Warning;
            hasCompilationErrors = true;
            break;
        case CXDiagnostic_Note:
            logLevel = Debug;
            break;
        case CXDiagnostic_Ignored:
            break;
        }

        CXSourceLocation loc = clang_getDiagnosticLocation(diagnostic);
        const unsigned diagnosticOptions = (CXDiagnostic_DisplaySourceLocation|
                                            CXDiagnostic_DisplayColumn|
                                            CXDiagnostic_DisplaySourceRanges|
                                            CXDiagnostic_DisplayOption|
                                            CXDiagnostic_DisplayCategoryId|
                                            CXDiagnostic_DisplayCategoryName);

        ByteArray string;
        CXFile file;
        clang_getSpellingLocation(loc, &file, 0, 0, 0);
        if (file) {
            string = RTags::eatString(clang_formatDiagnostic(diagnostic, diagnosticOptions));
            mData->diagnostics[Location(file, 0).fileId()].append(string);
        }
        if (testLog(logLevel) || (logLevel >= Warning && testLog(CompilationError))) {
            if (string.isEmpty())
                string = RTags::eatString(clang_formatDiagnostic(diagnostic, diagnosticOptions));
            log(logLevel, "%s: %s => %s", mPath.constData(), mClangLine.constData(), string.constData());
            log(CompilationError, "%s", string.constData());
        }

        const unsigned fixItCount = clang_getDiagnosticNumFixIts(diagnostic);
        for (unsigned f=0; f<fixItCount; ++f) {
            CXSourceRange range;
            ByteArray string = RTags::eatString(clang_getDiagnosticFixIt(diagnostic, f, &range));
            const Location start(clang_getRangeStart(range));
            unsigned endOffset = 0;
            clang_getSpellingLocation(clang_getRangeEnd(range), 0, 0, 0, &endOffset);

            error("Fixit (%d/%d) for %s: [%s] %s-%d", f + 1, fixItCount, mPath.constData(),
                  string.constData(), start.key().constData(), endOffset);
            mData->fixIts[start] = std::pair<int, ByteArray>(endOffset - start.offset(), string);
        }

        clang_disposeDiagnostic(diagnostic);
    }
    if (!hasCompilationErrors) {
        log(CompilationError, "%s parsed", mPath.constData());
    }
}

void IndexerJob::visit()
{
    if (!mUnit)
        return;
    clang_getInclusions(mUnit, inclusionVisitor, this);
    if (isAborted())
        return;

    // clang_visitChildren(clang_getTranslationUnitCursor(mUnit), indexVisitor, this);
    // if (isAborted())
    //     return;
    // if (testLog(VerboseDebug)) {
    //     VerboseVisitorUserData u = { 0, "<VerboseVisitor " + mClangLine + ">\n", this };
    //     clang_visitChildren(clang_getTranslationUnitCursor(mUnit), verboseVisitor, &u);
    //     u.out += "</VerboseVisitor " + mClangLine + ">";
    //     if (getenv("RTAGS_INDEXERJOB_DUMP_TO_FILE")) {
    //         char buf[1024];
    //         snprintf(buf, sizeof(buf), "/tmp/%s.log", mPath.fileName());
    //         FILE *f = fopen(buf, "w");
    //         assert(f);
    //         fwrite(u.out.constData(), 1, u.out.size(), f);
    //         fclose(f);
    //     } else {
    //         logDirect(VerboseDebug, u.out);
    //     }
    //     // {
    //     //     VerboseVisitorUserData u = { -1, "<VerboseVisitor2 " + clangLine + ">", this };
    //     //    clang_visitChildren(clang_getTranslationUnitCursor(mUnit), verboseVisitor, &u);
    //     //     u.out += "</VerboseVisitor2 " + clangLine + ">";
    //     //     logDirect(VerboseDebug, u.out);
    //     // }
    // }
}

void IndexerJob::run()
{
    mData.reset(new IndexData);
    if (mIndexer.lock()) {
        typedef void (IndexerJob::*Function)();
        Function functions[] = { &IndexerJob::parse, &IndexerJob::diagnose, &IndexerJob::visit };
        for (unsigned i=0; i<sizeof(functions) / sizeof(Function); ++i) {
            (this->*functions[i])();
            if (isAborted())
                break;
        }

        mHeaderMap.clear();
        char buf[1024];
        const int w = snprintf(buf, sizeof(buf), "Visited %s (%s) in %sms. (%d syms, %d deps, %d symNames)%s",
                               mPath.constData(), mUnit ? "success" : "error", ByteArray::number(mTimer.elapsed()).constData(),
                               mData->symbols.size(), mData->dependencies.size(), mData->symbolNames.size(),
                               mFlags & Dirty ? " (dirty)" : "");
        mData->message = ByteArray(buf, w);
    } else {
        parse();
        if (mUnit) {
            DumpUserData u = { 0, this };
            clang_visitChildren(clang_getTranslationUnitCursor(mUnit), dumpVisitor, &u);
        }
    }
    if (mUnit) {
        clang_disposeTranslationUnit(mUnit);
        mUnit = 0;
    }
    if (mIndex) {
        clang_disposeIndex(mIndex);
        mIndex = 0;
    }

    if (shared_ptr<Indexer> idx = indexer())
        idx->onJobFinished(this);
}

void IndexerJob::indexDeclarations(CXClientData userData, const CXIdxDeclInfo *decl)
{
    if (!decl->entityInfo->name) {
        // Could maybe do better with this but I want to keep it simple for now
        return;
    }

    CXFile f;
    unsigned o;
    clang_indexLoc_getFileLocation(decl->loc, 0, &f, 0, 0, &o);
    const Location location(f, o);
    IndexerJob *job = reinterpret_cast<IndexerJob*>(userData);
    if (location.isNull() || job->mData->symbols.contains(location)) {
        // ### is this safe? Could I have gotten a reference to something before
        // ### the declaration?
        return;
    }

    const CXCursor &cursor = decl->entityInfo->cursor;

    CursorInfo &info = job->mData->symbols[location];
    info.isDefinition = decl->isDefinition;
    info.kind = decl->entityInfo->kind;
    info.symbolName = decl->entityInfo->name;
    info.symbolLength = info.symbolName.size(); // could use getcursorrange stuff for this
    job->addNamePermutations(cursor, location, true);

    bool findDef = false;
    switch (info.kind) {
    case CXIdxEntity_Function:
    case CXIdxEntity_Struct:
    case CXIdxEntity_CXXClass:
    case CXIdxEntity_CXXStaticVariable:
    case CXIdxEntity_CXXStaticMethod:
    case CXIdxEntity_CXXConversionFunction:
        findDef = true;
        break;

    case CXIdxEntity_CXXConstructor:
    case CXIdxEntity_CXXDestructor: {
        findDef = true;
        Location parentLocation = job->createLocation(clang_getCursorSemanticParent(cursor));
        // consider doing this for only declaration/inline definition since
        // declaration and definition should know of one another
        if (parentLocation.isValid()) {
            CursorInfo &parent = job->mData->symbols[parentLocation];
            parent.references.insert(location);
            info.references.insert(parentLocation);
        }
        break; }
    case CXIdxEntity_CXXInstanceMethod: {
        List<CursorInfo*> infos;
        infos.append(&info);
        job->addOverriddenCursors(cursor, location, infos);
        break; }
    case CXIdxEntity_Unexposed:
    case CXIdxEntity_Typedef:
    case CXIdxEntity_Variable:
    case CXIdxEntity_Field:
    case CXIdxEntity_EnumConstant:
    case CXIdxEntity_ObjCClass:
    case CXIdxEntity_ObjCProtocol:
    case CXIdxEntity_ObjCCategory:
    case CXIdxEntity_ObjCInstanceMethod:
    case CXIdxEntity_ObjCClassMethod:
    case CXIdxEntity_ObjCProperty:
    case CXIdxEntity_ObjCIvar:
    case CXIdxEntity_Enum:
    case CXIdxEntity_Union:
    case CXIdxEntity_CXXNamespace:
    case CXIdxEntity_CXXNamespaceAlias:
    case CXIdxEntity_CXXTypeAlias:
    case CXIdxEntity_CXXInterface:
        break;
    }
    if (findDef && !info.isDefinition) {
        CXCursor def = clang_getCursorDefinition(cursor);
        Location defLoc = job->createLocation(def);
        if (!defLoc.isNull()) {
            info.target = defLoc;
            CursorInfo &other = job->mData->symbols[defLoc];
            if (other.target.isNull()) {
                other.target = location;
            }
        }
    }
}

void IndexerJob::indexEntityReferences(CXClientData userData, const CXIdxEntityRefInfo *ref)
{
    CXFile f;
    unsigned o;
    clang_indexLoc_getFileLocation(ref->loc, 0, &f, 0, 0, &o);
    const Location location(f, o);
    IndexerJob *job = reinterpret_cast<IndexerJob*>(userData);
    if (location.isNull())
        return;
    CXSourceLocation loc = clang_getCursorLocation(ref->referencedEntity->cursor);
    clang_getInstantiationLocation(loc, &f, 0, 0, &o);
    const Location refLoc(f, o);
    if (refLoc.isNull())
        return;

    CursorInfo &cursorInfo = job->mData->symbols[location];
    if (!cursorInfo.symbolLength) {
        cursorInfo.kind = CursorInfo::ReferenceKind;
        cursorInfo.symbolName = ref->referencedEntity->name;
        cursorInfo.symbolLength = cursorInfo.symbolName.size(); // could use getcursorrange stuff for this
        cursorInfo.target = refLoc;
    }
    CursorInfo &reffed = job->mData->symbols[refLoc];
    if (!reffed.symbolLength) {
        reffed.symbolName = cursorInfo.symbolName;
        reffed.symbolLength = cursorInfo.symbolLength;
        reffed.kind = ref->referencedEntity->kind;
        // error() << "We had to make up a reference at" << refLoc << "for" << cursorInfo;
    }
    reffed.references.insert(location);
}

CXChildVisitResult IndexerJob::verboseVisitor(CXCursor cursor, CXCursor, CXClientData userData)
{
    VerboseVisitorUserData *u = reinterpret_cast<VerboseVisitorUserData*>(userData);
    Location loc = u->job->createLocation(cursor);
    if (loc.fileId()) {
        CXCursor ref = clang_getCursorReferenced(cursor);

        VerboseVisitorUserData *u = reinterpret_cast<VerboseVisitorUserData*>(userData);
        if (u->indent >= 0)
            u->out += ByteArray(u->indent, ' ');
        u->out += RTags::cursorToString(cursor);
        if (clang_equalCursors(ref, cursor)) {
            u->out += " refs self";
        } else if (!clang_equalCursors(ref, nullCursor)) {
            u->out += " refs " + RTags::cursorToString(ref);
        }

        if (loc.fileId() && u->job->mPaths.value(loc.fileId()) == IndexerJob::Index) {
            if (u->job->mData->symbols.value(loc).kind == CursorInfo::ReferenceKind) {
                u->out += " used as reference\n";
            } else if (u->job->mData->symbols.contains(loc)) {
                u->out += " used as cursor\n";
            } else {
                u->out += " not used\n";
            }
        } else {
            u->out += " not indexed\n";
        }
    }
    if (u->indent >= 0) {
        u->indent += 2;
        clang_visitChildren(cursor, verboseVisitor, userData);
        u->indent -= 2;
        return CXChildVisit_Continue;
    } else {
        return CXChildVisit_Recurse;
    }
}

CXChildVisitResult IndexerJob::dumpVisitor(CXCursor cursor, CXCursor, CXClientData userData)
{
    DumpUserData *dump = reinterpret_cast<DumpUserData*>(userData);
    assert(dump);
    assert(dump->job);
    Location loc = dump->job->createLocation(cursor);
    if (loc.fileId()) {
        CXCursor ref = clang_getCursorReferenced(cursor);

        ByteArray out;
        out.reserve(256);
        if (dump->indent >= 0)
            out += ByteArray(dump->indent, ' ');
        out += RTags::cursorToString(cursor, RTags::AllCursorToStringFlags);
        if (clang_equalCursors(ref, cursor)) {
            out += " refs self";
        } else if (!clang_equalCursors(ref, nullCursor)) {
            out += " refs " + RTags::cursorToString(ref, RTags::AllCursorToStringFlags);
        }
        dump->job->write(out);
    }
    if (dump->indent >= 0) {
        DumpUserData userData = { dump->indent + 2, dump->job };
        clang_visitChildren(cursor, dumpVisitor, &userData);
        return CXChildVisit_Continue;
    } else {
        return CXChildVisit_Recurse;
    }
}
