#ifndef THREADPOOL_H
#define THREADPOOL_H

#include "Mutex.h"
#include "WaitCondition.h"
#include <deque>

class ThreadPoolThread;

class ThreadPoolJob;
class ThreadPool
{
public:
    ThreadPool(int concurrentJobs);
    ~ThreadPool();

    void setConcurrentJobs(int concurrentJobs);
    void clearBackLog();

    enum { Guaranteed = -1 };

    void start(const shared_ptr<ThreadPoolJob> &job, int priority = 0);

    static int idealThreadCount();
    static ThreadPool* globalInstance();

private:
    static bool jobLessThan(const shared_ptr<ThreadPoolJob> &l, const shared_ptr<ThreadPoolJob> &r);

private:
    int mConcurrentJobs;
    Mutex mMutex;
    WaitCondition mCond;
    std::deque<shared_ptr<ThreadPoolJob> > mJobs;
    List<ThreadPoolThread*> mThreads;
    int mBusyThreads;

    static ThreadPool* sGlobalInstance;

    friend class ThreadPoolThread;
};

class ThreadPoolJob
{
public:
    ThreadPoolJob();
    virtual ~ThreadPoolJob();

protected:
    virtual void run() {}
private:
    int mPriority;
    Mutex mMutex;

    friend class ThreadPool;
    friend class ThreadPoolThread;
};



#endif
