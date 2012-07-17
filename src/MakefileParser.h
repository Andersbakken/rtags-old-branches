#ifndef MAKEFILEPARSER_H
#define MAKEFILEPARSER_H

#include "Path.h"
#include "GccArguments.h"
#include <List.h>
#include <Map.h>
#include <signalslot.h>

class DirectoryTracker;
class Connection;
class Process;

class MakefileParser
{
public:
    MakefileParser(const List<ByteArray> &extraFlags, Connection *conn);
    ~MakefileParser();

    void run(const Path &makefile, const List<ByteArray> &args);
    bool isDone() const;
    List<ByteArray> extraFlags() const { return mExtraFlags; }
    List<ByteArray> mapPchToInput(const List<ByteArray> &input) const;
    void setPch(const ByteArray &output, const ByteArray &input);
    Path makefile() const { return mMakefile; }
    Connection *connection() const { return mConnection; }
    signalslot::Signal1<MakefileParser*> &done() { return mDone; }

    int sourceCount() const { return mSourceFiles.size(); }
    int pchCount() const { return mPchFiles.size(); }
    Map<Path, List<ByteArray> > &pendingFiles() { return mPendingFiles; }
    bool hasProject() const { return mHasProject; }
    void setHasProject(bool hasProject) { mHasProject = hasProject; }
    const Map<Path, GccArguments> &pchFiles() { return mPchFiles; }
    const Map<Path, GccArguments> &sourceFiles() { return mPchFiles; }
private:
    void processMakeOutput();
    void processMakeError();
    void processMakeLine(const ByteArray &line);
    void onDone();

    Process *mProc;
    ByteArray mData;
    DirectoryTracker *mTracker;
    const List<ByteArray> mExtraFlags;
    Map<ByteArray, ByteArray> mPchs;
    Path mMakefile;
    Connection *mConnection;
    signalslot::Signal1<MakefileParser*> mDone;
    bool mHasProject;
    Map<Path, List<ByteArray> > mPendingFiles;
    Map<Path, GccArguments> mSourceFiles, mPchFiles;
};

#endif // MAKEFILEPARSER_H
