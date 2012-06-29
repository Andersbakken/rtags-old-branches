#include "EventLoop.h"
#include "Event.h"
#include "EventReceiver.h"
#include "MutexLocker.h"
#include "Rdm.h"
#include <unistd.h>
#include <errno.h>
#include <sys/select.h>
#include <fcntl.h>
#include <algorithm>

EventLoop* EventLoop::sInstance = 0;

EventLoop::EventLoop()
    : mQuit(false), mThread(0)
{
    if (!sInstance)
        sInstance = this;
    int err;
    eintrwrap(err, ::pipe2(mEventPipe, O_NONBLOCK));
}

EventLoop::~EventLoop()
{
    int err;
    eintrwrap(err, ::close(mEventPipe[0]));
    eintrwrap(err, ::close(mEventPipe[1]));
}

EventLoop* EventLoop::instance()
{
    return sInstance;
}

void EventLoop::addFileDescriptor(int fd, unsigned int flags, FdFunc callback, void* userData)
{
    MutexLocker locker(&mMutex);
    FdData &data = mFdData[fd];
    data.flags = flags;
    data.callback = callback;
    data.userData = userData;

    locker.unlock();

    if (!pthread_equal(pthread_self(), mThread)) {
        const char c = 'a';
        int r;
        do {
            eintrwrap(r, ::write(mEventPipe[1], &c, 1));
        } while (r == -1 && errno == EAGAIN);
    }
}

void EventLoop::removeFileDescriptor(int fd)
{
    MutexLocker locker(&mMutex);

    if (mFdData.remove(fd) && !pthread_equal(pthread_self(), mThread)) {
        const char c = 'r';
        int r;
        do {
            eintrwrap(r, ::write(mEventPipe[1], &c, 1));
        } while (r == -1 && errno == EAGAIN);
        mCond.wait(&mMutex);
    }
}

void EventLoop::postEvent(EventReceiver* receiver, Event* event)
{
    {
        EventData data = { receiver, event };

        MutexLocker locker(&mMutex);
        mEvents.push_back(data);
    }
    const char c = 'e';
    int r;
    do {
        eintrwrap(r, ::write(mEventPipe[1], &c, 1));
    } while (r == -1 && errno == EAGAIN);
}

void EventLoop::run()
{
    mThread = pthread_self();
    fd_set rset, wset;
    int max;
    for (;;) {
        FD_ZERO(&rset);
        FD_ZERO(&wset);
        FD_SET(mEventPipe[0], &rset);
        max = mEventPipe[0];
        {
            MutexLocker locker(&mMutex);
            for (Map<int, FdData>::const_iterator it = mFdData.begin();
                 it != mFdData.end(); ++it) {
                if (it->second.flags & Read)
                    FD_SET(it->first, &rset);
                if (it->second.flags & Write)
                    FD_SET(it->first, &wset);
                max = std::max(max, it->first);
            }
        }
        int r;
        // ### use poll instead? easier to catch exactly what fd that was problematic in the EBADF case
        eintrwrap(r, ::select(max + 1, &rset, &wset, 0, 0));
        if (r == -1) { // ow
            return;
        }
        if (FD_ISSET(mEventPipe[0], &rset))
            handlePipe();
        Map<int, FdData> fds;
        {
            MutexLocker locker(&mMutex);
            fds = mFdData;
        }

        Map<int, FdData>::const_iterator it = fds.begin();
        while (it != fds.end()) {
            if ((it->second.flags & Read) && FD_ISSET(it->first, &rset)) {
                unsigned int flag = Read;
                if ((it->second.flags & Write) && FD_ISSET(it->first, &wset))
                    flag |= Write;
                it->second.callback(it->first, flag, it->second.userData);
            } else if ((it->second.flags & Write) && FD_ISSET(it->first, &wset)) {
                it->second.callback(it->first, Write, it->second.userData);
            } else {
                continue;
            }
            MutexLocker lock(&mMutex);
            do {
                ++it;
            } while (it != fds.end() && !mFdData.contains(it->first));
        }
        if (mQuit)
            break;
    }
}

void EventLoop::handlePipe()
{
    char c;
    for (;;) {
        int r;
        eintrwrap(r, ::read(mEventPipe[0], &c, 1));
        if (r == 1) {
            switch (c) {
            case 'e':
                sendPostedEvents();
                break;
            case 'q':
                mQuit = true;
                break;
            case 'a':
                break;
            case 'r': {
                MutexLocker locker(&mMutex);
                mCond.wakeAll();
                break; }
            }
        } else
            break;
    }
}

void EventLoop::sendPostedEvents()
{
    MutexLocker locker(&mMutex);
    while (!mEvents.empty()) {
        std::vector<EventData>::iterator first = mEvents.begin();
        EventData data = *first;
        mEvents.erase(first);
        locker.unlock();
        data.receiver->event(data.event);
        delete data.event;
        locker.relock();
    }
}

void EventLoop::exit()
{
    const char q = 'q';
    int r;
    do {
        eintrwrap(r, ::write(mEventPipe[1], &q, 1));
    } while (r == -1 && errno == EAGAIN);
}
