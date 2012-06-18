#ifndef Database_h
#define Database_h

#include <leveldb/db.h>
#include <leveldb/write_batch.h>
#include <QtCore>
#include "Location.h"
#include "CursorInfo.h"

struct Slice {
    Slice(const std::string &str);
    Slice(const QByteArray &d);
    Slice(const char *d = 0, int s = -1);
    const char *data() const;
    int size() const;
    void clear();
    bool operator==(const Slice &other) const;
    bool operator!=(const Slice &other) const;
    QByteArray byteArray() const { return QByteArray(data(), size()); }
private:
    Slice(const leveldb::Slice &slice);
    leveldb::Slice mSlice;
    friend class Database;
    friend class Iterator;
    friend struct Batch;
};

static inline QDebug operator<<(QDebug dbg, const Slice &slice)
{
    dbg << std::string(slice.data(), slice.size()).c_str();
    return dbg;
}

template <typename T> QByteArray encode(const T &t)
{
    QByteArray out;
    QDataStream ds(&out, QIODevice::WriteOnly);
    ds << t;
    return out;
}

template <typename T> T decode(const Slice &slice)
{
    const QByteArray ba = QByteArray::fromRawData(slice.data(), slice.size());
    QDataStream ds(ba);
    T t;
    ds >> t;
    return t;
}

template <> inline QByteArray encode(const QSet<Location> &locations)
{
    QByteArray out(locations.size() * sizeof(quint64), '\0');
    quint64 *ptr = reinterpret_cast<quint64*>(out.data());
    for (QSet<Location>::const_iterator it = locations.begin(); it != locations.end(); ++it) {
        *ptr++ = (*it).mData;
    }
    return out;
}

template <> inline QSet<Location> decode(const Slice &slice)
{
    QSet<Location> ret;
    const quint64 *ptr = reinterpret_cast<const quint64*>(slice.data());
    const int count = slice.size() / sizeof(quint64);
    for (int i=0; i<count; ++i) {
        ret.insert(Location(*ptr++));
    }
    return ret;
}

template <> inline QByteArray encode(const CursorInfo &info)
{
    // null-terminated symbolName, quint32(symbolLength), quint32(kind), quint8(isDefinition), quint64(target.location), quint64(refs)...
    QByteArray out(info.symbolName.size() + 1 + (sizeof(quint32) * 2) + sizeof(quint8)
                   + (sizeof(quint64) * (1 + info.references.size())), '\0');
    memcpy(out.data(), info.symbolName.constData(), info.symbolName.size() + 1);
    quint32 *ptr = reinterpret_cast<quint32*>(out.data() + (info.symbolName.size() + 1));
    *ptr++ = info.symbolLength;
    *ptr++ = info.kind;
    quint8 *isDefinitionPtr = reinterpret_cast<quint8*>(ptr);
    *isDefinitionPtr++ = info.isDefinition;
    quint64 *locPtr = reinterpret_cast<quint64*>(isDefinitionPtr);
    *locPtr++ = info.target.mData;
    *locPtr++ = info.parent.mData;
    foreach(const Location &loc, info.references) {
        *locPtr++ = loc.mData;
    }
    return out;
}

template <> inline CursorInfo decode(const Slice &slice)
{
    CursorInfo ret;
    ret.symbolName = QByteArray(slice.data()); // 0-terminated
    const quint32 *ptr = reinterpret_cast<const quint32*>(slice.data() + ret.symbolName.size() + 1);
    ret.symbolLength = *ptr++;
    ret.kind = static_cast<CXCursorKind>(*ptr++);
    const quint8 *isDefinitionPtr = reinterpret_cast<const quint8*>(ptr);
    ret.isDefinition = *isDefinitionPtr++;
    const quint64 *locPtr = reinterpret_cast<const quint64*>(isDefinitionPtr);
    const int count = ((slice.size() - ret.symbolName.size() - sizeof(char) - (sizeof(quint32) * 2) - sizeof(quint8)) / sizeof(quint64));
    ret.target.mData = *locPtr++;
    ret.parent.mData = *locPtr++;
    for (int i=0; i<count - 1; ++i) {
        const Location loc(*locPtr++);
        ret.references.insert(loc);
    }
    return ret;
}


class Iterator
{
    Iterator(leveldb::Iterator *iterator);
public:
    ~Iterator();
    void seekToFirst();
    void seekToLast();
    bool isValid() const;
    void next();
    void previous();
    Slice key() const;
    Slice rawValue() const;
    void seek(const Slice &slice);
    template <typename T> T value() const { return decode<T>(rawValue()); }
private:
    leveldb::Iterator *mIterator;
    friend class Database;
};

class LocationComparator;
class Database
{
public:
    Database(const char *path, int cacheSizeMB, bool locationKeys);
    ~Database();
    void lockForRead() { mLock.lockForRead(); }
    void lockForWrite() { mLock.lockForWrite(); }
    void unlock() { mLock.unlock(); }
    bool isOpened() const;
    void close();
    QByteArray openError() const;
    std::string rawValue(const Slice &key, bool *ok = 0) const;
    template <typename T> T value(const Slice &key, bool *ok = 0) {
        const std::string val = rawValue(key, ok);
        if (!val.empty())
            return decode<T>(val);
        return T();
    }
    int setRawValue(const Slice &key, const Slice &value);
    template <typename T> int setValue(const Slice &key, const T &t) { return setRawValue(key, encode(t)); }
    bool contains(const Slice &key) const;
    void remove(const Slice &key);
    Iterator *createIterator() const;
private:
    QReadWriteLock mLock;
    leveldb::DB *mDB;
    const leveldb::WriteOptions mWriteOptions;
    QByteArray mOpenError;
    LocationComparator *mLocationComparator;
    friend struct Batch;
};

struct Batch {
    enum { BatchThreshold = 1024 * 1024 };
    Batch(Database *d);
    ~Batch();
    int flush();
    template <typename T> int add(const Slice &key, const T &t)
    {
        const QByteArray encoded = encode<T>(t);
        return addEncoded(key, Slice(encoded));
    }

    void remove(const Slice &key);
    int size() const { return mSize; }
    int total() const { return mTotal; }
    int addEncoded(const Slice &key, const Slice &data);
private:
    Database *mDB;
    int mSize, mTotal;
    leveldb::WriteBatch mBatch;
};


#endif
