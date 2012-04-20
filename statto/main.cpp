#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dlfcn.h>

int main(int argc, char **argv)
{
    struct stat64 s64;
    struct stat s;
    stat(argv[0], &s);
    stat64(argv[0], &s64);
    return 0;
}
