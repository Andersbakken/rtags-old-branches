#include "FollowLocationJob.h"
#include "RTags.h"
#include "Server.h"
#include "CursorInfo.h"

FollowLocationJob::FollowLocationJob(const Location &loc, const QueryMessage &query, const shared_ptr<Project> &project)
    : Job(query, 0, project), location(loc)
{
}

void FollowLocationJob::run()
{
    Scope<const SymbolMap&> scope = project()->lockSymbolsForRead();
    const SymbolMap &map = scope.data();
    const SymbolMap::const_iterator it = RTags::findCursorInfo(map, location);
    if (it == map.end())
        return;

    const CursorInfo &cursorInfo = it->second;
    if (cursorInfo.target.isNull())
        return;

    Location out = cursorInfo.target;
    if (cursorInfo.kind == CursorInfo::ReferenceKind) {
        SymbolMap::const_iterator target = RTags::findCursorInfo(map, cursorInfo.target);
        if (target != map.end() && !target->second.isDefinition) {
            switch (target->second.kind) {
            case CXIdxEntity_Function:
            case CXIdxEntity_CXXInstanceMethod:
            case CXIdxEntity_CXXDestructor:
            case CXIdxEntity_CXXConstructor:
                target = RTags::findCursorInfo(map, target->second.target);
                if (target != map.end())
                    out = target->first;
                break;
            default:
                break;
            }
        }
    }
    assert(!out.isNull());
    write(out.key(keyFlags()));
}
