#include "leveldb/db.h"
#include "Server.h"

IndexerSyncer::IndexerSyncer(Indexer *indexer, QObject* parent)
    : QThread(parent), mIndexer(indexer), mStopped(false)
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

void IndexerSyncer::addSymbolNames(const SymbolNameHash &locations)
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

void IndexerSyncer::addSymbols(const SymbolHash &symbols)
{
    QMutexLocker lock(&mMutex);
    if (mSymbols.isEmpty()) {
        mSymbols = symbols;
    } else {
        const SymbolHash::const_iterator end = symbols.end();
        for (SymbolHash::const_iterator it = symbols.begin(); it != end; ++it) {
            mSymbols[it.key()].unite(it.value());
        }
    }
    maybeWake();
}

void IndexerSyncer::addReferences(const ReferenceHash &references)
{
    QMutexLocker lock(&mMutex);
    if (mReferences.isEmpty()) {
        mReferences = references;
    } else {
        const ReferenceHash::const_iterator end = references.end();
        for (ReferenceHash::const_iterator it = references.begin(); it != end; ++it) {
            mReferences[it.key()] = it.value();
        }
    }
    maybeWake();
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
            while (mSymbols.isEmpty()
                   && mSymbolNames.isEmpty()
                   && mDependencies.isEmpty()
                   && mInformations.isEmpty()
                   && mReferences.isEmpty()
                   && mPchDependencies.isEmpty()
                   && mPchUSRHashes.isEmpty()) {
                mCond.wait(&mMutex, 10000);
                if (mStopped)
                    return;
            }
            qSwap(symbolNames, mSymbolNames);
            qSwap(symbols, mSymbols);
            qSwap(dependencies, mDependencies);
            qSwap(pchDependencies, mPchDependencies);
            qSwap(pchUSRHashes, mPchUSRHashes);
            qSwap(informations, mInformations);
            qSwap(references, mReferences);
        }
        mIndexer->write(symbolNames, symbols, dependencies, pchDependencies,
                        informations, references, pchUSRHashes, &wroteSymbolNames);
    }
}

void IndexerSyncer::maybeWake()
{
    const int size = (mSymbols.size() + mSymbolNames.size() + mDependencies.size() + mPchDependencies.size()
                      + mInformations.size() + mReferences.size() + mPchUSRHashes.size());
    enum { MaxSize = 1024 * 64 };
    if (size > MaxSize) // ### tunable?
        mCond.wakeOne();
}
