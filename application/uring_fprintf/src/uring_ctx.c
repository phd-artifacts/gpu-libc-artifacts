#include "uring_ctx.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdatomic.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <errno.h>

// todo understang, got from the Arch Wiki example
#define QUEUE_DEPTH 1
#define BLOCK_SZ   1024

//todo: reafactor global to static class for uring ctx
int                   ring_fd;
unsigned             *sring_tail, *sring_mask, *sring_array;
unsigned             *cring_head, *cring_tail, *cring_mask;
struct io_uring_sqe  *sqes; // submission queue
struct io_uring_cqe  *cqes; // completion queue
char                  buff[BLOCK_SZ];
off_t                 offset = 0; // todo: not hardcode offset

/*  Todo: clean this up. From arch wiki example:
 * Raw syscall wrappers (glibc may not have them yet).
 */
static int io_uring_setup(unsigned entries, struct io_uring_params *p) {
    return (int) syscall(__NR_io_uring_setup, entries, p);
}
static int io_uring_enter(int ring_fd,
                          unsigned to_submit,
                          unsigned min_complete,
                          unsigned flags) {
    return (int) syscall(__NR_io_uring_enter,
                        ring_fd, to_submit, min_complete, flags, NULL, 0);
}

/* 
 * Initialize io_uring in polling mode, then mmap the SQ/CQ rings and SQE array.
 */
int app_setup_uring(void) {
    struct io_uring_params p;
    void *sq_ptr, *cq_ptr;
    int   sring_sz, cring_sz;

    memset(&p, 0, sizeof(p));
    ring_fd = io_uring_setup(QUEUE_DEPTH, &p);
    if (ring_fd < 0) {
        perror("io_uring_setup");
        return -1;
    }

    /* Compute how many bytes we need to mmap for SQ ring and CQ ring. */
    sring_sz  = p.sq_off.array + p.sq_entries * sizeof(unsigned);
    cring_sz  = p.cq_off.cqes  + p.cq_entries * sizeof(struct io_uring_cqe);

    /* If the kernel supports a single mmap for both SQ and CQ… */
    if (p.features & IORING_FEAT_SINGLE_MMAP) {
        if (cring_sz > sring_sz)
            sring_sz = cring_sz;
        cring_sz = sring_sz;
    }

    /* Map the submission ring buffer. */
    sq_ptr = mmap(NULL, sring_sz,
                  PROT_READ | PROT_WRITE,
                  MAP_SHARED | MAP_POPULATE,
                  ring_fd, IORING_OFF_SQ_RING);
    if (sq_ptr == MAP_FAILED) {
        perror("mmap(SQ ring)");
        return -1;
    }

    /* Map the completion ring buffer (or reuse the same if SINGLE_MMAP). */
    if (p.features & IORING_FEAT_SINGLE_MMAP) {
        cq_ptr = sq_ptr;
    } else {
        cq_ptr = mmap(NULL, cring_sz,
                      PROT_READ | PROT_WRITE,
                      MAP_SHARED | MAP_POPULATE,
                      ring_fd, IORING_OFF_CQ_RING);
        if (cq_ptr == MAP_FAILED) {
            perror("mmap(CQ ring)");
            return -1;
        }
    }

    /* Grab pointers to SQ ring head/tail/mask/array fields. */
    sring_tail  = (unsigned *)((char *)sq_ptr + p.sq_off.tail);
    sring_mask  = (unsigned *)((char *)sq_ptr + p.sq_off.ring_mask);
    sring_array = (unsigned *)((char *)sq_ptr + p.sq_off.array);

    /* Map the actual SQE array. */
    sqes = mmap(NULL,
                p.sq_entries * sizeof(struct io_uring_sqe),
                PROT_READ | PROT_WRITE,
                MAP_SHARED | MAP_POPULATE,
                ring_fd, IORING_OFF_SQES);
    if (sqes == MAP_FAILED) {
        perror("mmap(SQEs)");
        return -1;
    }

    /* Grab pointers to CQ ring head/tail/mask fields and the CQE array. */
    cring_head = (unsigned *)((char *)cq_ptr + p.cq_off.head);
    cring_tail = (unsigned *)((char *)cq_ptr + p.cq_off.tail);
    cring_mask = (unsigned *)((char *)cq_ptr + p.cq_off.ring_mask);
    cqes       = (struct io_uring_cqe *)((char *)cq_ptr + p.cq_off.cqes);

    return 0;
}

/* TODO: understand this better, came from Arch Wiki example
* Read from completion queue.
* In this function, we read completion events from the completion queue.
* We dequeue the CQE, update and head and return the result of the operation.
* */
int read_from_cq(void) {
    unsigned head = io_uring_smp_load_acquire(cring_head);
    if (head == *cring_tail)
        return -1;  /* no completions yet */

    struct io_uring_cqe *cqe = &cqes[head & (*cring_mask)];
    if (cqe->res < 0) {
        fprintf(stderr, "I/O error: %s\n", strerror(-cqe->res));
    }
    head++;
    io_uring_smp_store_release(cring_head, head);
    return cqe->res;
}

// submit I/O request
int submit_to_sq(int fd, int op, size_t len, off_t off) {
    unsigned tail = *sring_tail;
    unsigned idx  = tail & *sring_mask;
    struct io_uring_sqe *sqe = &sqes[idx];

    memset(sqe, 0, sizeof(*sqe));
    sqe->opcode = op; // IORING_OP_WRITE or IORING_OP_READ
    sqe->fd     = fd; // the file descriptor
    sqe->addr   = (unsigned long) buff;
    sqe->len    = len; // buffer len
    sqe->off    = off; // file offset

    /* Publish this SQE index in the ring array. */
    sring_array[idx] = idx;
    tail++;
    io_uring_smp_store_release(sring_tail, tail);

    /* Wake the kernel thread and wait for at least one completion. */
    int ret = io_uring_enter(ring_fd, 1, 1, IORING_ENTER_GETEVENTS);
    if (ret < 0) {
        perror("io_uring_enter");
        return -1;
    }
    return 0;
}
