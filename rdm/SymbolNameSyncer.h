#ifndef SymbolNameSyncer_h
#define SymbolNameSyncer_h

#include "Indexer.h"
#include <QtCore>

class SymbolNameSyncer : public QThread
{
    Q_OBJECT
public:
    SymbolNameSyncer(QObject* parent = 0);
    void addSymbolNames(const SymbolNameHash &symbolNames);
    void notify();
    void stop();

protected:
    void run();
signals:
    void symbolNamesChanged();
private:
    void maybeWake();

    bool mStopped;
    QMutex mMutex;
    QWaitCondition mCond;
    SymbolNameHash mSymbolNames;
};

#endif
