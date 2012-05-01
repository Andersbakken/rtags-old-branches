#ifndef IndexerSyncer_h
#define IndexerSyncer_h

#include "Indexer.h"
#include <QtCore>

class IndexerSyncer : public QThread
{
    Q_OBJECT
public:
    IndexerSyncer(QObject* parent = 0);
    void addDependencies(const DependencyHash& dependencies);
    void setPchDependencies(const DependencyHash& dependencies);
    void addFileInformation(const Path& input, const QList<QByteArray>& args, time_t timeStamp);
    void addFileInformations(const QSet<Path>& files);
    void addPchUSRHash(const Path &pchHeader, const PchUSRHash &hash);
    void notify();
    void stop();

protected:
    void run();
private:
    void maybeWake();

    bool mStopped;
    QMutex mMutex;
    QWaitCondition mCond;
    DependencyHash mDependencies, mPchDependencies;
    InformationHash mInformations;
    QHash<Path, PchUSRHash> mPchUSRHashes;
};

#endif
