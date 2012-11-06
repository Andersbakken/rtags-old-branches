#ifndef Match_h
#define Match_h

#include "ByteArray.h"
#include "RegExp.h"

class Match
{
public:
    enum Flag {
        Flag_None = 0x0,
        Flag_StringMatch = 0x1,
        Flag_RegExp = 0x2,
        Flag_CaseInsensitive = 0x4
    };
    inline Match(const ByteArray &pattern, unsigned flags = Flag_StringMatch)
        : mFlags(flags)
    {
        if (flags & Flag_RegExp)
            mRegExp = pattern;
        if (flags & Flag_StringMatch)
            mPattern = pattern;
    }
    inline Match(const RegExp &regExp)
        : mRegExp(regExp), mFlags(Flag_RegExp)
    {}

    inline bool match(const ByteArray &text) const
    {
        return indexIn(text) != -1;
    }
    inline int indexIn(const ByteArray &text) const
    {
        int index = -1;
        if (mFlags & Flag_StringMatch)
            index = text.indexOf(mPattern, 0, mFlags & Flag_CaseInsensitive ? ByteArray::CaseInsensitive : ByteArray::CaseSensitive);
        if (index == -1 && mFlags & Flag_RegExp)
            index = mRegExp.indexIn(text);
        return index;
    }
    bool isEmpty() const
    {
        return !mFlags || (mPattern.isEmpty() && mRegExp.isEmpty());
    }
private:
    RegExp mRegExp;
    ByteArray mPattern;
    unsigned mFlags;

};

#endif
