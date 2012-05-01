#include "IndexerSyncer.h"
#include "leveldb/db.h"
#include "Server.h"

IndexerSyncer::IndexerSyncer(QObject* parent)
    : QThread(parent), mStopped(false)
{
}

void IndexerSyncer::stop()
{
    QMutexLocker locker(&mMutex);
    mStopped = true;
    mCond.wakeOne();
}

void IndexerSyncer::notify()
{
    QMutexLocker locker(&mMutex); // is this needed here?
    mCond.wakeOne();
}

void IndexerSyncer::addDependencies(const DependencyHash& dependencies)
{
    QMutexLocker lock(&mMutex);
    if (mDependencies.isEmpty()) {
        mDependencies = dependencies;
    } else {
        const DependencyHash::const_iterator end = dependencies.end();
        for (DependencyHash::const_iterator it = dependencies.begin(); it != end; ++it) {
            mDependencies[it.key()].unite(it.value());
        }
    }
    maybeWake();
}

void IndexerSyncer::setPchDependencies(const DependencyHash& dependencies)
{
    QMutexLocker lock(&mMutex);
    if (mPchDependencies.isEmpty()) {
        mPchDependencies = dependencies;
    } else {
        const DependencyHash::const_iterator end = dependencies.end();
        for (DependencyHash::const_iterator it = dependencies.begin(); it != end; ++it) {
            mPchDependencies[it.key()].unite(it.value());
        }
    }
    maybeWake();
}

void IndexerSyncer::addPchUSRHash(const Path &pchHeader, const PchUSRHash &hash)
{
    QMutexLocker lock(&mMutex);
    mPchUSRHashes[pchHeader] = hash;
    maybeWake();
}

void IndexerSyncer::addFileInformation(const Path& input, const QList<QByteArray>& args, time_t timeStamp)
{
    FileInformation fi;
    fi.lastTouched = timeStamp;
    fi.compileArgs = args;
    QMutexLocker lock(&mMutex);
    mInformations[input] = fi;
    maybeWake();
}

void IndexerSyncer::addFileInformations(const QSet<Path>& files)
{
    QMutexLocker lock(&mMutex);
    foreach (const Path &path, files) {
        FileInformation &fi = mInformations[path]; // force creation
        (void)fi;
    }
    maybeWake();
}


void IndexerSyncer::run()
{
    while (true) {
        DependencyHash dependencies, pchDependencies;
        InformationHash informations;
        ReferenceHash references;
        QHash<Path, PchUSRHash> pchUSRHashes;
        {
            QMutexLocker locker(&mMutex);
            if (mStopped)
                return;
            while (mDependencies.isEmpty()
                   && mInformations.isEmpty()
                   && mPchDependencies.isEmpty()
                   && mPchUSRHashes.isEmpty()) {
                mCond.wait(&mMutex, 10000);
                if (mStopped)
                    return;
            }
            qSwap(dependencies, mDependencies);
            qSwap(pchDependencies, mPchDependencies);
            qSwap(pchUSRHashes, mPchUSRHashes);
            qSwap(informations, mInformations);
        }
        warning() << "IndexerSyncer::run woke up dependencies" << dependencies.size()
                  << "informations" << informations.size()
                  << "pchDependencies" << pchDependencies.size()
                  << "pchUSRHashes" << pchUSRHashes.size();
        QList<QByteArray> out;

        if (!dependencies.isEmpty()) {
            QElapsedTimer timer;
            timer.start();
            leveldb::DB *db = Server::instance()->db(Server::Dependency);
            Rdm::Batch batch(db);

            DependencyHash::iterator it = dependencies.begin();
            const DependencyHash::const_iterator end = dependencies.end();
            while (it != end) {
                const char* key = it.key().constData();
                QSet<Path> added = it.value();
                QSet<Path> current = Rdm::readValue<QSet<Path> >(db, key);
                const int oldSize = current.size();
                if (current.unite(added).size() > oldSize) {
                    batch.add(key, current);
                }
                ++it;
            }
            batch.write();
            if (batch.totalWritten) {
                out += QByteArray("Wrote " + QByteArray::number(dependencies.size())
                                  + " dependencies, " + QByteArray::number(batch.totalWritten) + " bytes in "
                                  + QByteArray::number(timer.elapsed()) + "ms");
            }
        }
        if (!pchDependencies.isEmpty() || !pchUSRHashes.isEmpty()) {
            QElapsedTimer timer;
            timer.start();
            leveldb::DB *db = Server::instance()->db(Server::PCH);
            Rdm::Batch batch(db);
            if (!pchDependencies.isEmpty())
                batch.add("dependencies", pchDependencies);

            for (QHash<Path, PchUSRHash>::const_iterator it = pchUSRHashes.begin(); it != pchUSRHashes.end(); ++it) {
                batch.add(it.key(), it.value());
            }
            batch.write();
            out += ("Wrote " + QByteArray::number(pchDependencies.size() + pchUSRHashes.size()) + " pch infos, "
                    + QByteArray::number(batch.totalWritten) + " bytes in "
                    + QByteArray::number(timer.elapsed()) + "ms");
        }
        if (!informations.isEmpty()) {
            QElapsedTimer timer;
            timer.start();
            leveldb::DB *db = Server::instance()->db(Server::FileInformation);
            Rdm::Batch batch(db);

            InformationHash::iterator it = informations.begin();
            const InformationHash::const_iterator end = informations.end();
            while (it != end) {
                const char *key = it.key().constData();
                batch.add(key, it.key());
                ++it;
            }

            out += ("Wrote " + QByteArray::number(informations.size()) + " fileinfos, "
                    + QByteArray::number(batch.totalWritten) + " bytes in "
                    + QByteArray::number(timer.elapsed()) + "ms");
        }
        if (!out.isEmpty())
            error() << RTags::join(out, ", ").constData();
    }
}

void IndexerSyncer::maybeWake()
{
    const int size = (mDependencies.size() + mPchDependencies.size()
                      + mInformations.size() + mPchUSRHashes.size());
    enum { MaxSize = 1024 * 64 };
    if (size > MaxSize) // ### tunable?
        mCond.wakeOne();
}
