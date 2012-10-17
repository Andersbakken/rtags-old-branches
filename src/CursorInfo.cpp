#include "CursorInfo.h"
#include "RTagsClang.h"
#include "Project.h"

ByteArray CursorInfo::toString(unsigned keyFlags) const
{
    ByteArray ret(16384, '\0');
    char *buf = ret.data();
    int pos = snprintf(buf, ret.size(), "%s CursorInfo(%ssymbolLength: %u symbolName: %s usr: %s kind: %s%s",
                       location.key(keyFlags).constData(),
                       start != -1 && end != -1 ? ByteArray::snprintf<16>("%d-%d ", start, end).constData() : "",
                       symbolLength, symbolName.constData(), usr.constData(),
                       RTags::eatString(clang_getCursorKindSpelling(kind)).constData(),
                       isDefinition ? " definition" : "");
    buf += pos;

    if (pos < ret.size() && !targets.isEmpty()) {
        int w = snprintf(buf, ret.size() - pos, " targets:\n");
        pos += w;
        buf += w;
        for (Set<ByteArray>::const_iterator tit = targets.begin(); tit != targets.end() && w < ret.size(); ++tit) {
            std::shared_ptr<Project> proj = Project::currentProjectForThread();
            if (proj) {
                const Location l = proj->findLocation(*tit);
                if (!l.isNull()) {
                    w = snprintf(buf, ret.size() - pos, "    %s\n", l.key(keyFlags).constData());
                    buf += w;
                    pos += w;
                } else {
                    w = snprintf(buf, ret.size() - pos, "    invalid target %s \n", tit->constData());
                    buf += w;
                    pos += w;
                }
            } else {
                w = snprintf(buf, ret.size() - pos, "    %s\n", tit->constData());
                buf += w;
                pos += w;
            }
        }
    }

    if (pos < ret.size() && !references.isEmpty()) {
        int w = snprintf(buf, ret.size() - pos, " references:\n");
        pos += w;
        buf += w;
        for (Set<ByteArray>::const_iterator rit = references.begin(); rit != references.end() && w < ret.size(); ++rit) {
            std::shared_ptr<Project> proj = Project::currentProjectForThread();
            if (proj) {
                const Location l = proj->findLocation(*rit);
                if (!l.isNull()) {
                    w = snprintf(buf, ret.size() - pos, "    %s\n", l.key(keyFlags).constData());
                    buf += w;
                    pos += w;
                } else {
                    w = snprintf(buf, ret.size() - pos, "    invalid target %s \n", rit->constData());
                    buf += w;
                    pos += w;
                }
            } else {
                w = snprintf(buf, ret.size() - pos, "    %s\n", rit->constData());
                buf += w;
                pos += w;
            }
        }
    }
    ret.truncate(pos);
    return ret;
}

int CursorInfo::cursorRank(CXCursorKind kind)
{
    switch (kind) {
    case CXCursor_Constructor: // this one should be more than class/struct decl
        return 1;
    case CXCursor_ClassDecl:
    case CXCursor_StructDecl:
    case CXCursor_ClassTemplate:
        return 0;
    case CXCursor_MacroDefinition:
        return 3;
    default:
        return 2;
    }
}

CursorInfo CursorInfo::bestTarget(const SymbolMap &map) const
{
    const SymbolMap targets = targetInfos(map);

    SymbolMap::const_iterator best = targets.end();
    int bestRank = -1;
    for (SymbolMap::const_iterator it = targets.begin(); it != targets.end(); ++it) {
        const CursorInfo &ci = it->second;
        const int r = cursorRank(ci.kind);
        if (r > bestRank || (r == bestRank && ci.isDefinition)) {
            bestRank = r;
            best = it;
        }
    }
    if (best != targets.end()) {
        return best->second;
    }
    return CursorInfo();
}

SymbolMap CursorInfo::targetInfos(const SymbolMap &map) const
{
    SymbolMap ret;
    for (Set<ByteArray>::const_iterator it = targets.begin(); it != targets.end(); ++it) {
        const SymbolMap::const_iterator found = map.find(*it);
        if (found != map.end()) {
            ret[*it] = found->second;
        } else {
            ret[*it] = CursorInfo();
            // we need this one for inclusion directives which target a
            // non-existing CursorInfo
        }
    }
    return ret;
}

