#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

#include "uffdmap.h"

#define OK 0
#define ERROR -1

const char *testfile_name = "tmpfile";

static int genfile(size_t size)
{
    FILE *f = fopen(testfile_name, "wb");
    if (!f) {
        printf("Can't create test file\n");
        return ERROR;
    }
    
    size_t nr_words = size / sizeof(size_t);

    for (size_t i = 0; i < nr_words; ++i) {
        if (fwrite(&i, sizeof(i), 1, f) != 1) {
            printf("Can't write word to test file\n");
            return ERROR;
        }
    }

    fclose(f);
    return OK;
}

int run_test(size_t file_size)
{
    printf("Running test with file_size: %zu\n", file_size);
    int ret = genfile(file_size);
    if (ret != OK)
        return ret;

    int fd = open(testfile_name, O_RDONLY | O_DIRECT);
    if (fd < 0) {
        printf("Can't open test file\n");
        return ERROR;
    }

    void *mapped = uffdmap(fd);
    if (mapped == UFFDMAP_FAILED) {
        printf("Can't uffdmap test file\n");
        close(fd);
        return ERROR;
    }

    size_t nr_words = file_size / sizeof(size_t);
    for (size_t i = 0; i < nr_words; ++i) {
        if (((size_t *) mapped)[i] != i) {
            printf("Word %zi didn't match\n", i);
            uffdunmap(mapped);
            close(fd);
            return ERROR;
        }
    }

    uffdunmap(mapped);
    close(fd);
    printf("all words matched\n");
    return OK;
}

int main(int argc, char **argv)
{
    for (size_t file_size = 8; file_size < 100 * 1024 * 2024; file_size *= 2) {
        if (run_test(file_size) != OK) {
            break;
        }
    }

    unlink(testfile_name);

    return EXIT_SUCCESS;
}
