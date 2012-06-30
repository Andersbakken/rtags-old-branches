#ifndef FileSystemWatcher_h
#define FileSystemWatcher_h

#include <Path.h>
#include <Map.h>
#include <Mutex.h>
#include <signalslot.h>

class FileSystemWatcher
{
public:
    FileSystemWatcher();
    ~FileSystemWatcher();

    bool watch(const Path &path);
    bool unwatch(const Path &path);
    signalslot::Signal1<const Path&> &removed() { return mRemoved; }
    signalslot::Signal1<const Path&> &modified() { return mModified; }
private:
    signalslot::Signal1<const Path&> mRemoved, mModified;
#ifdef OS_Linux
    Mutex mMutex;
    int mInotifyFd;
    Map<Path, int> mWatchedByPath;
    Map<int, Path> mWatchedById;
    void inotifyReadyRead();
    static void iNotifyCallback(int, unsigned int, void *user) { reinterpret_cast<FileSystemWatcher*>(user)->inotifyReadyRead(); }
#else
#warning "FileSystemWatcher not implemented on this platform"
#endif
};
#endif
