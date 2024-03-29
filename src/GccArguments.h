#ifndef GCCARGUMENTS_H
#define GCCARGUMENTS_H

#include "Path.h"
#include "List.h"
#include "ByteArray.h"

class GccArgumentsImpl;

class GccArguments
{
public:
    enum Lang { NoLang, C, CPlusPlus };

    GccArguments();

    bool parse(ByteArray args, const Path &base);
    Lang lang() const;
    void clear();

    void addFlags(const List<ByteArray> &extraFlags);
    List<ByteArray> clangArgs() const;
    List<Path> inputFiles() const;
    List<Path> unresolvedInputFiles() const;
    Path outputFile() const;
    Path baseDirectory() const;
    Path compiler() const;
    Path projectRoot() const;
private:
    List<ByteArray> mClangArgs;
    List<Path> mInputFiles, mUnresolvedInputFiles;
    Path mOutputFile, mBase, mCompiler;
    GccArguments::Lang mLang;
    friend class MakefileParser;
    friend class Server;
};

#endif
