#include "GRParseJob.h"
#include "GRParser.h"
#include "Project.h"

GRParseJob::GRParseJob(const Path &path, unsigned flags, const SharedPtr<Project> &project)
    : mPath(path), mFlags(flags), mParseTime(0), mProject(project)
{
}

void GRParseJob::run()
{
    Timer timer;
    GRParser parser;
    const char *extension = mPath.extension();
    const unsigned flags = extension && strcmp("c", extension) ? GRParser::CPlusPlus : GRParser::None;
    mParseTime = time(0);
    parser.parse(mPath, flags, mEntries);
    if (SharedPtr<Project> project = mProject.lock()) {
        SharedPtr<GRParseJob> job = std::static_pointer_cast<GRParseJob>(shared_from_this());
        mFinished(job, mEntries);
    }
}
