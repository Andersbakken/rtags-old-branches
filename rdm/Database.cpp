#ifdef USE_KYOTO
#include <kcdbext.h>
#include <kcdb.h>
#endif
#include "Database.h"
#ifdef USE_LEVELDB
#include <leveldb/cache.h>
#include <leveldb/comparator.h>
#endif

#ifdef USE_LEVELDB

// ================== Slice ==================

Slice::Slice(const std::string &string)
    : mSlice(string.data(), string.size())
{}

Slice::Slice(const leveldb::Slice &slice)
    : mSlice(slice)
{}

Slice::Slice(const QByteArray &d)
    : mSlice(d.constData(), d.size())
{}

Slice::Slice(const char *d, int s)
    : mSlice(d, s == -1 ? strlen(d) : s)
{}

bool Slice::operator==(const Slice &other) const
{
    return mSlice == other.mSlice;
}

bool Slice::operator!=(const Slice &other) const
{
    return mSlice != other.mSlice;
}

const char *Slice::data() const
{
    return mSlice.data();
}

int Slice::size() const
{
    return mSlice.size();
}

void Slice::clear()
{
    mSlice.clear();
}

// ================== Iterator ==================

Iterator::Iterator(leveldb::Iterator *iterator)
    : mIterator(iterator)
{
}

Iterator::~Iterator()
{
    delete mIterator;
}

void Iterator::seekToFirst()
{
    mIterator->SeekToFirst();
}

void Iterator::seekToLast()

{    mIterator->SeekToLast();
}

void Iterator::seek(const Slice &slice)
{
    mIterator->Seek(slice.mSlice);
}

bool Iterator::isValid() const
{
    return mIterator->Valid();
}

void Iterator::next()
{
    mIterator->Next();
}

void Iterator::previous()
{
    mIterator->Prev();
}

Slice Iterator::key() const
{
    return Slice(mIterator->key());
}

Slice Iterator::value() const
{
    return mIterator->value();
}

// ================== Database ==================

class LocationComparator : public leveldb::Comparator
{
public:
    int Compare(const leveldb::Slice &left, const leveldb::Slice &right) const
    {
        Q_ASSERT(left.size() == right.size());
        Q_ASSERT(left.size() == 8);
        const quint32 *l = reinterpret_cast<const quint32*>(left.data());
        const quint32 *r = reinterpret_cast<const quint32*>(right.data());
        if (*l < *r)
            return -1;
        if (*l > *r)
            return 1;
        ++l;
        ++r;
        if (*l < *r)
            return -1;
        if (*l > *r)
            return 1;
        return 0;
    }

    const char* Name() const { return "LocationComparator"; }
    void FindShortestSeparator(std::string*, const leveldb::Slice&) const { }
    void FindShortSuccessor(std::string*) const { }
};

Database::Database(const char *path, int cacheSizeMB, bool locationKeys)
    : mDB(0), mLocationComparator(locationKeys ? new LocationComparator : 0)
{
    leveldb::Options opt;
    opt.create_if_missing = true;
    if (locationKeys)
        opt.comparator = mLocationComparator;
    if (cacheSizeMB)
        opt.block_cache = leveldb::NewLRUCache(cacheSizeMB * 1024 * 1024);
    leveldb::Status status = leveldb::DB::Open(opt, path, &mDB);
    if (!status.ok())
        mOpenError = status.ToString().c_str();
}

Database::~Database()
{
    delete mLocationComparator;
}

bool Database::isOpened() const
{
    return mDB;
}

void Database::close()
{
    delete mDB;
    mDB = 0;
    mOpenError.clear();
}

QByteArray Database::openError() const
{
    return mOpenError;
}

std::string Database::rawValue(const Slice &key, bool *ok) const
{
    std::string value;
    leveldb::Status status = mDB->Get(leveldb::ReadOptions(), key.mSlice, &value);
    if (ok)
        *ok = status.ok();
    return value;
}

int Database::setRawValue(const Slice &key, const Slice &value)
{
    mDB->Put(mWriteOptions, key.mSlice, value.mSlice);
    return value.size();
}
bool Database::contains(const Slice &key) const
{
    bool ok = false;
    rawValue(key, &ok);
    return ok;
}

void Database::remove(const Slice &key)
{
    mDB->Delete(mWriteOptions, key.mSlice);
}

Iterator *Database::createIterator() const
{
    return new Iterator(mDB->NewIterator(leveldb::ReadOptions()));
}

void Database::flush()
{
}

Batch::Batch(Database *d)
    : mDB(d), mSize(0), mTotal(0)
{}

Batch::~Batch()
{
    flush();
}

int Batch::flush()
{
    const int was = mSize;
    if (mSize) {
        // error("About to write %d bytes to %p", batchSize, db);
        mDB->mDB->Write(mDB->mWriteOptions, &mBatch);
        mBatch.Clear();
        mTotal += mSize;
        // error("Wrote %d (%d) to %p", batchSize, totalWritten, db);
        mSize = 0;
    }
    return was;
}

int Batch::writeEncoded(const Slice &key, const Slice &data)
{
    mBatch.Put(key.mSlice, data.mSlice);
    mSize += data.size();
    if (mSize >= BatchThreshold) {
        flush();
    }
    return data.size();
}


