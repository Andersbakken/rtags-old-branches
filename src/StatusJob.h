#ifndef StatusJob_h
#define StatusJob_h

#include "ByteArray.h"
#include "List.h"
#include "Job.h"

class QueryMessage;
class StatusJob : public Job
{
public:
    StatusJob(Connection *connection, const QueryMessage &query, const shared_ptr<Project> &project);
    virtual void execute();
private:
    const ByteArray query;
};

#endif
