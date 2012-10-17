#include "FollowLocationJob.h"
#include "RTags.h"
#include "Server.h"
#include "CursorInfo.h"

FollowLocationJob::FollowLocationJob(const Location &loc, const QueryMessage &query, const std::shared_ptr<Project> &project)
    : Job(query, 0, project), location(loc)
{
}

void FollowLocationJob::execute()
{
    std::shared_ptr<Project> proj = project();
    if (!proj)
        return;
    const CursorInfo cursorInfo = proj->findCursorInfo(location);
    if (cursorInfo.isEmpty() || (cursorInfo.isClass() && cursorInfo.isDefinition))
        return;

    Scope<const SymbolMap&> scope = project()->lockSymbolsForRead();
    const SymbolMap &map = scope.data();
    if (scope.isNull())
        return;

    CursorInfo target = cursorInfo.bestTarget(map);
    if (!target.isNull()) {
        if (cursorInfo.kind != target.kind) {
            if (!target.isDefinition && !target.targets.isEmpty()) {
                switch (target.kind) {
                case CXCursor_ClassDecl:
                case CXCursor_ClassTemplate:
                case CXCursor_StructDecl:
                case CXCursor_FunctionDecl:
                case CXCursor_CXXMethod:
                case CXCursor_Destructor:
                case CXCursor_Constructor:
                    target = target.bestTarget(map);
                    break;
                default:
                    break;
                }
            }
        }
        if (!target.isNull()) {
            write(target.location.key(keyFlags()));
        }
    }
}
