#include "SymbolNameSyncer.h"
#include "leveldb/db.h"
#include "Server.h"

SymbolNameSyncer::SymbolNameSyncer(QObject* parent)
    : QThread(parent), mStopped(false)
{
}

void SymbolNameSyncer::stop()
{
    QMutexLocker locker(&mMutex);
    mStopped = true;
    mCond.wakeOne();
}

void SymbolNameSyncer::notify()
{
    QMutexLocker locker(&mMutex); // is this needed here?
    mCond.wakeOne();
}

void SymbolNameSyncer::addSymbolNames(const SymbolNameHash &locations)
{
    QMutexLocker lock(&mMutex);
    if (mSymbolNames.isEmpty()) {
        mSymbolNames = locations;
    } else {
        const SymbolNameHash::const_iterator end = locations.end();
        for (SymbolNameHash::const_iterator it = locations.begin(); it != end; ++it) {
            mSymbolNames[it.key()].unite(it.value());
        }
    }
    maybeWake();
}

void SymbolNameSyncer::run()
{
    bool wroteSymbolNames = false;
    while (true) {
        SymbolNameHash symbolNames;
        SymbolHash symbols;
        DependencyHash dependencies, pchDependencies;
        InformationHash informations;
        ReferenceHash references;
        QHash<Path, PchUSRHash> pchUSRHashes;
        {
            QMutexLocker locker(&mMutex);
            if (mStopped)
                return;
            if (wroteSymbolNames && mSymbolNames.isEmpty()) {
                wroteSymbolNames = false;
                emit symbolNamesChanged();
            }
            while (mSymbolNames.isEmpty()) {
                mCond.wait(&mMutex, 10000);
                if (mStopped)
                    return;
            }
            qSwap(symbolNames, mSymbolNames);
        }
        warning() << "SymbolNameSyncer::run woke up symbolNames" << symbolNames.size();
        QList<QByteArray> out;

        if (!symbolNames.isEmpty()) {
            QElapsedTimer timer;
            timer.start();
            leveldb::DB *db = Server::instance()->db(Server::SymbolName);

            Rdm::Batch batch(db);

            SymbolNameHash::iterator it = symbolNames.begin();
            const SymbolNameHash::const_iterator end = symbolNames.end();
            while (it != end) {
                const char *key = it.key().constData();
                const QSet<Location> added = it.value();
                QSet<Location> current = Rdm::readValue<QSet<Location> >(db, key);
                if (Rdm::addTo(current, added)) {
                    batch.add(key, current);
                }
                ++it;
            }

            batch.write();
            if (batch.totalWritten) {
                out += QByteArray("Wrote " + QByteArray::number(symbolNames.size()) + " symbolNames "
                                  + QByteArray::number(batch.totalWritten) + " bytes in "
                                  + QByteArray::number(timer.elapsed()) + "ms");
                wroteSymbolNames = true;
            }
        }
        if (!out.isEmpty())
            error() << RTags::join(out, ", ").constData();
    }
}

void SymbolNameSyncer::maybeWake()
{
    const int size = mSymbolNames.size();
    enum { MaxSize = 1024 * 64 };
    if (size > MaxSize) // ### tunable?
        mCond.wakeOne();
}
