#ifndef UFFDMAP_H
#define UFFDMAP_H

#define UFFDMAP_FAILED ((void *) -1)

/* Use user-space page fault handling technique for loading file pages to memory.
 * io_uring with i/o polling is used to read file.
 * fd must be opened with O_DIRECT for polling to work.
 *
 * Constraints:
 *  - mapped memory region is writeable, but saving changes
 *  to file is not implemented.
 *  - pages are not flushed back to disk, so application can get
 *  killed by OOM if the file is bigger than available RAM & swap 
 *  - only one file can be mapped at a time */

void *uffdmap(int fd);
void uffdunmap(void *addr);

#endif /* UFFDMAP_H */
