#ifndef ValidateDBJob_h
#define ValidateDBJob_h

#include "Job.h"
#include "Set.h"
#include "Location.h"
#include "SignalSlot.h"

class ValidateDBJob : public Job
{
public:
    ValidateDBJob(const std::shared_ptr<Project> &proj, const Set<ByteArray> &prev);
    signalslot::Signal1<const Set<ByteArray> &> &errors() { return mErrors; }
protected:
    virtual void execute();
private:
    const Set<ByteArray> mPrevious;
    signalslot::Signal1<const Set<ByteArray> &> mErrors;

};

#endif
