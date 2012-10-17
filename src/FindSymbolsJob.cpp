#include "FindSymbolsJob.h"
#include "Server.h"
#include "Log.h"
#include "RTagsClang.h"

static inline unsigned jobFlags(unsigned queryFlags)
{
    return (queryFlags & QueryMessage::ElispList) ? Job::QuoteOutput : Job::None;
}

FindSymbolsJob::FindSymbolsJob(const QueryMessage &query, const std::shared_ptr<Project> &proj)
    : Job(query, ::jobFlags(query.flags()), proj), string(query.query())
{
}

void FindSymbolsJob::execute()
{
    std::shared_ptr<Project> proj = project();
    Map<Location, bool> out;
    if (proj->indexer) {
        Scope<const SymbolNameMap&> scope = proj->lockSymbolNamesForRead();
        if (scope.isNull())
            return;
        const SymbolNameMap &map = scope.data();
        const SymbolNameMap::const_iterator it = map.find(string);
        if (it != map.end()) {
            const Set<Location> &locations = it->second;
            for (Set<Location>::const_iterator i = locations.begin(); i != locations.end(); ++i) {
                out[*i] = true;
            }
        }
    }
    if (proj->grtags) {
        Scope<const GRMap &> scope = proj->lockGRForRead();
        const GRMap &map = scope.data();
        GRMap::const_iterator it = map.find(string);
        if (it != map.end()) {
            const Map<Location, bool> &locations = it->second;
            for (Map<Location, bool>::const_iterator i = locations.begin(); i != locations.end(); ++i) {
                if (!i->second)
                    out[i->first] = false;
            }
        }
    }

    if (out.size()) {
        Scope<const SymbolMap&> scope = proj->lockSymbolsForRead();
        List<RTags::SortedCursor> sorted;
        sorted.reserve(out.size());
        for (Map<Location, bool>::const_iterator it = out.begin(); it != out.end(); ++it) {
            RTags::SortedCursor node(it->first);
            if (it->second && proj->indexer) {
                const CursorInfo info = proj->findCursorInfo(it->first);
                if (!info.isNull()) {
                    node.isDefinition = info.isDefinition;
                    node.kind = info.kind;
                }
            }
            sorted.push_back(node);
        }

        if (queryFlags() & QueryMessage::ReverseSort) {
            std::sort(sorted.begin(), sorted.end(), std::greater<RTags::SortedCursor>());
        } else {
            std::sort(sorted.begin(), sorted.end());
        }
        const uint32_t keyFlags = QueryMessage::keyFlags(queryFlags());
        const int count = sorted.size();
        for (int i=0; i<count; ++i) {
            write(sorted.at(i).location.key(keyFlags));
        }
    }
}
