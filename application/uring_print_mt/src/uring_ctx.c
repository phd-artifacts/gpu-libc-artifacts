#include "uring_ctx.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/uio.h>
#include <sys/mman.h>
#include <liburing/barrier.h>
#include <limits.h>
#include <hsa/hsa.h>
#include <hsa/hsa_ext_amd.h>
#include <omp.h>


/*
 * Initialize io_uring in polling mode
   and mmap the SQ/CQ rings and SQE array.
 */
int setup_uring(uring_ctx_t *ctx) {
  hsa_status_t hsa_status = hsa_init();
  assert(hsa_status == HSA_STATUS_SUCCESS);

  struct io_uring_params p;
  void *sq_ptr, *cq_ptr;
  int sring_sz, cring_sz;

  memset(&p, 0, sizeof(p));
  p.flags = IORING_SETUP_SQPOLL;
  p.sq_thread_idle = UINT_MAX/1000; // in micro seconds TODO: check what the kernel does with this
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
  ctx->sq_ring_ptr = sq_ptr;
  ctx->sq_ring_sz = sring_sz;
  void *sq_agent_ptr = NULL;
  hsa_status = hsa_amd_memory_lock(sq_ptr, sring_sz, NULL, 0, &sq_agent_ptr);
  assert(hsa_status == HSA_STATUS_SUCCESS);
  ctx->sring_head_dev = (char *)sq_agent_ptr + p.sq_off.head;
  ctx->sring_tail_dev = (char *)sq_agent_ptr + p.sq_off.tail;
  ctx->sring_mask_dev = (char *)sq_agent_ptr + p.sq_off.ring_mask;
  ctx->sring_array_dev = (char *)sq_agent_ptr + p.sq_off.array;

  /* Map the completion ring buffer (or reuse the same if SINGLE_MMAP). */
  if (p.features & IORING_FEAT_SINGLE_MMAP) {
    cq_ptr = sq_ptr;
  } else {
    cq_ptr = mmap(NULL, cring_sz, PROT_READ | PROT_WRITE,
                  MAP_SHARED | MAP_POPULATE, ctx->ring_fd, IORING_OFF_CQ_RING);
    assert(cq_ptr != MAP_FAILED);
  }
  ctx->cq_ring_ptr = cq_ptr;
  ctx->cq_ring_sz = cring_sz;
  void *cq_agent_ptr = NULL;
  hsa_status = hsa_amd_memory_lock(cq_ptr, cring_sz, NULL, 0, &cq_agent_ptr);
  assert(hsa_status == HSA_STATUS_SUCCESS);

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
  void *sqe_agent_ptr = NULL;
  hsa_status = hsa_amd_memory_lock(ctx->sqes,
                                   p.sq_entries * sizeof(struct io_uring_sqe),
                                   NULL, 0, &sqe_agent_ptr);
  assert(hsa_status == HSA_STATUS_SUCCESS);
  ctx->sqes_dev = sqe_agent_ptr;

  /* Grab pointers to CQ ring head/tail/mask fields and the CQE array. */
  ctx->cring_head = (unsigned *)((char *)cq_ptr + p.cq_off.head);
  ctx->cring_tail = (unsigned *)((char *)cq_ptr + p.cq_off.tail);
  ctx->cring_mask = (unsigned *)((char *)cq_ptr + p.cq_off.ring_mask);
  ctx->cqes = (struct io_uring_cqe *)((char *)cq_ptr + p.cq_off.cqes);

  io_uring_enter(ctx->ring_fd, 0, 0, IORING_ENTER_SQ_WAKEUP);

  size_t pool_bytes = QUEUE_DEPTH * MSG_BUF_SIZE;
  ctx->msg_pool = aligned_alloc(4096, pool_bytes);
    assert(ctx->msg_pool != NULL);

     memset(ctx->msg_pool, 0, pool_bytes);
  void *pool_agent_ptr = NULL;
  hsa_status = hsa_amd_memory_lock(ctx->msg_pool, pool_bytes,
                                   NULL, 0, &pool_agent_ptr);
  assert(hsa_status == HSA_STATUS_SUCCESS);
  ctx->msg_pool_dev = pool_agent_ptr; // device view of the message pool

  struct iovec iov = { .iov_base = ctx->msg_pool, .iov_len = pool_bytes };
  ret = io_uring_register(ctx->ring_fd, IORING_REGISTER_BUFFERS, &iov, 1);
  assert(ret == 0);

    atomic_init(&ctx->sq_tail_cache, *ctx->sring_tail);
  return 0;
}

// async submit print request to SQ
// should we mutex guard this?
#pragma omp declare target
void uring_perror(uring_ctx_t *ctx, const char *msg, size_t msg_len) {
  unsigned tail = atomic_fetch_add_explicit(&ctx->sq_tail_cache, 1,
                                            memory_order_relaxed);
  int host = omp_is_initial_device();
  unsigned *mask_ptr = host ? ctx->sring_mask : ctx->sring_mask_dev;
  unsigned *tail_ptr = host ? ctx->sring_tail : ctx->sring_tail_dev;
  unsigned *array_ptr = host ? ctx->sring_array : ctx->sring_array_dev;
  struct io_uring_sqe *sqe_base = host ? ctx->sqes : ctx->sqes_dev;
  char *base = host ? ctx->msg_pool : (char *)ctx->msg_pool_dev;

  unsigned idx = tail & *mask_ptr; // TODO: check if not full
  struct io_uring_sqe *sqe = &sqe_base[idx];

  char *buff = base + idx * MSG_BUF_SIZE;
  memset(buff, 0, MSG_BUF_SIZE);
  assert(msg_len < MSG_BUF_SIZE);
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
  sqe->user_data = idx; // cookie for completion

  /* publish and advance the submission ring */
  array_ptr[idx] = idx; // must happen before the store_release
  io_uring_smp_store_release(tail_ptr, tail + 1);
}
#pragma omp end declare target

void uring_process_completions(uring_ctx_t *ctx) {
  unsigned head = io_uring_smp_load_acquire(ctx->cring_head);
  unsigned tail = *ctx->cring_tail;

  while (head != tail) {
    // cqes -> head is the index of the next completion
    // cring_mask is the mask for the completion ring
    //    i.e. a 32-bit value equal to ring_entries − 1.
    // so we do bitwise AND of the mask and the HEAD
    // so it wraps around the ring buffer
    // 
    struct io_uring_cqe *cqe = &ctx->cqes[head & *ctx->cring_mask];
    // the cookie cqe->user_data identifies which buffer was used
    head++; // just advance the head since we just printed anyway
  }

  io_uring_smp_store_release(ctx->cring_head, head);
}

void teardown_uring(uring_ctx_t *ctx) {
  // io_uring_unregister_buffers(ctx->ring_fd);
  if (ctx->msg_pool) {
    hsa_amd_memory_unlock(ctx->msg_pool);
    free(ctx->msg_pool);
  }
}