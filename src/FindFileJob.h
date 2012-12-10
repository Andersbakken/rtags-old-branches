#ifndef FindFileJob_h
#define FindFileJob_h

#include "ByteArray.h"
#include "List.h"
#include "RTagsClang.h"
#include "Job.h"
#include "Location.h"
#include "RegExp.h"

class FindFileJob : public Job
{
public:
    FindFileJob(Connection *connection, const QueryMessage &query, const shared_ptr<Project> &project);
    virtual void execute();
private:
    ByteArray mPattern;
    RegExp mRegExp;
};

#endif
