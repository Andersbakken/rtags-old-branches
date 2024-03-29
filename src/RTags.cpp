#include "RTags.h"
#include "CursorInfo.h"
#include "Server.h"
#include "StopWatch.h"
#include "Str.h"
#include "config.h"
#include <dirent.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <sys/types.h>
#ifdef OS_FreeBSD
#include <sys/sysctl.h>
#endif
#ifdef OS_Darwin
#include <mach-o/dyld.h>
#endif

bool inSignalHandler = false;

namespace RTags {

int canonicalizePath(char *path, int len)
{
    assert(path[0] == '/');
    for (int i=0; i<len - 3; ++i) {
        if (path[i] == '/' && path[i + 1] == '.'
            && path[i + 2] == '.' && path[i + 3] == '/') {
            for (int j=i - 1; j>=0; --j) {
                if (path[j] == '/') {
                    memmove(path + j, path + i + 3, len - (i + 2));
                    const int removed = (i + 3 - j);
                    len -= removed;
                    i -= removed;
                    break;
                }
            }
        }
    }
    return len;
}

ByteArray unescape(ByteArray command)
{
    command.replace("\'", "\\'");
    command.prepend("bash --norc -c 'echo -n ");
    command.append('\'');
    // ByteArray cmd = "bash --norc -c 'echo -n " + command + "'";
    FILE *f = popen(command.constData(), "r");
    ByteArray ret;
    char buf[1024];
    do {
        const int read = fread(buf, 1, 1024, f);
        if (read)
            ret += ByteArray(buf, read);
    } while (!feof(f));
    fclose(f);
    return ret;
}

int readLine(FILE *f, char *buf, int max)
{
    assert(!buf == (max == -1));
    if (max == -1)
        max = INT_MAX;
    for (int i=0; i<max; ++i) {
        const int ch = fgetc(f);
        switch (ch) {
        case EOF:
            if (!i)
                i = -1;
            // fall through
        case '\n':
            if (buf)
                *buf = '\0';
            return i;
        }
        if (buf)
            *buf++ = *reinterpret_cast<const char*>(&ch);
    }
    return -1;
}


ByteArray shortOptions(const option *longOptions)
{
    ByteArray ret;
    for (int i=0; longOptions[i].name; ++i) {
        if (ret.contains(longOptions[i].val)) {
            error("%c (%s) is already used", longOptions[i].val, longOptions[i].name);
            assert(!ret.contains(longOptions[i].val));
        }
        ret.append(longOptions[i].val);
        switch (longOptions[i].has_arg) {
        case no_argument:
            break;
        case optional_argument:
            ret.append(':');
            ret.append(':');
            break;
        case required_argument:
            ret.append(':');
            break;
        default:
            assert(0);
            break;
        }
    }
#if 0
    ByteArray unused;
    for (char ch='a'; ch<='z'; ++ch) {
        if (!ret.contains(ch)) {
            unused.append(ch);
        }
        const char upper = toupper(ch);
        if (!ret.contains(upper)) {
            unused.append(upper);
        }
    }
    printf("Unused letters: %s\n", unused.nullTerminated());
#endif
    return ret;
}

void removeDirectory(const Path &path)
{
    DIR *d = opendir(path.constData());
    size_t path_len = path.size();
    char buf[PATH_MAX];
    dirent *dbuf = reinterpret_cast<dirent*>(buf);

    if (d) {
        dirent *p;

        while (!readdir_r(d, dbuf, &p) && p) {
            char *buf;
            size_t len;

            /* Skip the names "." and ".." as we don't want to recurse on them. */
            if (!strcmp(p->d_name, ".") || !strcmp(p->d_name, "..")) {
                continue;
            }

            len = path_len + strlen(p->d_name) + 2;
            buf = static_cast<char*>(malloc(len));

            if (buf) {
                struct stat statbuf;
                snprintf(buf, len, "%s/%s", path.constData(), p->d_name);
                if (!stat(buf, &statbuf)) {
                    if (S_ISDIR(statbuf.st_mode)) {
                        removeDirectory(buf);
                    } else {
                        unlink(buf);
                    }
                }

                free(buf);
            }
        }
        closedir(d);
    }
    rmdir(path.constData());
}
bool startProcess(const Path &dotexe, const List<ByteArray> &dollarArgs)
{
    switch (fork()) {
    case 0:
        break;
    case -1:
        return false;
    default:
        return true;
    }

    if (setsid() < 0)
        _exit(1);


    switch (fork()) {
    case 0:
        break;
    case -1:
        _exit(1);
    default:
        _exit(0);
    }

    int ret = chdir("/");
    if (ret == -1)
        perror("RTags::startProcess() Failed to chdir(\"/\")");

    umask(0);

    const int fdlimit = sysconf(_SC_OPEN_MAX);
    for (int i=0; i<fdlimit; ++i)
        close(i);

    open("/dev/null", O_RDWR);
    ret = dup(0);
    if (ret == -1)
        perror("RTags::startProcess() Failed to duplicate fd");
    ret = dup(0);
    if (ret == -1)
        perror("RTags::startProcess() Failed to duplicate fd");
    char **args = new char*[dollarArgs.size() + 2];
    args[0] = strndup(dotexe.constData(), dotexe.size());
    for (int i=0; i<dollarArgs.size(); ++i) {
        args[i + 1] = strndup(dollarArgs.at(i).constData(), dollarArgs.at(i).size());
    }
    args[dollarArgs.size() + 1] = 0;
    execvp(dotexe.constData(), args);
    FILE *f = fopen("/tmp/failedtolaunch", "w");
    if (f) {
        fwrite(dotexe.constData(), 1, dotexe.size(), f);
        fwrite(" ", 1, 1, f);
        const ByteArray joined = ByteArray::join(dollarArgs, " ");
        fwrite(joined.constData(), 1, joined.size(), f);
        fclose(f);
    }
    _exit(1);
    return false;
}

static Path sApplicationDirPath;
Path applicationDirPath()
{
    return sApplicationDirPath;
}

void findApplicationDirPath(const char *argv0)
{
#if defined(OS_Linux)
    char buf[32];
    const int w = snprintf(buf, sizeof(buf), "/proc/%d/exe", getpid());
    Path p(buf, w);
    if (p.isSymLink()) {
        p.resolve();
        sApplicationDirPath = p;
        return;
    }
#elif defined(OS_Darwin)
    {
        char path[PATH_MAX];
        uint32_t size = sizeof(path);
        if (_NSGetExecutablePath(path, &size) == 0) {
            Path p(path, size);
            if (p.resolve()) {
                assert(p.isFile());
                sApplicationDirPath = p.parentDir();
                assert(sApplicationDirPath.isDir());
                return;
            }
        }
    }
#elif defined(OS_FreeBSD)
    {
        int mib[4] = { CTL_KERN, KERN_PROC, KERN_PROC_PATHNAME, -1 };
        char path[PATH_MAX];
        size_t size = sizeof(path);
        if (!sysctl(mib, 4, path, &size, 0, 0)) {
            Path p(path, size);
            if (p.resolve()) {
                // ### bit of a hack
                assert(p.isFile());
                sApplicationDirPath = p.parentDir();
                assert(sApplicationDirPath.isDir());
                return;
            }
        }
    }
#else
#warning Unknown platform.
#endif
    {
        assert(argv0);
        Path a(argv0);
        if (a.resolve()) {
            sApplicationDirPath = a.parentDir();
            return;
        }
    }
    const char *path = getenv("PATH");
    const List<ByteArray> paths = ByteArray(path).split(':');
    for (int i=0; i<paths.size(); ++i) {
        Path p = (paths.at(i) + "/") + argv0;
        if (p.resolve()) {
            sApplicationDirPath = p.parentDir();
            return;
        }
    }
    fprintf(stderr, "Can't find applicationDirPath");
}

#ifdef HAVE_BACKTRACE
#include <execinfo.h>
#include <cxxabi.h>

static inline char *demangle(const char *str)
{
    if (!str)
        return 0;
    int status;
#ifdef OS_Darwin
    char paren[1024];
    sscanf(str, "%*d %*s %*s %s %*s %*d", paren);
#else
    const char *paren = strchr(str, '(');
    if (!paren) {
        paren = str;
    } else {
        ++paren;
    }
#endif
    size_t l;
    if (const char *plus = strchr(paren, '+')) {
        l = plus - paren;
    } else {
        l = strlen(paren);
    }

    char buf[1024];
    size_t len = sizeof(buf);
    if (l >= len)
        return 0;
    memcpy(buf, paren, l + 1);
    buf[l] = '\0';
    char *ret = abi::__cxa_demangle(buf, 0, 0, &status);
    if (status != 0) {
        if (ret)
            free(ret);
#ifdef OS_Darwin
        return strdup(paren);
#else
        return 0;
#endif
    }
    return ret;
}

ByteArray backtrace(int maxFrames)
{
    enum { SIZE = 1024 };
    void *stack[SIZE];

    int frameCount = backtrace(stack, sizeof(stack) / sizeof(void*));
    if (frameCount <= 0)
        return ByteArray("Couldn't get stack trace");
    ByteArray ret;
    char **symbols = backtrace_symbols(stack, frameCount);
    if (symbols) {
        char frame[1024];
        for (int i=1; i<frameCount && (maxFrames < 0 || i - 1 < maxFrames); ++i) {
            char *demangled = demangle(symbols[i]);
            snprintf(frame, sizeof(frame), "%d/%d %s\n", i, frameCount - 1, demangled ? demangled : symbols[i]);
            ret += frame;
            if (demangled)
                free(demangled);
        }
        free(symbols);
    }
    return ret;
}
#else
ByteArray backtrace(int)
{
    return ByteArray();
}
#endif

void dirtySymbolNames(SymbolNameMap &map, const Set<uint32_t> &dirty)
{
    SymbolNameMap::iterator it = map.begin();
    while (it != map.end()) {
        Set<Location> &locations = it->second;
        Set<Location>::iterator i = locations.begin();
        while (i != locations.end()) {
            if (dirty.contains(i->fileId())) {
                locations.erase(i++);
            } else {
                ++i;
            }
        }
        if (locations.isEmpty()) {
            map.erase(it++);
        } else {
            ++it;
        }
    }
}

void dirtySymbols(SymbolMap &map, const Set<uint32_t> &dirty)
{
    SymbolMap::iterator it = map.begin();
    while (it != map.end()) {
        if (dirty.contains(it->first.fileId())) {
            map.erase(it++);
        } else {
            CursorInfo &cursorInfo = it->second;
            cursorInfo.dirty(dirty);
            ++it;
        }
    }
}
void dirtyUsr(UsrMap &map, const Set<uint32_t> &dirty)
{
    UsrMap::iterator it = map.begin();
    while (it != map.end()) {
        Set<Location> &locations = it->second;
        Set<Location>::iterator i = locations.begin();
        while (i != locations.end()) {
            if (dirty.contains(i->fileId())) {
                locations.erase(i++);
            } else {
                ++i;
            }
        }
        if (locations.isEmpty()) {
            map.erase(it++);
        } else {
            ++it;
        }
    }
}
/* Same behavior as rtags-default-current-project() */

enum FindAncestorFlag {
    Shallow = 0x1,
    Wildcard = 0x2
};
static inline Path findAncestor(Path path, const char *fn, unsigned flags)
{
    Path ret;
    int slash = path.size();
    const int len = strlen(fn) + 1;
    struct stat st;
    char buf[PATH_MAX + sizeof(dirent) + 1];
    dirent *direntBuf = 0, *entry = 0;
    if (flags & Wildcard)
        direntBuf = reinterpret_cast<struct dirent *>(malloc(sizeof(buf)));

    memcpy(buf, path.constData(), path.size() + 1);
    while ((slash = path.lastIndexOf('/', slash - 1)) > 0) { // We don't want to search in /
        if (!(flags & Wildcard)) {
            memcpy(buf + slash + 1, fn, len);
            if (!stat(buf, &st)) {
                buf[slash + 1] = '\0';
                ret = buf;
                if (flags & Shallow) {
                    break;
                }
            }
        } else {
            buf[slash + 1] = '\0';
            DIR *dir = opendir(buf);
            bool found = false;
            if (dir) {
                while (!readdir_r(dir, direntBuf, &entry) && entry) {
                    const int l = strlen(entry->d_name) + 1;
                    switch (l - 1) {
                    case 1:
                        if (entry->d_name[0] == '.')
                            continue;
                        break;
                    case 2:
                        if (entry->d_name[0] == '.' && entry->d_name[1] == '.')
                            continue;
                        break;
                    }
                    assert(buf[slash] == '/');
                    assert(l + slash + 1 < static_cast<int>(sizeof(buf)));
                    memcpy(buf + slash + 1, entry->d_name, l);
                    if (!fnmatch(fn, buf, 0)) {
                        ret = buf;
                        ret.truncate(slash + 1);
                        found = true;
                        break;
                    }
                }
            }
            closedir(dir);
            if (found && flags & Shallow)
                break;
        }
    }
    if (flags & Wildcard)
        free(direntBuf);

    if (!ret.isEmpty() && !ret.endsWith('/'))
        ret.append('/');
    return ret;
}

struct Entry {
    const char *name;
    const unsigned flags;
};

static inline Path checkEntry(const Entry *entries, const Path &path, const Path &home)
{
    for (int i=0; entries[i].name; ++i) {
        const Path p = findAncestor(path, entries[i].name, entries[i].flags);
        if (!p.isEmpty() && p != home) {
            if (!p.compare("./") || !p.compare("."))
                error() << "1" << path << "=>" << p << entries[i].name;
            return p;
        }
    }
    return Path();
}


Path findProjectRoot(const Path &path)
{
    assert(path.isAbsolute());
    static const Path home = Path::home();
    const Entry before[] = {
        { "GTAGS", 0 },
        { "CMakeLists.txt", 0 },
        { "configure", 0 },
        { ".git", 0 },
        { ".svn", 0 },
        { "*.pro", Wildcard },
        { "scons.1", 0 },
        { "*.scons", Wildcard },
        { "SConstruct", 0 },
        { "autogen.*", Wildcard },
        { "GNUMakefile*", Wildcard },
        { "INSTALL*", Wildcard },
        { "README*", Wildcard },
        { 0, 0 }
    };
    {
        const Path ret = checkEntry(before, path, home);
        if (!ret.isEmpty())
            return ret;
    }
    {
        const Path configStatus = findAncestor(path, "config.status", 0);
        if (!configStatus.isEmpty()) {
            FILE *f = fopen((configStatus + "config.status").constData(), "r");
            Path ret;
            if (f) {
                char line[1024];
                enum { MaxLines = 10 };
                for (int i=0; i<MaxLines; ++i) {
                    int r = RTags::readLine(f, line, sizeof(line));
                    if (r == -1)
                        break;
                    char *configure = strstr(line, "/configure");
                    if (configure) {
                        char *end = configure + 10;
                        while (--configure >= line) {
                            const Path conf = Path::resolved(ByteArray(configure, end - configure));
                            if (conf.isFile()) {
                                ret = conf.parentDir();
                                if (ret == home)
                                    ret.clear();
                                break;
                            }
                        }
                    }
                    if (!ret.isEmpty())
                        break;
                }
                fclose(f);
                if (!ret.isEmpty())
                    return ret;
            }
        }
    }
    {
        const Path cmakeCache = findAncestor(path, "CMakeCache.txt", 0);
        if (!cmakeCache.isEmpty()) {
            FILE *f = fopen((cmakeCache + "Makefile").constData(), "r");
            if (f) {
                Path ret;
                char line[1024];
                enum { MaxLines = 10 };
                for (int i=0; i<MaxLines; ++i) {
                    int r = RTags::readLine(f, line, sizeof(line));
                    if (r == -1)
                        break;
                    if (!strncmp(line, "CMAKE_SOURCE_DIR", 16)) {
                        fclose(f);
                        char *dir = line + 16;
                        while (*dir && (*dir == ' ' || *dir == '='))
                            ++dir;
                        if (dir != home) {
                            ret = dir;
                            if (!Path(ret + "/CMakeLists.txt").isFile())
                                ret.clear();
                        }
                        break;
                    }
                }
                fclose(f);
                if (!ret.isEmpty())
                    return ret;
            }
        }
    }
    const Entry after[] = {
        { "Makefile*", Wildcard },
        { 0, 0 }
    };

    {
        const Path ret = checkEntry(after, path, home);
        if (!ret.isEmpty())
            return ret;
    }

    return Path();
}

ByteArray filterPreprocessor(const Path &path)
{
    ByteArray ret;
    FILE *f = fopen(path.constData(), "r");
    if (f) {
        char line[1026];
        int r;
        while ((r = RTags::readLine(f, line, sizeof(line) - 1)) != -1) {
            int start = 0;
            while (start < r && isspace(line[start]))
                ++start;
            if (start == r || line[start] != '#')
                continue;
            line[r] = '\n';
            ret.append(line, r + 1);

            int end = r - 1;
            while (end >= start && isspace(line[end]))
                --end;
            while ((r = RTags::readLine(f, line, sizeof(line) - 1)) != -1) {
                line[r] = '\n';
                ret.append(line, r + 1);
                end = r - 1;
                while (end >= 0 && isspace(line[end]))
                    --end;
                if (end < 0 || line[end] != '\\') {
                    break;
                }
            }
        }

        fclose(f);
    }

    return ret;
}

}

#ifdef RTAGS_DEBUG_MUTEX
void Mutex::lock()
{
    Timer timer;
    while (!tryLock()) {
        usleep(10000);
        if (timer.elapsed() >= 10000) {
            error("Couldn't acquire lock in 10 seconds\n%s", RTags::backtrace().constData());
            timer.restart();
        }
    }
}
#endif
