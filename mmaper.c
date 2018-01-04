#include <sys/mman.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>
#include <assert.h>

#include "common.h"
#include "mmaper.h"

static void * mmap_addr;
static int g_idx;

#define FILESZ (5L * 1024L * 1024L * 1024L)
//#define FILESZ (30L * 1024L * 1024L * 1024L)
//#define FILESZ (30L * 1024L)

#define CORE_FILTER "0x3f"

int allow_mmap_in_core()
{
    int fd = open("/proc/self/coredump_filter", O_WRONLY);
    if (fd < 0)
        handle_error("open");

    ssize_t written = write(fd, CORE_FILTER, sizeof(CORE_FILTER));
    if (written != sizeof(CORE_FILTER))
        handle_error("write");

    close(fd);
    return 0;
}

static void fill_filename(char* fname, size_t sz, int idx)
{
    snprintf(fname, sz, "/tmp/mmaped_file_%d", idx);
}

void map_remap_private(int idx)
{
    char fname[PATH_MAX];
    fill_filename(fname, sizeof(fname), idx);
    int fd = open(fname, O_CREAT | O_RDWR, S_IWUSR | S_IRUSR);
    if (fd < 0)
        handle_error("open");

    fprintf(stderr, "Starting remap: 0x%p\n", mmap_addr);

    int res = munmap(mmap_addr, FILESZ);
    if (res)
        handle_error("munmap");

    void* new_map = mmap(mmap_addr, FILESZ, PROT_READ, MAP_PRIVATE | MAP_FIXED, fd, 0);
    if (new_map == MAP_FAILED)
        handle_error("mmap private");

    fprintf(stderr, "Ending remap: 0x%p\n", new_map);
    
    assert(new_map == mmap_addr);

    close(fd);
}

int map_memory(int idx, int respawned)
{
    static char template[] = "/tmp/myfileXXXXXX";
    int res;

    g_idx = idx;

    allow_mmap_in_core();

#if 0
    int fd = mkstemp(template);
    if (fd < 0)
        handle_error("mkstemp");
#endif

    char fname[PATH_MAX];
    fill_filename(fname, sizeof(fname), idx);
    int fd = open(fname, O_CREAT | O_RDWR, S_IWUSR | S_IRUSR);
    if (fd < 0)
        handle_error("open");

    res = ftruncate(fd, FILESZ);
    if (res)
        handle_error("ftruncate");

#if 1
    res = posix_fallocate(fd, 0, FILESZ);
    if (res)
        handle_error("posix_fallocate");
#endif

    mmap_addr = mmap(NULL, FILESZ, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (mmap_addr == MAP_FAILED)
        handle_error("mmap");

    close(fd);

    if (!respawned)
        memset(mmap_addr, 'S', FILESZ);
    else
        memset(mmap_addr, 'E', FILESZ);

    return 0;
}

