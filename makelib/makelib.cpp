#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dlfcn.h>

struct StatData
{
    const char* data;
    int len;
};
static StatData statData[] = {
    { ".o",       2 },
    { ".lo",      3 },
    { ".gch/c++", 8 },
    { ".gch/c",   6 },
    { 0,          0 }
};

typedef int (*XStat64)(int, const char*, struct stat64*);
typedef int (*Stat)(const char*, struct stat*);
typedef int (*Stat64)(const char*, struct stat64*);
template <typename T>
int sharedStat(int ret, const char *filename, T *stat_buf)
{
    printf("yo yo yo %s\n", filename);
    if (!ret && S_ISREG(stat_buf->st_mode)) {
        const int len = strlen(filename);
        bool changed = false;
        for (StatData* current = statData; current->data; ++current) {
            const int& currentLen = current->len;
            if (len >= currentLen && !strncmp(filename + len - currentLen, current->data, currentLen))  {
                stat_buf->st_mtime = 1;
                changed = true;
                break;
            }
        }
        static bool debug = getenv("DEBUG_STAT");
        if (debug) {
            FILE* logfile = fopen("/tmp/makelib.log", "a");
            if (logfile) {
                fprintf(logfile, "stated [%s]%s\n", filename, changed ? " changed" : "");
                fclose(logfile);
            }
        }
    }
    return ret;
}

int __xstat64(int ver, const char *filename, struct stat64 *stat_buf)
{
    printf("[%s] %s:%d: int __xstat64(int ver, const char *filename, struct stat64 *stat_buf)\n", __func__, __FILE__, __LINE__);
    static XStat64 realStat = 0;
    if (!realStat)
        realStat = reinterpret_cast<XStat64>(dlsym(RTLD_NEXT, "__xstat64"));
    return sharedStat(realStat(ver, filename, stat_buf), filename, stat_buf);
}
int stat(const char *filename, struct stat *stat_buf)
{
    printf("[%s] %s:%d: int stat(const char *filename, struct stat *stat_buf)\n", __func__, __FILE__, __LINE__);
    static Stat realStat = 0;
    if (!realStat)
        realStat = reinterpret_cast<Stat>(dlsym(RTLD_NEXT, "stat"));
    return sharedStat(realStat(filename, stat_buf), filename, stat_buf);
}

int stat64(const char *filename, struct stat64 *stat_buf)
{
    printf("[%s] %s:%d: int stat64(const char *filename, struct stat64 *stat_buf)\n", __func__, __FILE__, __LINE__);
    static Stat64 realStat = 0;
    if (!realStat)
        realStat = reinterpret_cast<Stat64>(dlsym(RTLD_NEXT, "stat64"));
    return sharedStat(realStat(filename, stat_buf), filename, stat_buf);
}

