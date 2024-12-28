#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>

#include "uffdmap.h"

static size_t fd_getsize(int fd)
{
    struct stat stat;
    if (fstat(fd, &stat) == -1) {
        printf("stat() failed: %s\n", strerror(errno));
        abort();
    }

    return stat.st_size;
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        printf("usage: uffdmap-cat <pathname>\n");
        return EXIT_FAILURE;
    }

    int fd = open(argv[1], O_RDONLY | O_DIRECT);
    if (fd < 0) {
        printf("Can't open provided file\n");
        return EXIT_FAILURE;
    }

    size_t sz = fd_getsize(fd);

    void *mapped = uffdmap(fd);
    if (mapped == UFFDMAP_FAILED) {
        printf("Can't uffdmap file\n");
        return EXIT_FAILURE;
    }

    for (size_t i = 0; i < sz; ++i) {
        char c = ((char *) mapped)[i];
        fputc(c, stdout);
    }

    uffdunmap(mapped);

    return EXIT_SUCCESS;
}
