Btree-source-code/btree2
========================

Here are files in the btree project:

btree2s.c       Single Threaded/MultiProcess version that removes keys all the way back to an original empty btree, placing removed nodes on a free list.  Operates under either memory mapping or file I/O.  Recommended btrees hosted on network file systems.

btree2t.c       Single Threaded/MultiProcess version similar to btree2s except that fcntl locking has been replaced by test & set latches in the first few btree pages.  Uses either memory mapping or file I/O.

btree2u.c		Single Threaded/MultiProcess version that implements a traditional buffer pool manager in the first n pages of the btree file.  The buffer pool accesses its pages with mmap.  Evicted pages are written back to the btree file from the buffer pool pages with pwrite. Recommended for linux.

btree2v.c		Single Threaded/MultiProcess version based on btree2u that utilizes linux only futex calls for latch contention.

threads2i.c     Multi-Threaded with latching implemented by a latch manager with test & set latches in the first few btree pages with thread yield system calls during contention.

threads2j.c     Multi-Threaded with latching implemented by a latch manager with test & set locks in the first few btree pages with Linux futex system calls during contention.

threadskv8.c	Multi-Threaded/Single-Process with atomic-consistent add of a set of keys based on threadskv6.c.  Uses btree page latches as locking granularity.

threadskv10g.c	Multi-Threaded/Multi-Process with 2 Log-Structured-Merge (LSM) btrees based on threadskv8.c. Also adds dual leaf/interior node page sizes for each btree. Includes an experimental recovery redo log. This file is linux only.

threadskv10h.c	Multi-Threaded/Multi-Process with 2 Log-Structured-Merge (LSM) btrees based on threadskv8.c. Also adds dual leaf/interior node page sizes for each btree. This file is linux only.

Compilation is achieved on linux or Windows by:

gcc -D STANDALONE -O3 btree2*.c

or

cl /Ox /D STANDALONE btree2*.c
