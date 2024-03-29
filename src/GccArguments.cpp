#include "GccArguments.h"
#include "Log.h"
#include "RTags.h"
#include "Process.h"
#include "Server.h"

GccArguments::GccArguments()
    : mLang(NoLang)
{
}

void GccArguments::clear()
{
    mClangArgs.clear();
    mInputFiles.clear();
    mUnresolvedInputFiles.clear();
    mOutputFile.clear();
    mBase.clear();
    mCompiler.clear();
    mLang = NoLang;
}

static inline GccArguments::Lang guessLang(const Path &fullPath)
{
    ByteArray compiler = fullPath.fileName();
    ByteArray c;
    int dash = compiler.lastIndexOf('-');
    if (dash >= 0) {
        c = ByteArray(compiler.constData() + dash + 1, compiler.size() - dash - 1);
    } else {
        c = ByteArray(compiler.constData(), compiler.size());
    }

    if (c.size() != compiler.size()) {
        bool isVersion = true;
        for (int i=0; i<c.size(); ++i) {
            if ((c.at(i) < '0' || c.at(i) > '9') && c.at(i) != '.') {
                isVersion = false;
                break;
            }
        }
        if (isVersion) {
            dash = compiler.lastIndexOf('-', dash - 1);
            if (dash >= 0) {
                c = compiler.mid(dash + 1, compiler.size() - c.size() - 2 - dash);
            } else {
                c = compiler.left(dash);
            }
        }
    }

    GccArguments::Lang lang = GccArguments::NoLang;
    if (c.startsWith("g++") || c.startsWith("c++")) {
        lang = GccArguments::CPlusPlus;
    } else if (c.startsWith("gcc") || c.startsWith("cc")) {
        lang = GccArguments::C;
    }
    return lang;
}

static inline void eatAutoTools(List<ByteArray> &args)
{
    List<ByteArray> copy = args;
    for (int i=0; i<args.size(); ++i) {
        if (args.at(i).contains("gcc") || args.at(i).contains("g++") || args.at(i) == "cd" || args.at(i) == "c++") {
            if (i) {
                args.erase(args.begin(), args.begin() + i);
                if (testLog(Debug)) {
                    debug() << "ate something " << copy;
                    debug() << "now we have " << args;
                }
            }
            break;
        }
    }
}

static inline ByteArray trim(const char *start, int size)
{
    while (size && isspace(*start)) {
        ++start;
        --size;
    }
    while (size && isspace(start[size - 1])) {
        --size;
    }
    return ByteArray(start, size);
}

