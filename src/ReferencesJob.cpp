#include "Database.h"
#include "ReferencesJob.h"
#include "Server.h"
#include "RTags.h"
#include "CursorInfo.h"

ReferencesJob::ReferencesJob(int i, const Location &loc, unsigned fl)
    : Job(i, QueryJobPriority), symbolName(ByteArray()), flags(fl)
{
    locations.insert(loc);
}

ReferencesJob::ReferencesJob(int i, const ByteArray &sym, unsigned fl)
    : Job(i, QueryJobPriority), symbolName(sym), flags(fl)
{
}

struct LocationAndDefinitionNode
{
    LocationAndDefinitionNode(const Location &loc, bool def)
        : location(loc), isDefinition(def)
    {}
    LocationAndDefinitionNode() {}
    Location location;
    bool isDefinition;

    bool operator<(const LocationAndDefinitionNode &other) const
    {
        if (isDefinition != other.isDefinition)
            return isDefinition;
        return location < other.location;
    }
    bool operator>(const LocationAndDefinitionNode &other) const
    {
        if (isDefinition != other.isDefinition)
            return !isDefinition;
        return location > other.location;
    }

};

void ReferencesJob::execute()
{
    if (!symbolName.isEmpty()) {
        ScopedDB db = Server::instance()->db(Server::SymbolName, ReadWriteLock::Read);
        locations = db->value<Set<Location> >(symbolName);
        if (locations.isEmpty()) {
            return;
        }
    }
    const bool excludeDefsAndDecls = !(flags & QueryMessage::IncludeDeclarationsAndDefinitions);
    ScopedDB db = Server::instance()->db(Server::Symbol, ReadWriteLock::Read);
    const unsigned keyFlags = QueryMessage::keyFlags(flags);
    Map<Location, bool> refs;
    Set<Location> filtered;
    for (Set<Location>::const_iterator it = locations.begin(); it != locations.end(); ++it) {
        if (isAborted())
            return;

        const Location &location = *it;
        Location realLoc;
        CursorInfo cursorInfo = RTags::findCursorInfo(db, location, &realLoc);
        if (RTags::isReference(cursorInfo.kind)) {
            const Location target = cursorInfo.target;
            cursorInfo = RTags::findCursorInfo(db, cursorInfo.target);
            if (excludeDefsAndDecls) {
                filtered.insert(target);
            } else {
                refs[target] = cursorInfo.isDefinition;
            }
        } else {
            if (excludeDefsAndDecls) {
                filtered.insert(realLoc);
            } else {
                refs[realLoc] = cursorInfo.isDefinition;
            }
        }

        if (cursorInfo.symbolLength) {
            if (cursorInfo.target.isValid() && excludeDefsAndDecls) {
                filtered.insert(cursorInfo.target);
            }
            for (Set<Location>::const_iterator it = cursorInfo.references.begin(); it != cursorInfo.references.end(); ++it) {
                const Location &l = *it;
                if (!excludeDefsAndDecls || !filtered.contains(l)) {
                    refs[l] = false; // none of these should be definitions right?
                }
            }
            if (cursorInfo.target.isValid() && cursorInfo.kind != CXCursor_VarDecl) {
                const Location l = cursorInfo.target;
                cursorInfo = RTags::findCursorInfo(db, cursorInfo.target);
                refs[l] = cursorInfo.isDefinition;
                for (Set<Location>::const_iterator it = cursorInfo.references.begin(); it != cursorInfo.references.end(); ++it) {
                    const Location &l = *it;
                    if (!excludeDefsAndDecls || !filtered.contains(l)) {
                        refs[l] = false;
                    }
                }
            }
        }
    }
    List<LocationAndDefinitionNode> sorted(refs.size());
    int i = 0;
    for (Map<Location, bool>::const_iterator it = refs.begin(); it != refs.end(); ++it) {
        sorted[i++] = LocationAndDefinitionNode(it->first, it->second);
    }

    if (flags & QueryMessage::ReverseSort) {
        std::sort(sorted.begin(), sorted.end(), std::greater<LocationAndDefinitionNode>());
    } else {
        std::sort(sorted.begin(), sorted.end());
    }
    for (List<LocationAndDefinitionNode>::const_iterator it = sorted.begin(); it != sorted.end(); ++it) {
        write(it->location.key(keyFlags));
    }
}
