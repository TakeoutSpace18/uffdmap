## Experiment with user-space page fault handling and I/O polling

This is an attempt to make I/O polling to work with memory mapped files, thus making the reads faster with modern low-latency SSDs.
Userfaultfd is used to intercept page faults in anonymous memory region and load file pages into it. Loading is done via io_uring interface, because it is currently the only way in Linux to use I/O polling.

Constraints:
- mapped memory region is writeable, but saving changes
to file is not implemented.
- pages are not flushed back to disk, so application can get
killed by OOM if the file is bigger than available RAM & swap 
- only one file can be mapped at a time

#### Build 
```sh
meson setup build
meson compile -C build
```