bool GccArguments::parse(ByteArray args, const Path &base)
{
    mLang = NoLang;
    mClangArgs.clear();
    mInputFiles.clear();
    mBase = base;

    char quote = '\0';
    List<ByteArray> split;
    ByteArray old2 = args;
    {
        char *cur = args.data();
        char *prev = cur;
        // ### handle escaped quotes?
        int size = args.size();
        while (size > 0) {
            switch (*cur) {
            case '"':
            case '\'':
                if (quote == '\0')
                    quote = *cur;
                else if (*cur == quote)
                    quote = '\0';
                break;
            case ' ':
                if (quote == '\0') {
                    if (cur > prev)
                        split.append(trim(prev, cur - prev));
                    prev = cur + 1;
                }
                break;
            default:
                break;
            }
            --size;
            ++cur;
        }
        if (cur > prev)
            split.append(trim(prev, cur - prev));
    }
    eatAutoTools(split);

    if (split.isEmpty()) {
        clear();
        return false;
    }
    debug() << "GccArguments::parse (" << args << ") => " << split;

    Path path;
    if (split.front() == "cd" && split.size() > 3 && split.at(2) == "&&") {
        path = Path::resolved(split.at(1), base);
        split.erase(split.begin(), split.begin() + 3);
    } else {
        path = base;
    }

    mLang = guessLang(split.front());
    if (mLang == NoLang) {
        clear();
        return false;
    }

    const int s = split.size();
    bool seenCompiler = false;
    for (int i=0; i<s; ++i) {
        const ByteArray &arg = split.at(i);
        if (arg.isEmpty())
            continue;
        if (arg.startsWith('-')) {
            if (arg.startsWith("-x")) {
                ByteArray a;
                if (arg.size() == 2 && i + 1 < s) {
                    a = split.at(++i);
                } else {
                    a = arg.mid(2);
                }
                if (a == "c-header" || a == "c++-header")
                    return false;
                mClangArgs.append("-x");
                mClangArgs.append(a);
            } else if (arg.startsWith("-o")) {
                if (!mOutputFile.isEmpty()) {
                    warning("Already have an output file: %s (new %s)", mOutputFile.constData(), arg.constData());
                }
                const Path out = Path::resolved(arg, path);
                mOutputFile = out;
            } else if (arg.startsWith("-D")) {
                ByteArray a;
                if (arg.size() == 2 && i + 1 < s) {
                    a = (arg + split.at(++i));
                } else {
                    a = arg;
                }
                mClangArgs.append(a);
            } else if (arg.startsWith("-I")) {
                Path inc;
                bool ok = false;
                if (arg.size() > 2) {
                    inc = Path::resolved(arg.mid(2), path, &ok);
                } else if (i + 1 < s) {
                    inc = Path::resolved(split.at(++i), path, &ok);
                }
                if (ok)
                    mClangArgs.append("-I" + inc);
            } else if (arg.startsWith("-std") || arg == "-m32") {
                mClangArgs.append(arg);
            }
        } else {
            if (!seenCompiler) {
                seenCompiler = true;
            } else {
                bool ok;
                Path input = Path::resolved(arg, path, &ok);
                if (input.isSource()) {
                    if (ok)
                        mInputFiles.append(input);
                    mUnresolvedInputFiles.append(arg);
                }
            }
        }
    }

    if (mUnresolvedInputFiles.isEmpty()) {
        clear();
        return false;
    }

    if (mInputFiles.isEmpty()) {
        error("Unable to find or resolve input files");
        const int c = mUnresolvedInputFiles.size();
        for (int i=0; i<c; ++i) {
            const ByteArray &input = mUnresolvedInputFiles.at(i);
            error("  %s", input.constData());
        }
        clear();
        return false;
    }

    mOutputFile = Path::resolved(mOutputFile, path);
    static Map<Path, Path> resolvedFromPath;
    Path &compiler = resolvedFromPath[split.front()];
    if (compiler.isEmpty()) {
        compiler = Process::findCommand(split.front());
        if (compiler.isEmpty()) {
            compiler = split.front();
        }
    }
    mCompiler = compiler;
    return true;
}

GccArguments::Lang GccArguments::lang() const
{
    return mLang;
}

List<ByteArray> GccArguments::clangArgs() const
{
    return mClangArgs;
}

List<Path> GccArguments::inputFiles() const
{
    return mInputFiles;
}

List<Path> GccArguments::unresolvedInputFiles() const
{
    return mUnresolvedInputFiles;
}

Path GccArguments::outputFile() const
{
    return mOutputFile;
}

Path GccArguments::baseDirectory() const
{
    return mBase;
}

void GccArguments::addFlags(const List<ByteArray> &extraFlags)
{
    const int count = extraFlags.size();
    for (int i=0; i<count; ++i) {
        ByteArray flag = extraFlags.at(i);
        if (flag.startsWith("-I")) {
            Path p = Path::resolved(flag.constData() + 2);
            flag.replace(2, flag.size() - 2, p);
        }
        mClangArgs.append(flag);
    }
}

Path GccArguments::compiler() const
{
    return mCompiler;
}

Path GccArguments::projectRoot() const
{
    const List<Path> *files[] = { &mUnresolvedInputFiles, &mInputFiles };
    for (int i=0; i<2; ++i) {
        const List<Path> &list = *files[i];
        for (int j=0; j<list.size(); ++j) {
            Path src = list.at(j);
            if (!src.isAbsolute())
                src.prepend(mBase);
            Path srcRoot = RTags::findProjectRoot(src);
            if (!srcRoot.isEmpty()) {
                return srcRoot;
            }
        }
    }
    return Path();
}
