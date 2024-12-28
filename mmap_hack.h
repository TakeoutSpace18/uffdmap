#ifndef MMAP_HACK_H
#define MMAP_HACK_H

#include <stdlib.h>

/* Map anonymous read-wrieable memory region without size constraint.
 * Linux does not allow to map writeable memory region bigger than RAM size.
 * Works by continiously allocating smaller chunks on fixed addressed.*/
void *hacked_anon_rw_mmap(size_t size);

#endif /* MMAP_HACK_H */
