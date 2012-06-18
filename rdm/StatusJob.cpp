#include "StatusJob.h"
#include "Database.h"
#include "Server.h"
#include "RTags.h"
#include "Indexer.h"
#include <clang-c/Index.h>
#include <Rdm.h>
#include "CursorInfo.h"

const char *StatusJob::delimiter = "*********************************";
StatusJob::StatusJob(int i, const QByteArray &q)
    : Job(i, QueryJobPriority, WriteUnfiltered), query(q)
{
}

void StatusJob::execute()
{
    if (query.isEmpty() || query == "general") {
        ScopedDB db = Server::instance()->db(Server::General, ScopedDB::Read);
        write(delimiter);
        write(Server::databaseDir(Server::General));
        write("    version: " + QByteArray::number(db->value<int>("version")));

        const QHash<Path, QPair<QList<QByteArray>, QList<QByteArray> > > makefiles
            = db->value<QHash<Path, QPair<QList<QByteArray>, QList<QByteArray> > > >("makefiles");

        for (QHash<Path, QPair<QList<QByteArray>, QList<QByteArray> > >::const_iterator it = makefiles.begin();
             it != makefiles.end(); ++it) {
            QByteArray out = "    " + it.key();
            if (!it.value().first.isEmpty())
                out += " args: " + RTags::join(it.value().first, " ");
            if (!it.value().second.isEmpty())
                out += " extra flags: " + RTags::join(it.value().second, " ");
            write(out);
        }
    }

    if (query.isEmpty() || query == "dependencies") {
        ScopedDB db = Server::instance()->db(Server::Dependency, ScopedDB::Read);
        write(delimiter);
        write(Server::databaseDir(Server::Dependency));
        RTags::Ptr<Iterator> it(db->createIterator());
        it->seekToFirst();
        char buf[1024];
        while (it->isValid()) {
            if (isAborted())
                return;
            const quint32 key = *reinterpret_cast<const quint32*>(it->key().data());
            snprintf(buf, sizeof(buf), "  %s (%d) is depended on by", Location::path(key).constData(), key);
            write(buf);
            const QSet<quint32> deps = it->value<QSet<quint32> >();
            foreach (quint32 p, deps) {
                snprintf(buf, sizeof(buf), "    %s (%d)", Location::path(p).constData(), p);
                write(buf);
            }
            it->next();
        }
    }

    if (query.isEmpty() || query == "symbols") {
        ScopedDB db = Server::instance()->db(Server::Symbol, ScopedDB::Read);
        write(delimiter);
        write(Server::databaseDir(Server::Symbol));
        RTags::Ptr<Iterator> it(db->createIterator());
        it->seekToFirst();
        char buf[1024];
        while (it->isValid()) {
            if (isAborted())
                return;
            const CursorInfo ci = it->value<CursorInfo>();
            CXString kind = clang_getCursorKindSpelling(ci.kind);
            Location loc = Location::fromKey(it->key().data());
            snprintf(buf, sizeof(buf),
                     "  %s symbolName: %s kind: %s isDefinition: %s symbolLength: %d target: %s parent: %s%s",
                     loc.key().constData(), ci.symbolName.constData(),
                     clang_getCString(kind), ci.isDefinition ? "true" : "false", ci.symbolLength,
                     ci.target.key().constData(), ci.parent.key().constData(),
                     ci.references.isEmpty() ? "" : " references:");
            clang_disposeString(kind);
            write(buf);
            foreach(const Location &loc, ci.references) {
                snprintf(buf, sizeof(buf), "    %s", loc.key().constData());
                write(buf);
            }
            it->next();
        }
    }

    if (query.isEmpty() || query == "symbolnames") {
        ScopedDB db = Server::instance()->db(Server::SymbolName, ScopedDB::Read);
        write(delimiter);
        write(Server::databaseDir(Server::SymbolName));
        RTags::Ptr<Iterator> it(db->createIterator());
        it->seekToFirst();
        char buf[1024];
        while (it->isValid()) {
            if (isAborted())
                return;
            snprintf(buf, sizeof(buf), "  %s:", it->key().byteArray().constData());
            write(buf);
            const QSet<Location> locations = it->value<QSet<Location> >();
            foreach (const Location &loc, locations) {
                snprintf(buf, sizeof(buf), "    %s", loc.key().constData());
                write(buf);
            }
            it->next();
        }
    }

    if (query.isEmpty() || query == "fileinfos") {
        ScopedDB db = Server::instance()->db(Server::FileInformation, ScopedDB::Read);
        write(delimiter);
        write(Server::databaseDir(Server::FileInformation));
        RTags::Ptr<Iterator> it(db->createIterator());
        it->seekToFirst();
        char buf[1024];
        while (it->isValid()) {
            if (isAborted())
                return;

            const FileInformation fi = it->value<FileInformation>();
            const quint32 fileId = *reinterpret_cast<const quint32*>(it->key().data());
            snprintf(buf, 1024, "  %s: last compiled: %s compile args: %s",
                     Location::path(fileId).constData(),
                     QDateTime::fromTime_t(fi.lastTouched).toString().toLocal8Bit().constData(),
                     RTags::join(fi.compileArgs, " ").constData());
            write(buf);
            it->next();
        }
    }

    if (query.isEmpty() || query == "pch") {
        ScopedDB db = Server::instance()->db(Server::FileIds, ScopedDB::Read);
        RTags::Ptr<Iterator> it(db->createIterator());
        it->seekToFirst();
        char buf[1024];
        while (it->isValid()) {
            if (isAborted())
                return;

            const PchUSRHash hash = it->value<PchUSRHash>();
            write(it->key().byteArray());
            snprintf(buf, 1024, "  %s", it->key().byteArray().constData());
            write(buf);
            for (PchUSRHash::const_iterator i = hash.begin(); i != hash.end(); ++i) {
                snprintf(buf, 1024, "    %s: %s", i.key().constData(), i.value().key().constData());
                write(buf);
            }

            it->next();
        }
    }

    if (query.isEmpty() || query == "fileids") {
        ScopedDB db = Server::instance()->db(Server::FileIds, ScopedDB::Read);
        write(delimiter);
        write(Server::databaseDir(Server::FileIds));
        RTags::Ptr<Iterator> it(db->createIterator());
        it->seekToFirst();
        char buf[1024];
        while (it->isValid()) {
            snprintf(buf, 1024, "  %s: %d", it->key().byteArray().constData(), it->value<quint32>());
            write(buf);
            it->next();
        }
    }
    if (query.isEmpty() || query == "visitedFiles") {
        write(delimiter);
        write("visitedFiles");
        char buf[1024];
        foreach(quint32 id, Server::instance()->indexer()->visitedFiles()) {
            snprintf(buf, sizeof(buf), "  %s: %d", Location::path(id).constData(), id);
            write(buf);
        }
    }
}
