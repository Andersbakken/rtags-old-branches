#ifndef GRParser_h
#define GRParser_h

#define __STDC_LIMIT_MACROS
#define __STDC_CONSTANT_MACROS
#include <clang-c/Index.h>
#include <string.h>
#include <stdlib.h>
#include <clang/Lex/Lexer.h>
#include <clang/Basic/SourceLocation.h>
#include <stack>
#include "GRTags.h"
#include "Map.h"
#include "ByteArray.h"
#include "StopWatch.h"
#include "Log.h"
#include "RTags.h"

class GRParser
{
public:
    enum Option {
        None = 0x0,
        CPlusPlus = 0x1
    };
    GRParser();
    ~GRParser();
    int parse(const Path &file, unsigned opts, Map<ByteArray, Map<Location, bool> > &entries);
private:
    void addEntry(const ByteArray &name, const List<ByteArray> &containerScope, int offset);
    void addReference(const ByteArray &name, int offset);
    inline clang::tok::TokenKind kind(int idx) const
    {
        if (idx < 0 || idx >= mTokens.size())
            return clang::tok::NUM_TOKENS;
        return mTokens.at(idx).getKind();
    }

    int findMatching(int idx) const;
    void handleRawIdentifier();
    void handleLeftBrace();
    void handleColon();
    void handleLeftParen();
    void handleRightBrace();
    void handleSemi();
    int addContext(int idx); // returns number added
    inline void tokenSpelling(const clang::Token &token, const char *&string, int &length) const
    {
        string = mBuf + tokenOffset(token);
        length = token.getLength();
    }

    inline ByteArray tokenSpelling(const clang::Token &token) const
    {
        return ByteArray(mBuf + tokenOffset(token), token.getLength());
    }

    static inline int tokenOffset(const clang::Token &token)
    {
        enum { MacroIDBit = 1U << 31 };
        return token.getLocation().getRawEncoding() & ~MacroIDBit;
    }

    Path mFileName;
    clang::Lexer *mLexer;
    int mBraceCount, mSize, mCount, mCurrentToken;
    char *mBuf;
    enum StateType {
        Global,
        FunctionBody,
        ContainerPending,
        FunctionPending,
        Container
    };
    struct State {
        State(StateType t = Global, int idx = -1, const ByteArray &n = ByteArray())
            : type(t), braceIndex(idx), name(n), pendingIndex(-1)
        {}
        StateType type;
        int braceIndex; // what brace index this state should get popped on or -1
        ByteArray name; // for classes/structs/namespaces
        int pendingIndex; // index of where the real class is for pending
                          // container states or where the real function is for
                          // pendingfunction
    };
    std::stack<State> mState;
    List<ByteArray> mContainerScope;
    List<clang::Token> mTokens;
    uint32_t mFileId;
    Map<ByteArray, Map<Location, bool> > *mEntries;
};

#endif
