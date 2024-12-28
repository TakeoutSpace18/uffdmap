#include "mmap_hack.h"

#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/mman.h>
#include <sys/sysinfo.h>

static size_t get_total_ram(void)
{
    struct sysinfo info;
    sysinfo(&info);

    return info.totalram;
}

static void map_chunk(void *addr, size_t size)
{
    int flags = MAP_PRIVATE | MAP_ANON | MAP_FIXED_NOREPLACE;
    void *ret = mmap(addr, size, PROT_READ | PROT_WRITE, flags, -1, 0);
    if (ret == MAP_FAILED) {
        printf("[addr: %p, size: %zi] map chunk failed: %s\n", addr, size,
               strerror(errno));
        abort();
    }
}

static void *get_mappable_address(size_t size)
{
    void *addr = mmap(NULL, size, PROT_READ, MAP_PRIVATE | MAP_ANON, -1, 0);
    if (addr == MAP_FAILED) {
        printf("get_mappable_address() mmap failed: %s\n", strerror(errno));
        abort();
    }
    munmap(addr, size);

    return addr;
}

static void map_sequence(char *start, size_t size)
{
    const size_t ram = get_total_ram();
    const size_t chunk_size = ram - (ram % sysconf(_SC_PAGESIZE));
    const size_t nr_chunks = size / chunk_size;

    for (size_t i = 0; i < nr_chunks; ++i) {
        char *addr = start + chunk_size * i;
        map_chunk(addr, chunk_size);
    }

    map_chunk(start + nr_chunks * chunk_size, size - nr_chunks * chunk_size);
}

void *hacked_anon_rw_mmap(size_t size)
{
    void *start = get_mappable_address(size);
    map_sequence(start, size);
    return start;
}
