#include "CursorInfoJob.h"
#include "RTags.h"
#include "Server.h"
#include "CursorInfo.h"

CursorInfoJob::CursorInfoJob(const Location &loc, const QueryMessage &query, const std::shared_ptr<Project> &proj)
    : Job(query, 0, proj), location(loc)
{
}

void CursorInfoJob::execute()
{
    CursorInfo cursorInfo = project()->findCursorInfo(location);
    if (!cursorInfo.isEmpty())
        write(cursorInfo);
}