void Batch::remove(const Slice &key)
{
    mBatch.Delete(key.mSlice);
}

#elif USE_KYOTO

// ================== Slice ==================

Slice::Slice(const std::string &string)
    : mSlicePtr(string.data()), mSliceSize(string.size())
{}

Slice::Slice(const QByteArray &d)
    : mSlicePtr(d.constData()), mSliceSize(d.size())
{}

Slice::Slice(const char *d, int s)
    : mSlicePtr(d), mSliceSize(s == -1 ? strlen(d) : s)
{}

bool Slice::operator==(const Slice &other) const
{
    if (mSlicePtr == other.mSlicePtr)
        return true;
    if (mSliceSize != other.mSliceSize)
        return false;
    return !memcmp(mSlicePtr, other.mSlicePtr, mSliceSize);
}

bool Slice::operator!=(const Slice &other) const
{
    if (mSlicePtr == other.mSlicePtr)
        return false;
    if (mSliceSize != other.mSliceSize)
        return true;
    return (memcmp(mSlicePtr, other.mSlicePtr, mSliceSize) != 0);
}

const char *Slice::data() const
{
    return mSlicePtr;
}

int Slice::size() const
{
    return mSliceSize;
}

void Slice::clear()
{
    mSlicePtr = "";
    mSliceSize = 0;
}

// ================== Iterator ==================

Iterator::Iterator(kyotocabinet::BasicDB::Cursor *cursor)
    : mCursor(cursor), mIsValid(false)
{
}

Iterator::~Iterator()
{
    delete mCursor;
}

void Iterator::seekToFirst()
{
    mIsValid = mCursor->jump();
}

void Iterator::seekToLast()
{
    mIsValid = mCursor->jump_back();
}

static inline bool isGreater(const char* v, size_t sz, const Slice& slice)
{
    const size_t cmpsz = qMin(static_cast<int>(sz), slice.size());
    return strncmp(v, slice.data(), cmpsz) > 0;
}

void Iterator::seek(const Slice &slice)
{
    mIsValid = mCursor->jump(slice.mSlicePtr, slice.mSliceSize);
    if (!mIsValid) {
        mIsValid = mCursor->jump();
        if (!mIsValid) {
            return;
        }
        char* v;
        size_t sz;
        for (;;) {
            v = mCursor->get_key(&sz, true); // 'true' means increase the cursor position
            if (!v) {
                mIsValid = false;
                return;
            }
            if (isGreater(v, sz, slice))
                return;
        }
    }
}

bool Iterator::isValid() const
{
    return mIsValid;
}

void Iterator::next()
{
    mIsValid = mCursor->step();
}

void Iterator::previous()
{
    mIsValid = mCursor->step_back();
}

Slice Iterator::key() const
{
    size_t sz;
    char* k = mCursor->get_key(&sz);
    return Slice(k, sz);
}

Slice Iterator::value() const
{
    size_t sz;
    char* v = mCursor->get_value(&sz);
    return Slice(v, sz);
}

// ================== Database ==================

Database::Database(const char *path, const Server::Options &options, bool locationKeys)
    : mDB(0)
{
    mDB = new kyotocabinet::IndexDB;
    std::string realPath(path);
    realPath += ".kcf";
    if (locationKeys)
        realPath += "#comparator=dec";
    if (!mDB->open(realPath)) {
        kyotocabinet::BasicDB::Error err = mDB->error();
        mOpenError = err.message();
    }
}

Database::~Database()
{
    close();
}

bool Database::isOpened() const
{
    return mDB->error().code() == kyotocabinet::BasicDB::Error::SUCCESS;
}

void Database::close()
{
    if (mDB) {
        mDB->close();
        delete mDB;
        mDB = 0;
        mOpenError.clear();
    }
}

QByteArray Database::openError() const
{
    return mOpenError;
}

std::string Database::rawValue(const Slice &key, bool *ok) const
{
    std::string value;
    bool tmp;
    if (!ok)
        ok = &tmp;
    *ok = mDB->get(std::string(key.data(), key.size()), &value);
    return value;
}

int Database::setRawValue(const Slice &key, const Slice &value)
{
    mDB->set(key.data(), key.size(), value.data(), value.size());
    return value.size();
}

bool Database::contains(const Slice &key) const
{
    bool ok = false;
    rawValue(key, &ok);
    return ok;
}

void Database::remove(const Slice &key)
{
    mDB->remove(key.data(), key.size());
}

Iterator *Database::createIterator() const
{
    return new Iterator(mDB->cursor());
}

void Database::flush()
{
    //    mDB->synchronize();
}

Batch::Batch(Database *d)
    : mDB(d), mSize(0), mTotal(0)
{}

Batch::~Batch()
{
    flush();
    mDB->flush();
}

int Batch::flush()
{
    return 0;
}

int Batch::writeEncoded(const Slice &key, const Slice &data)
{
    mDB->mDB->set(key.data(), key.size(), data.data(), data.size());
    return data.size();
}


void Batch::remove(const Slice &key)
{
    mDB->mDB->remove(key.data(), key.size());
}
#else
#error No Datatype selected
#endif
