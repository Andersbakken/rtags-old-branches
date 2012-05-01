#ifndef SymbolSyncer_h
#define SymbolSyncer_h

#include "Indexer.h"
#include <QtCore>

class SymbolSyncer : public QThread
{
    Q_OBJECT
public:
    SymbolSyncer(QObject* parent = 0);

    void addSymbols(const SymbolHash &data, const ReferenceHash &references);
    void notify();
    void stop();

protected:
    void run();
private:
    void maybeWake();

    bool mStopped;
    QMutex mMutex;
    QWaitCondition mCond, mJobCond;
    SymbolHash mSymbols;
    ReferenceHash mReferences;
};

#endif
