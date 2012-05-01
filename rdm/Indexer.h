#ifndef INDEXER_H
#define INDEXER_H

#include <QtCore>
#include <AddMessage.h>
#include "Rdm.h"
#include "CursorInfo.h"

typedef QHash<Location, CursorInfo> SymbolHash;
typedef QHash<Location, QPair<Location, Rdm::ReferenceType> > ReferenceHash;
typedef QHash<QByteArray, QSet<Location> > SymbolNameHash;
typedef QHash<Path, QSet<Path> > DependencyHash;
typedef QPair<QByteArray, quint64> WatchedPair;
typedef QHash<QByteArray, Location> PchUSRHash;
typedef QHash<Path, QSet<WatchedPair> > WatchedHash;

struct FileInformation {
    FileInformation() : parseTime(0) {}
    quint64 parseTime;
    QList<QByteArray> compileArgs;
};
static inline QDataStream &operator<<(QDataStream &ds, const FileInformation &ci)
{
    ds << ci.parseTime << ci.compileArgs;
    return ds;
}

static inline QDataStream &operator>>(QDataStream &ds, FileInformation &ci)
{
    ds >> ci.parseTime >> ci.compileArgs;
    return ds;
}

static inline QDataStream &operator<<(QDataStream &ds, Rdm::ReferenceType type)
{
    ds << static_cast<quint8>(type);
    return ds;
}

static inline QDataStream &operator>>(QDataStream &ds, Rdm::ReferenceType &type)
{
    quint8 t;
    ds >> t;
    type = static_cast<Rdm::ReferenceType>(t);
    return ds;
}

typedef QHash<Path, FileInformation> InformationHash;

class IndexerJob;
class IndexerSyncer;
class SymbolNameSyncer;
class SymbolSyncer;
class Indexer : public QObject
{
    Q_OBJECT;
public:

    Indexer(const QByteArray& path, QObject* parent = 0);
    ~Indexer();

    int index(const QByteArray& input, const QList<QByteArray>& arguments);

    void setDefaultArgs(const QList<QByteArray> &args);
    inline QList<QByteArray> defaultArgs() const { return mDefaultArgs; }
    void setPchDependencies(const Path &pchHeader, const QSet<Path> &deps);
    QSet<Path> pchDependencies(const Path &pchHeader) const;
    QHash<QByteArray, Location> pchUSRHash(const QList<Path> &pchFiles) const;
    void setPchUSRHash(const Path &pch, const PchUSRHash &astHash);
    Path path() const { return mPath; }
    void abort();

    void deferData(const QByteArray &data)
    {
        QMutexLocker lock(&mMutex);
        if (!mTempFile) {
            mTempFile = new QTemporaryFile;
            mTempFile->open();
        }
        // qDebug() << "wrote" << data.size() << "to" << mTempFile->fileName();
        mTempFile->write(data);
    }
protected:
    void timerEvent(QTimerEvent *e);
signals:
    void indexingDone(int id);
    void jobsComplete();
private slots:
    void onJobComplete(int id, const Path& input, bool isPch, const QByteArray &msg);
    void onDirectoryChanged(const QString& path);
private:
    void processDeferredData(); // QMutex held
    void commitDependencies(const DependencyHash& deps, bool sync);
    void initWatcher();
    void init();
    bool needsToWaitForPch(IndexerJob *job) const;
    void startJob(int id, IndexerJob *job);

    QTemporaryFile *mTempFile;

    mutable QReadWriteLock mPchUSRHashLock;
    QHash<Path, PchUSRHash > mPchUSRHashes;

    QList<QByteArray> mDefaultArgs;
    mutable QReadWriteLock mPchDependenciesLock;
    QHash<Path, QSet<Path> > mPchDependencies;
    int mJobCounter;

    QMutex mMutex;
    QSet<QByteArray> mIndexing;

    QByteArray mPath;
    QHash<int, IndexerJob*> mJobs, mWaitingForPCH;

    bool mTimerRunning;
    QElapsedTimer mTimer;
    QBasicTimer mJobsDoneTimer;

    QFileSystemWatcher mWatcher;
    DependencyHash mDependencies;
    QMutex mWatchedMutex;
    WatchedHash mWatched;
};

#endif
