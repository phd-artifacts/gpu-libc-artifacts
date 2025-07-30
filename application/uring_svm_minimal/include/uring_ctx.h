#ifndef URING_CONTEXT_H
#define URING_CONTEXT_H

#include <stddef.h>
#include <sys/types.h>
#include <linux/io_uring.h>
#include <sys/syscall.h>
#include <liburing/barrier.h>
#include <atomic>
#include <unistd.h>

/* ***********************************
   Helpers from the Arch Wiki example
   - raw syscall wrappers, in case glibc does not have it
   - atomic operations for memory barriers
   **********************************/

static int io_uring_setup(unsigned entries, struct io_uring_params *p) {
  return (int)syscall(__NR_io_uring_setup, entries, p);
}
static int io_uring_enter(int ring_fd, unsigned to_submit,
                          unsigned min_complete, unsigned flags) {
  return (int)syscall(__NR_io_uring_enter, ring_fd, to_submit, min_complete,
                      flags, NULL, 0);
}

static inline int io_uring_register(unsigned int fd,
                                    unsigned int opcode,
                                    const void *arg,
                                    unsigned int nr_args) {
    return (int)syscall(__NR_io_uring_register, fd, opcode, arg, nr_args);
}

#define QUEUE_DEPTH 12
#define MSG_BUF_SIZE 256

typedef struct uring_ctx {
    void                *sq_ring_ptr;
    int                  sq_ring_sz;
    void                *cq_ring_ptr;
    int                  cq_ring_sz;
    /* device accessible addresses */
    void                *sring_head_dev;
    void                *sring_tail_dev;
    void                *sring_mask_dev;
    void                *sring_array_dev;
    struct io_uring_sqe *sqes_dev;
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
    char                *msg_pool;      /* host pointer */
    void                *msg_pool_dev;  /* device view */
    std::atomic_uint     sq_tail_cache;
} uring_ctx_t;

int setup_uring(uring_ctx_t *ctx);
void uring_perror(uring_ctx_t *ctx, const char *msg, size_t msg_len);
void uring_process_completions(uring_ctx_t *ctx);
void teardown_uring(uring_ctx_t *ctx);
void uring_flush(uring_ctx_t *ctx);

#endif // URING_CONTEXT_H
