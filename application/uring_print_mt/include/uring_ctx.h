#ifndef URING_CONTEXT_H
#define URING_CONTEXT_H

#include <stddef.h>
#include <sys/types.h>
#include <linux/io_uring.h>
#include <stdatomic.h>

// Helpers from the Arch Wiki example
#define io_uring_smp_store_release(p, v)                                  \
    atomic_store_explicit((_Atomic typeof(*(p)) *)(p), (v), memory_order_release)
#define io_uring_smp_load_acquire(p)                                      \
    atomic_load_explicit((_Atomic typeof(*(p)) *)(p), memory_order_acquire)

typedef struct uring_ctx {
    int                  ring_fd;
    unsigned            *sring_head;
    unsigned            *sring_tail;
    unsigned            *sring_mask;
    unsigned            *sring_array;
    unsigned            *cring_head;
    unsigned            *cring_tail;
    unsigned            *cring_mask;
    struct io_uring_sqe *sqes;
    struct io_uring_cqe *cqes;
} uring_ctx_t;

int setup_uring(uring_ctx_t *ctx);
void uring_perror(uring_ctx_t *ctx, const char *msg, size_t msg_len);

#endif // URING_CONTEXT_H
