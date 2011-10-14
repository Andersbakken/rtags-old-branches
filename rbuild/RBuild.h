#ifndef RBUILD_H
#define RBUILD_H

#include "MakefileParser.h"
#include "Path.h"
#include "GccArguments.h"
#include "SystemInformation.h"
#include <QObject>

class CursorKey
{
public:
    CursorKey()
        : kind(CXCursor_FirstInvalid), file(0)
    {}
    CursorKey(const CXCursor &cursor)
        : kind(clang_getCursorKind(cursor)), file(0)
    {
        if (!clang_isInvalid(kind)) {
            CXSourceLocation loc = clang_getCursorLocation(cursor);
            clang_getInstantiationLocation(loc, &file, &line, &col, &off);
            if (file)
                fileName = clang_getFileName(file);
        }
    }

    ~CursorKey()
    {
        if (file)
            clang_disposeString(fileName);
    }

    bool isNull() const
    {
        return !file;
    }
    bool operator<(const CursorKey &other) const
    {
        if (isNull())
            return true;
        if (other.isNull())
            return false;
        const int ret = strcmp(clang_getCString(fileName), clang_getCString(other.fileName));
        if (ret < 0)
            return true;
        if (ret > 0)
            return false;
        return kind < other.kind;
    }

    bool operator==(const CursorKey &other) const
    {
        if (isNull())
            return other.isNull();
        return kind == other.kind && !strcmp(clang_getCString(fileName), clang_getCString(other.fileName));
    }

    CXCursorKind kind;
    CXFile file;
    CXString fileName;
    unsigned line, col, off;
};

static inline uint qHash(const CursorKey &key)
{
#define HASHCHAR(ch)                            \
    h = (h << 4) + ch;                          \
    h ^= (h & 0xf0000000) >> 23;                \
    h &= 0x0fffffff;                            \
    ++h;

    uint h = 0;
    if (!key.isNull()) {    
        const char *ch = clang_getCString(key.fileName);
        Q_ASSERT(ch);
        while (*ch) {
            HASHCHAR(*ch);
            ++ch;
        }
        const quint16 uints[] = { key.kind, key.line, key.col };
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
