#ifndef INDEXER_H
#define INDEXER_H

#include "CursorInfo.h"
#include "FileSystemWatcher.h"
#include "MutexLocker.h"
#include "RTags.h"
#include "ReadWriteLock.h"
#include "ThreadPool.h"
#include "Timer.h"
#include "Project.h"
#include "SharedPtr.h"
#include <clang-c/Index.h>

struct IndexData;
class IndexerJob;
class Indexer : public std::enable_shared_from_this<Indexer>
{
public:
    Indexer(const SharedPtr<Project> &project, bool validate);

    void index(const SourceInformation &args, unsigned indexerJobFlags);
    SourceInformation sourceInfo(uint32_t fileId) const;
    Set<uint32_t> dependencies(uint32_t fileId) const;
    bool visitFile(uint32_t fileId, const SharedPtr<IndexerJob> &job);
    ByteArray fixIts(const Path &path) const;
    ByteArray errors(const Path &path = Path()) const;
    int reindex(const ByteArray &pattern, bool regexp);
    signalslot::Signal2<SharedPtr<Indexer>, int> &jobsComplete() { return mJobsComplete; }
    signalslot::Signal2<SharedPtr<Indexer>, Path> &jobStarted() { return mJobStarted; }
    SharedPtr<Project> project() const { return mProject.lock(); }
    void beginMakefile();
    void endMakefile();
    void onJobFinished(const SharedPtr<IndexerJob> &job);
    bool isIndexed(uint32_t fileId) const;
    SourceInformationMap sources() const;
    DependencyMap dependencies() const;
    bool save(Serializer &out);
    bool restore(Deserializer &in);
    Set<Path> watchedPaths() const { return mWatchedPaths; }
private:
    void checkFinished();
    void onFileModified(const Path &);
    void onFileRemoved(const Path &);
    void addDependencies(const DependencyMap &hash, Set<uint32_t> &newFiles);
    void addDiagnostics(const DiagnosticsMap &errors, const FixitMap &fixIts);
    void write();
    void onFilesModifiedTimeout();
    static void onFilesModifiedTimeout(int id, void *userData)
    {
        EventLoop::instance()->removeTimer(id);
        static_cast<Indexer*>(userData)->onFilesModifiedTimeout();
    }
    void onValidateDBJobErrors(const Set<Location> &errors);

    enum InitMode {
        Normal,
        NoValidate,
        ForceDirty
    };

    Map<SharedPtr<IndexerJob>, Set<uint32_t> > mVisitedFilesByJob;
    Set<uint32_t> mVisitedFiles;

    int mJobCounter;
    bool mInMakefile;

    mutable Mutex mMutex;

    ByteArray mPath;
    Map<uint32_t, SharedPtr<IndexerJob> > mJobs;

    Set<uint32_t> mModifiedFiles;
    int mModifiedFilesTimerId;

    bool mTimerRunning;
    Timer mTimer;

    WeakPtr<Project> mProject;
    FileSystemWatcher mWatcher;
    DependencyMap mDependencies;
    SourceInformationMap mSources;

    Set<Path> mWatchedPaths;

    Map<Location, std::pair<int, ByteArray> > mFixIts;
    Map<uint32_t, ByteArray> mErrors;

    Set<Location> mPreviousErrors;

    signalslot::Signal2<SharedPtr<Indexer>, int> mJobsComplete;
    signalslot::Signal2<SharedPtr<Indexer>, Path> mJobStarted;
    bool mValidate;

    Map<uint32_t, SharedPtr<IndexData> > mPendingData;
    Set<uint32_t> mPendingDirtyFiles;
};

inline bool Indexer::visitFile(uint32_t fileId, const SharedPtr<IndexerJob> &job)
{
    MutexLocker lock(&mMutex);
    if (mVisitedFiles.contains(fileId)) {
        return false;
    }

    mVisitedFiles.insert(fileId);
    mVisitedFilesByJob[job].insert(fileId);
    return true;
}

#endif