SymbolMap CursorInfo::referenceInfos(const SymbolMap &map) const
{
    SymbolMap ret;
    for (Set<ByteArray>::const_iterator it = references.begin(); it != references.end(); ++it) {
        const SymbolMap::const_iterator found = map.find(*it);
        if (found != map.end()) {
            ret[*it] = found->second;
        }
    }
    return ret;
}

SymbolMap CursorInfo::callers(const SymbolMap &map) const
{
    assert(!RTags::isReference(kind));
    SymbolMap ret;
    const SymbolMap cursors = virtuals(map);
    for (SymbolMap::const_iterator c = cursors.begin(); c != cursors.end(); ++c) {
        for (Set<ByteArray>::const_iterator it = c->second.references.begin(); it != c->second.references.end(); ++it) {
            const SymbolMap::const_iterator found = map.find(*it);
            if (found == map.end())
                continue;
            if (RTags::isReference(found->second.kind)) { // is this always right?
                ret[*it] = found->second;
            } else if (kind == CXCursor_Constructor && (found->second.kind == CXCursor_VarDecl || found->second.kind == CXCursor_FieldDecl)) {
                ret[*it] = found->second;
            }
        }
    }
    return ret;
}

enum Mode {
    ClassRefs,
    VirtualRefs,
    NormalRefs
};

static inline void allImpl(const SymbolMap &map, const CursorInfo &info, SymbolMap &out, Mode mode, CXCursorKind kind)
{
    if (!out.insert(info.usr, info))
        return;
    typedef SymbolMap (CursorInfo::*Function)(const SymbolMap &map) const;
    const SymbolMap targets = info.targetInfos(map);
    for (SymbolMap::const_iterator t = targets.begin(); t != targets.end(); ++t) {
        bool ok;
        switch (mode) {
        case VirtualRefs:
        case NormalRefs:
            ok = (t->second.kind == kind);
            break;
        case ClassRefs:
            ok = (t->second.isClass()
                  || t->second.kind == CXCursor_Destructor
                  || t->second.kind == CXCursor_Constructor);
            break;
        }
        if (ok)
            allImpl(map, t->second, out, mode, kind);
    }
    const SymbolMap refs = info.referenceInfos(map);
    for (SymbolMap::const_iterator r = refs.begin(); r != refs.end(); ++r) {
        switch (mode) {
        case NormalRefs:
            out[r->first] = r->second;
            break;
        case VirtualRefs:
            if (r->second.kind == kind) {
                allImpl(map, r->second, out, mode, kind);
            } else {
                out[r->first] = r->second;
            }
            break;
        case ClassRefs:
            if (info.isClass()) // for class/struct we want the references inserted directly regardless and also recursed
                out[r->first] = r->second;
            if (r->second.isClass()
                || r->second.kind == CXCursor_Destructor
                || r->second.kind == CXCursor_Constructor) { // if is a constructor/destructor/class reference we want to recurse it
                allImpl(map, r->second, out, mode, kind);
            }
        }
    }
}

SymbolMap CursorInfo::allReferences(const SymbolMap &map) const
{
    SymbolMap ret;
    Mode mode = NormalRefs;
    switch (kind) {
    case CXCursor_Constructor:
    case CXCursor_Destructor:
        mode = ClassRefs;
        break;
    case CXCursor_CXXMethod:
        mode = VirtualRefs;
        break;
    default:
        mode = isClass() ? ClassRefs : VirtualRefs;
        break;
    }

    allImpl(map, *this, ret, mode, kind);
    return ret;
}

SymbolMap CursorInfo::virtuals(const SymbolMap &map) const
{
    SymbolMap ret;
    ret[usr] = *this;
    const SymbolMap s = (kind == CXCursor_CXXMethod ? allReferences(map) : targetInfos(map));
    for (SymbolMap::const_iterator it = s.begin(); it != s.end(); ++it) {
        if (it->second.kind == kind)
            ret[it->first] = it->second;
    }
    return ret;
}

SymbolMap CursorInfo::declarationAndDefinition(const SymbolMap &map) const
{
    SymbolMap cursors;
    cursors[usr] = *this;

    const CursorInfo t = bestTarget(map);

    if (t.kind == kind)
        cursors[t.usr] = t;
    return cursors;
}
