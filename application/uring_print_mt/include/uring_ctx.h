#ifndef URING_CONTEXT_H
#define URING_CONTEXT_H

#include <stddef.h>
#include <sys/types.h>
#include <linux/io_uring.h>
#include <sys/syscall.h>
#include <liburing.h>
#include <stdatomic.h>
#include <unistd.h>

/* ***********************************
   Helpers from the Arch Wiki example
   - raw syscall wrappers, in case glibc does not have it
   - atomic operations for memory barriers
   **********************************/
// #define io_uring_smp_store_release(p, v)                                  \
//     atomic_store_explicit((_Atomic typeof(*(p)) *)(p), (v), memory_order_release)
// #define io_uring_smp_load_acquire(p)                                      \
//     atomic_load_explicit((_Atomic typeof(*(p)) *)(p), memory_order_acquire)

static int io_uring_setup(unsigned entries, struct io_uring_params *p) {
  return (int)syscall(__NR_io_uring_setup, entries, p);
}
static int io_uring_enter(int ring_fd, unsigned to_submit,
                          unsigned min_complete, unsigned flags) {
  return (int)syscall(__NR_io_uring_enter, ring_fd, to_submit, min_complete,
                      flags, NULL, 0);
}

// extern int io_uring_register(unsigned fd,
//                              unsigned opcode,
//                              const void *arg,
//                              unsigned nr_args);




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
