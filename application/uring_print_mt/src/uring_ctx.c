#include "uring_ctx.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <liburing.h>
#include <limits.h>


/*
 * Initialize io_uring in polling mode
   and mmap the SQ/CQ rings and SQE array.
 */
int setup_uring(uring_ctx_t *ctx) {
  struct io_uring_params p;
  void *sq_ptr, *cq_ptr;
  int sring_sz, cring_sz;

  memset(&p, 0, sizeof(p));
  p.flags = IORING_SETUP_SQPOLL;
  p.sq_thread_idle = UINT_MAX/1000; // in micro seconds
  // p.sq_thread_idle = 0; // in micro seconds

  ctx->ring_fd = io_uring_setup(QUEUE_DEPTH, &p);

  // register stderr
  int fds[] = {STDERR_FILENO}; // STDERR_FILENO == 2
  int ret = io_uring_register(ctx->ring_fd,
                            IORING_REGISTER_FILES,
                            fds, /* pointer to your array */
                            1);  /* number of entries */
  assert(ret == 0);
  assert(ctx->ring_fd >= 0);

  /* Compute how many bytes we need to mmap for SQ ring and CQ ring. */
  sring_sz = p.sq_off.array + p.sq_entries * sizeof(unsigned);
  cring_sz = p.cq_off.cqes + p.cq_entries * sizeof(struct io_uring_cqe);

  /* If the kernel supports a single mmap for both SQ and CQ… */
  if (p.features & IORING_FEAT_SINGLE_MMAP) {
    if (cring_sz > sring_sz)
      sring_sz = cring_sz;
    cring_sz = sring_sz;
  }

  /* Map the submission ring buffer. */
  sq_ptr = mmap(NULL, sring_sz, PROT_READ | PROT_WRITE,
                MAP_SHARED | MAP_POPULATE, ctx->ring_fd, IORING_OFF_SQ_RING);
  assert(sq_ptr != MAP_FAILED);

  /* Map the completion ring buffer (or reuse the same if SINGLE_MMAP). */
  if (p.features & IORING_FEAT_SINGLE_MMAP) {
    cq_ptr = sq_ptr;
  } else {
    cq_ptr = mmap(NULL, cring_sz, PROT_READ | PROT_WRITE,
                  MAP_SHARED | MAP_POPULATE, ctx->ring_fd, IORING_OFF_CQ_RING);
    assert(cq_ptr != MAP_FAILED);
  }

  /* Grab pointers to SQ ring head/tail/mask/array fields. */
  ctx->sring_head = (unsigned *)((char *)sq_ptr + p.sq_off.head);
  ctx->sring_tail = (unsigned *)((char *)sq_ptr + p.sq_off.tail);
  ctx->sring_mask = (unsigned *)((char *)sq_ptr + p.sq_off.ring_mask);
  ctx->sring_array = (unsigned *)((char *)sq_ptr + p.sq_off.array);

  /* Map the actual SQE array. */
  ctx->sqes = mmap(NULL, p.sq_entries * sizeof(struct io_uring_sqe),
                   PROT_READ | PROT_WRITE, MAP_SHARED | MAP_POPULATE,
                   ctx->ring_fd, IORING_OFF_SQES);
  assert(ctx->sqes != MAP_FAILED);

  /* Grab pointers to CQ ring head/tail/mask fields and the CQE array. */
  ctx->cring_head = (unsigned *)((char *)cq_ptr + p.cq_off.head);
  ctx->cring_tail = (unsigned *)((char *)cq_ptr + p.cq_off.tail);
  ctx->cring_mask = (unsigned *)((char *)cq_ptr + p.cq_off.ring_mask);
  ctx->cqes = (struct io_uring_cqe *)((char *)cq_ptr + p.cq_off.cqes);

  io_uring_enter(ctx->ring_fd, 0, 0, IORING_ENTER_SQ_WAKEUP);

  return 0;
}

// async submit print request to SQ
void uring_perror(uring_ctx_t *ctx, const char *msg, size_t msg_len) {
  unsigned tail = *ctx->sring_tail;
  unsigned idx = tail & *ctx->sring_mask; // TODO: check if not full
  struct io_uring_sqe *sqe = &ctx->sqes[idx];
  // char buff[256] = {0};
  char *buff = (char *)malloc(256); // todo: do not use malloc. this is dangling
  memset(buff, 0, 256);
  assert(msg_len < 256);
  memcpy(buff, msg, msg_len);

  /* prepare SQE for stderr write */
  memset(sqe, 0, sizeof(*sqe));
  sqe->opcode = IORING_OP_WRITE;
  // sqe->fd = STDERR_FILENO;
  sqe->fd = 0; // idx in registered buffer
  sqe->flags = IOSQE_FIXED_FILE; // use registered file descriptor
  sqe->addr = (unsigned long)buff;
  sqe->len = msg_len;
  sqe->off = -1;
  sqe->user_data = (uintptr_t) buff; // for later free

  /* publish and advance the submission ring */
  ctx->sring_array[idx] = idx; // must happen before the store_release
  tail++;
  io_uring_smp_store_release(ctx->sring_tail, tail);
}