#include "SymbolSyncer.h"
#include "leveldb/db.h"
#include "Server.h"

SymbolSyncer::SymbolSyncer(QObject* parent)
    : QThread(parent), mStopped(false)
{
}

void SymbolSyncer::stop()
{
    QMutexLocker locker(&mMutex);
    mStopped = true;
    mCond.wakeOne();
}

void SymbolSyncer::notify()
{
    QMutexLocker locker(&mMutex); // is this needed here?
    mCond.wakeOne();
}

void SymbolSyncer::addSymbols(const SymbolHash &symbols, const ReferenceHash &references)
{
    QMutexLocker lock(&mMutex);
    QElapsedTimer timer;
    timer.start();
    while (mSymbols.size() + mReferences.size() > 50000) {
        // qDebug() << "waiting for stuff to clear up" << (mSymbols.size() + mReferences.size());
        mJobCond.wait(&mMutex);
        if (mSymbols.size() + mReferences.size() <= 50000) {
            qDebug() << "we waited for" << timer.elapsed() << "ms before starting"
                     << "with our" << (symbols.size() + references.size());
        }
    }

    // qDebug() << "adding" << symbols.size() << "symbols and" << references.size() << "references"
    //          << "we currently have" << mSymbols.size() << "symbols and" << mReferences.size() << "references";

    if (mSymbols.isEmpty()) {
        mSymbols = symbols;
    } else {
        const SymbolHash::const_iterator end = symbols.end();
        for (SymbolHash::const_iterator it = symbols.begin(); it != end; ++it) {
            mSymbols[it.key()].unite(it.value());
        }
    }
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

void SymbolSyncer::run()
{
    while (true) {
        SymbolHash symbols;
        ReferenceHash references;
        {
            QMutexLocker locker(&mMutex);
            if (mStopped)
                return;
            while (mSymbols.isEmpty() && mReferences.isEmpty()) {
                mCond.wait(&mMutex, 10000);
                if (mStopped)
                    return;
            }
            qSwap(symbols, mSymbols);
            qSwap(references, mReferences);
        }
        mJobCond.wakeAll();
        warning() << "SymbolSyncer::run woke up symbols" << symbols.size()
                  << "references" << references.size();
        QList<QByteArray> out;
        Q_ASSERT(!references.isEmpty() || !symbols.isEmpty());

        QElapsedTimer timer;
        timer.start();
        leveldb::DB *symbolDB = Server::instance()->db(Server::Symbol);

        Rdm::Batch batch(symbolDB);

        if (!references.isEmpty()) {
            const ReferenceHash::const_iterator end = references.end();
            for (ReferenceHash::const_iterator it = references.begin(); it != end; ++it) {
                const SymbolHash::iterator sym = symbols.find(it.value().first);
                if (sym != symbols.end()) {
                    CursorInfo &ci = sym.value();
                    ci.references.insert(it.key());
                    // if (it.value().first.path.contains("RTags.h"))
                    //     error() << "cramming" << it.key() << "into" << it.value();
                    if (it.value().second != Rdm::NormalReference) {
                        CursorInfo &other = symbols[it.key()];
                        ci.references += other.references;
                        other.references += ci.references;
                        if (other.target.isNull())
                            other.target = it.value().first;
                        if (ci.target.isNull())
                            ci.target = it.key();
                    }
                } else {
                    const QByteArray key = it.value().first.key(Location::Padded);
                    CursorInfo current = Rdm::readValue<CursorInfo>(symbolDB, key.constData());
                    bool changedCurrent = false;
                    if (Rdm::addTo(current.references, it.key()))
                        changedCurrent = true;
                    if (it.value().second != Rdm::NormalReference) {
                        const QByteArray otherKey = it.key().key(Location::Padded);
                        CursorInfo other = Rdm::readValue<CursorInfo>(symbolDB, otherKey);
                        bool changedOther = false;
                        if (Rdm::addTo(other.references, it.key()))
                            changedOther = true;
                        if (Rdm::addTo(other.references, current.references))
                            changedOther = true;
                        if (Rdm::addTo(current.references, other.references))
                            changedCurrent = true;

                        if (other.target.isNull()) {
                            other.target = it.value().first;
                            changedOther = true;
                        }

                        if (current.target.isNull()) {
                            current.target = it.key();
                            changedCurrent = true;
                        }

                        if (changedOther) {
                            batch.add(otherKey, other);
                        }
                        // error() << "ditched reference" << it.key() << it.value();
                    }
                    if (changedCurrent) {
                        batch.add(key, current);
                    }
                }
            }
        }
        if (!symbols.isEmpty()) {
            SymbolHash::iterator it = symbols.begin();
            const SymbolHash::const_iterator end = symbols.end();
            while (it != end) {
                const QByteArray key = it.key().key(Location::Padded);
                CursorInfo added = it.value();
                bool ok;
                CursorInfo current = Rdm::readValue<CursorInfo>(symbolDB, key.constData(), &ok);
                if (!ok) {
                    batch.add(key, added);
                } else if (current.unite(added)) {
                    batch.add(key, current);
                }
                ++it;
            }
        }
        batch.write();
        if (batch.totalWritten) {
            out += QByteArray("Wrote " + QByteArray::number(symbols.size())
                              + " symbols and " + QByteArray::number(references.size())
                              + " references " + QByteArray::number(batch.totalWritten) + " bytes in "
                              + QByteArray::number(timer.elapsed()) + "ms");
        }
        if (!out.isEmpty())
            error() << RTags::join(out, ", ").constData();
    }
}

void SymbolSyncer::maybeWake()
{
    const int size = mSymbols.size() + mReferences.size();
    enum { MaxSize = 1024 * 32 };
    if (size > MaxSize) // ### tunable?
        mCond.wakeOne();
}
