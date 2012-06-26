#ifndef MAKEFILEPARSER_H
#define MAKEFILEPARSER_H

#include "Path.h"
#include "GccArguments.h"
#include <QObject>
#include <List.h>
#include <Map.h>

class DirectoryTracker;
class Connection;
class Process;

class MakefileParser : public QObject
{
    Q_OBJECT
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

signals:
    void done(int sources, int pchs);
    void fileReady(const GccArguments &args);

private:
    void processMakeOutput();
    void processMakeLine(const ByteArray &line);
    void onDone();

private:
    Process *mProc;
    ByteArray mData;
    DirectoryTracker *mTracker;
    const List<ByteArray> mExtraFlags;
    Map<ByteArray, ByteArray> mPchs;
    int mSourceCount, mPchCount;
    Path mMakefile;
    Connection *mConnection;
};

#endif // MAKEFILEPARSER_H
