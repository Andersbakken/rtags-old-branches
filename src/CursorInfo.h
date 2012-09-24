#ifndef CursorInfo_h
#define CursorInfo_h

#include "ByteArray.h"
#include "Location.h"
#include "Path.h"
#include "Log.h"
#include <clang-c/Index.h>
#include "RTags.h"

class CursorInfo
{
public:
    enum { InvalidKind = -2, ReferenceKind = -1 };
    CursorInfo()
        : symbolLength(0), kind(InvalidKind), isDefinition(false)
    {}
    void clear()
    {
        symbolLength = 0;
        kind = InvalidKind;
        isDefinition = false;
        target.clear();
        references.clear();
        symbolName.clear();
    }

    bool dirty(const Set<uint32_t> &dirty)
    {
        bool changed = false;
        const uint32_t targetFileId = target.fileId();
        if (targetFileId && dirty.contains(targetFileId)) {
            changed = true;
            target.clear();
        }

        Set<Location>::iterator it = references.begin();
        while (it != references.end()) {
            if (dirty.contains(it->fileId())) {
                changed = true;
                references.erase(it++);
            } else {
                ++it;
            }
        }
        return changed;
    }

    bool isValid() const
    {
        return !isEmpty();
    }

    bool isEmpty() const
    {
        assert((symbolLength || symbolName.isEmpty()) && (symbolLength || kind == InvalidKind)); // these should be coupled
        return !symbolLength && !target && references.isEmpty();
    }

    bool unite(const CursorInfo &other)
    {
        bool changed = false;
        if (!target && other.target.isValid()) {
            target = other.target;
            changed = true;
        }

        if (!symbolLength && other.symbolLength) {
            symbolLength = other.symbolLength;
            kind = other.kind;
            isDefinition = other.isDefinition;
            symbolName = other.symbolName;
            changed = true;
        }
        const int oldSize = references.size();
        if (!oldSize) {
            references = other.references;
            if (!references.isEmpty())
                changed = true;
        } else {
            int inserted = 0;
            references.unite(other.references, &inserted);
            if (inserted)
                changed = true;
        }

        return changed;
    }

    const char *kindSpelling() const
    {
        return RTags::kindToString(static_cast<CXIdxEntityKind>(kind));
    }

    bool isReference() const
    {
        return kind == ReferenceKind;
    }

    ByteArray toString() const;

    ByteArray symbolName; // this is fully qualified Foobar::Barfoo::foo
    int symbolLength : 8; // this is just the symbol name length e.g. foo => 3
    int kind : 15; // -1 means reference, -2 means invalid
    bool isDefinition : 1;
    Location target;
    Set<Location> references;
};

inline Log operator<<(Log log, const CursorInfo &info)
{
    log << info.toString();
    return log;
}

#endif
