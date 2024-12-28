#define _GNU_SOURCE
/* Use userfaultfd for demand-paging, highly inspired by article - 
 * https://xzpeter.org/userfaultfd-wp-latency-measurements/*/

#include "mmap_hack.h"
#include "uffdmap.h"

#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <stdio.h>

#include <sys/mman.h>
#include <sys/syscall.h>
#include <sys/ioctl.h>

#include <linux/userfaultfd.h>
#include <liburing.h>

#define OK 0
#define ERROR -1

#define URING_QUEUE_DEPTH 1

struct uffdmap_data
{
    int file_fd;
    size_t file_size;
    int uffd;

    void *map_addr;
    size_t map_size;

    uint64_t map_start;
    uint64_t map_end;

    size_t page_size;

    struct io_uring ring;
};

struct uffdmap_data g_data = (struct uffdmap_data) { 0 };

static void load_page_from_file(uint64_t page_addr)
{
    off_t file_offset = page_addr - g_data.map_start;

    /* read is made directly into mapped region,
     * but page should be zeroed beforehand */
    struct uffdio_range range = {
        .start = page_addr,
        .len = g_data.page_size
    };

    struct uffdio_zeropage arg = {
        .range = range,
        .mode = UFFDIO_ZEROPAGE_MODE_DONTWAKE
    };

    if (ioctl(g_data.uffd, UFFDIO_ZEROPAGE, &arg) == -1) {
        printf("ioctl(UFFDIO_ZEROPAGE) failed: %s\n", strerror(errno));
        abort();
    }

    struct iovec iovec = {
        .iov_base = (void *) page_addr,
        .iov_len = g_data.page_size
    };

    struct io_uring_sqe *sqe = io_uring_get_sqe(&g_data.ring);
    io_uring_prep_readv2(sqe, g_data.file_fd, &iovec, 1, file_offset, RWF_HIPRI);

    /* set user data to ensure we wait for the right uring CQE */
    io_uring_sqe_set_data(sqe, (void *) page_addr);

    io_uring_submit(&g_data.ring);

    struct io_uring_cqe *cqe;
    int ret = io_uring_wait_cqe(&g_data.ring, &cqe);
    if (ret < 0) {
        perror("io_uring_wait_cqe");
        abort();
    }
    if (cqe->res < 0) {
        printf("Async readv failed: %s\n", strerror(-cqe->res));
        abort();
    }

    if (io_uring_cqe_get_data(cqe) != (void *) page_addr) {
        printf("Waited for wrong CQE\n");
        abort();
    }

    io_uring_cqe_seen(&g_data.ring, cqe);

    if (ioctl(g_data.uffd, UFFDIO_WAKE, &range) == -1) {
        printf("ioctl(UFFDIO_WAKE) failed: %s\n", strerror(errno));
        abort();
    }
}

static void zero_page(uint64_t page_number)
{
    struct uffdio_range range = {
        .start = g_data.map_start + page_number * g_data.page_size,
        .len = g_data.page_size
    };

    struct uffdio_zeropage arg = {
        .mode = 0,
        .range = range
    };

    if (ioctl(g_data.uffd, UFFDIO_ZEROPAGE, &arg) == -1) {
        printf("ioctl(UFFDIO_ZEROPAGE) failed: %s\n", strerror(errno));
        return;
    }
}

static void sigbus_handler(int code, siginfo_t *siginfo, void *ucontext)
{
    uint64_t addr = (uint64_t) siginfo->si_addr;
    if (addr < g_data.map_start || g_data.map_end <= addr) {
        printf("Received SIGBUS outside of mapped range\n");
        return;
    }

    uint64_t page_addr = addr - (addr % g_data.page_size);

    load_page_from_file(page_addr);
}

static int register_sigbus_handler(void)
{
    struct sigaction action;

    action.sa_flags = SA_RESTART;
    action.sa_handler = NULL;
    action.sa_sigaction = sigbus_handler;
    action.sa_flags |= SA_SIGINFO;
    sigemptyset(&action.sa_mask);

    int ret = sigaction(SIGBUS, &action, NULL);
    if (ret != 0)
    {
        printf("sigaction() failed: %s\n\n", strerror(errno));
        return ERROR;
    }

    return OK;
}

static int setup_uffd(void *mapped, size_t sz, int *uffd)
{
    if ((*uffd = syscall(SYS_userfaultfd, O_NONBLOCK)) == -1)
    {
        printf("userfaultfd(): Initial syscall failed!, %s\n", strerror(errno));
        return ERROR;
    }

    struct uffdio_api api = {
        .api = UFFD_API,
        .features = UFFD_FEATURE_SIGBUS
    };

    if (ioctl(*uffd, UFFDIO_API, &api) == -1) {
        printf("ioctl(UFFDIO_API) failed: %s\n", strerror(errno));
        return ERROR;
    }

    struct uffdio_range range = {
        .start = (uint64_t) mapped,
        .len = sz
    };

    struct uffdio_register reg = { 
        .range = range,
        .mode = UFFDIO_REGISTER_MODE_MISSING,
        .ioctls = 0
    };

    if (ioctl(*uffd, UFFDIO_REGISTER, &reg) == -1) {
        printf("ioctl(UFFDIO_REGISTER) failed: %s\n", strerror(errno));
        return ERROR;
    }

    return OK;
}

static void unregister_uffd(int uffd, void *mapped, size_t sz)
{
    
    struct uffdio_range range = {
        .start = (uint64_t) mapped,
        .len = sz
    };

    if (ioctl(uffd, UFFDIO_UNREGISTER, &range) == -1) {
        printf("ioctl(UFFDIO_UNREGISTER) failed: %s\n", strerror(errno));
    }
}

static size_t fd_getsize(int fd)
{
    struct stat stat;
    if (fstat(fd, &stat) == -1) {
        perror("fstat");
        abort();
    }

    return stat.st_size;
}

void *uffdmap(int fd)
{
    struct uffdmap_data d = { 0 };

    d.page_size = sysconf(_SC_PAGESIZE);
    d.file_size = fd_getsize(fd);
    d.file_fd = fd;

    /* map size should be multiple of page size */
    d.map_size = (d.file_size + d.page_size - 1) / d.page_size * d.page_size;

    /* usual mmap will not allocate writeable region
     * that is bigger than ram */
    d.map_addr = hacked_anon_rw_mmap(d.map_size);

    int err = setup_uffd(d.map_addr, d.map_size, &d.uffd);
    if (err != OK) {
        abort();
    }

    if (register_sigbus_handler() != OK) {
        abort();
    }

    d.map_start = (uint64_t) d.map_addr;
    d.map_end = d.map_start + d.map_size;

    int ret = io_uring_queue_init(URING_QUEUE_DEPTH, &d.ring, IORING_SETUP_IOPOLL);
    if (ret < 0) {
        printf("init io uring failed: %s\n", strerror(-ret));
        abort();
    }

    memcpy(&g_data, &d, sizeof(struct uffdmap_data));

    return d.map_addr;
}

void uffdunmap(void *addr)
{
    struct uffdmap_data *d = &g_data;

    unregister_uffd(d->uffd, d->map_addr, d->map_size);
    munmap(d->map_addr, d->map_size);
    io_uring_queue_exit(&d->ring);
}
