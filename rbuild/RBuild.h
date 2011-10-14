#ifndef RBUILD_H
#define RBUILD_H

#include "MakefileParser.h"
#include "Path.h"
#include "GccArguments.h"
#include "SystemInformation.h"
#include <QObject>

class CursorKey;
static inline uint qHash(const CursorKey &key);
class CursorKey
{
public:
    CursorKey(const CursorKey &other)
        : d(other.d)
    {}
    CursorKey()
        : d(0)
    {}
    CursorKey(const CXCursor &cursor)
        : d(0)
    {
        const CXCursorKind kind = clang_getCursorKind(cursor);
        if (!clang_isInvalid(kind)) {
            CXSourceLocation loc = clang_getCursorLocation(cursor);
            CXFile file;
            unsigned line, col, off;
            clang_getInstantiationLocation(loc, &file, &line, &col, &off);
            if (file) {
                const CXString fn = clang_getFileName(file);
                char *fileName = realpath(clang_getCString(fn), 0);
                if (fileName)
                    d = new CursorKeyData(kind, fileName, line, col, off);
                clang_disposeString(fn);
            }
        }
    }

    CursorKey &operator=(const CursorKey &other)
    {
        d = other.d;
        return *this;
    }

    bool isNull() const
    {
        return !d;
    }

    bool isValid() const
    {
        return d;
    }

    bool operator<(const CursorKey &other) const
    {
        if (isNull())
            return true;
        if (other.isNull())
            return false;
        const int ret = strcmp(d->fileName, other.d->fileName);
        if (ret < 0)
            return true;
        if (ret > 0)
            return false;
        return d->kind < other.d->kind;
    }

    bool operator==(const CursorKey &other) const
    {
        if (isNull())
            return other.isNull();
        if (other.isNull())
            return isNull();
        
        return d->kind == other.d->kind && !strcmp(d->fileName, other.d->fileName);
    }
private:
    class CursorKeyData : public QSharedData
    {
    public:
        CursorKeyData(CXCursorKind k, char *fn, unsigned l, unsigned c, unsigned o)
            : kind(k), fileName(fn), line(l), col(c), off(o), hash(0)
        {}
        ~CursorKeyData()
        {
            if (fileName)
                free(fileName);
        }

        CXCursorKind kind;
        char *fileName;
        unsigned line, col, off;
        mutable uint hash;
    };
    
    QSharedDataPointer<CursorKeyData> d;
    friend uint qHash(const CursorKey&);
};

static inline uint qHash(const CursorKey &key)
{
    if (key.isNull())
        return 0;
    uint &h = key.d->hash;
    if (!h) {
#define HASHCHAR(ch)                            \
        h = (h << 4) + ch;                      \
        h ^= (h & 0xf0000000) >> 23;            \
        h &= 0x0fffffff;                        \
        ++h;

        const char *ch = key.d->fileName;
        Q_ASSERT(ch);
        while (*ch) {
            HASHCHAR(*ch);
            ++ch;
        }
        const quint16 uints[] = { key.d->kind, key.d->line, key.d->col };
        for (int i=0; i<3; ++i) {
            ch = reinterpret_cast<const char*>(&uints[i]);
            for (int j=0; j<2; ++j) {
                HASHCHAR(*ch);
                ++ch;
            }
        }
    }
#undef HASHCHAR
    return h;
}

class RBuild : public QObject
{
    Q_OBJECT
    public:
    RBuild(QObject *parent = 0);

    void init(const Path& makefile);

    struct Entry
    {
        Entry();
        ~Entry();

        Path file;
        unsigned int line, column;
        QByteArray field;

        int cxKind;

        QList<Entry*> children;
        Entry* parent, *container;
    };

private slots:
    void makefileDone();
    void makefileFileReady(const MakefileItem& file);
    void startParse();

private:
    void compile(const GccArguments& arguments);

private:
    Path mMakefile;
    MakefileParser mParser;
    QList<Entry*> mEntries;
    QHash<CursorKey, RBuild::Entry*> mSeen;
    SystemInformation mSysInfo;
};

#endif // RBUILD_H
