#ifndef URING_CONTEXT_H
#define URING_CONTEXT_H

#include <stddef.h>
#include <sys/types.h>
#include <linux/io_uring.h>

// TODO: understand this better, I got from the Arch Wiki example
#define io_uring_smp_store_release(p, v)                                  \
    atomic_store_explicit((_Atomic typeof(*(p)) *)(p), (v), memory_order_release)
#define io_uring_smp_load_acquire(p)                                      \
    atomic_load_explicit((_Atomic typeof(*(p)) *)(p), memory_order_acquire)

extern char  buff[1024];
extern off_t offset;

// init uring ctx in polling mode
int app_setup_uring(void);

// read completion
int read_from_cq(void);

// submit io request (used by uring_fprintf)
int submit_to_sq(int fd, int op, size_t len, off_t off);

#endif // URING_CONTEXT_H