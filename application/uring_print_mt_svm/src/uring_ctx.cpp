#include "uring_ctx.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <atomic>
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

static hsa_status_t find_gpu_agent(hsa_agent_t agent, void *data) {
  hsa_device_type_t type;
  if (hsa_agent_get_info(agent, HSA_AGENT_INFO_DEVICE, &type) !=
      HSA_STATUS_SUCCESS)
    return HSA_STATUS_ERROR;
  if (type == HSA_DEVICE_TYPE_GPU) {
    *(hsa_agent_t *)data = agent;
    return HSA_STATUS_INFO_BREAK;
  }
  return HSA_STATUS_SUCCESS;
}

static hsa_agent_t get_gpu_agent(void) {
  hsa_agent_t agent = {0};
  hsa_status_t status = hsa_iterate_agents(find_gpu_agent, &agent);
  assert(status == HSA_STATUS_SUCCESS || status == HSA_STATUS_INFO_BREAK);
  return agent;
}

static void make_svm_accessible(void *ptr, size_t size, hsa_agent_t agent) {
  hsa_amd_svm_attribute_pair_t attrs[] = {
      {HSA_AMD_SVM_ATTRIB_AGENT_ACCESSIBLE, (uint64_t)agent.handle}};
  hsa_status_t status =
      hsa_amd_svm_attributes_set(ptr, size, attrs, /*attribute_count=*/1);
  if (status != HSA_STATUS_SUCCESS) {
    const char *err = NULL;
    if (hsa_status_string(status, &err) != HSA_STATUS_SUCCESS)
      err = "unknown";
    fprintf(stderr, "hsa_amd_svm_attributes_set failed: %s\n",
            err ? err : "");
    return;
  }
}

/*
 * Initialize io_uring in polling mode
   and mmap the SQ/CQ rings and SQE array.
 */
int setup_uring(uring_ctx_t *ctx) {
  hsa_status_t hsa_status = hsa_init();
  assert(hsa_status == HSA_STATUS_SUCCESS);
  hsa_agent_t gpu_agent = get_gpu_agent();

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
  make_svm_accessible(sq_ptr, sring_sz, gpu_agent);
  ctx->sring_head_dev = (char *)sq_ptr + p.sq_off.head;
  ctx->sring_tail_dev = (char *)sq_ptr + p.sq_off.tail;
  ctx->sring_mask_dev = (char *)sq_ptr + p.sq_off.ring_mask;
  ctx->sring_array_dev = (char *)sq_ptr + p.sq_off.array;

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
  make_svm_accessible(cq_ptr, cring_sz, gpu_agent);

  /* Grab pointers to SQ ring head/tail/mask/array fields. */
  ctx->sring_head = (unsigned *)((char *)sq_ptr + p.sq_off.head);
  ctx->sring_tail = (unsigned *)((char *)sq_ptr + p.sq_off.tail);
  ctx->sring_mask = (unsigned *)((char *)sq_ptr + p.sq_off.ring_mask);
  ctx->sring_array = (unsigned *)((char *)sq_ptr + p.sq_off.array);

  /* Map the actual SQE array. */
  ctx->sqes = (struct io_uring_sqe *)mmap(
      NULL, p.sq_entries * sizeof(struct io_uring_sqe),
      PROT_READ | PROT_WRITE, MAP_SHARED | MAP_POPULATE, ctx->ring_fd,
      IORING_OFF_SQES);
  assert(ctx->sqes != MAP_FAILED);
  make_svm_accessible(ctx->sqes,
                      p.sq_entries * sizeof(struct io_uring_sqe), gpu_agent);
  ctx->sqes_dev = ctx->sqes;

  /* Grab pointers to CQ ring head/tail/mask fields and the CQE array. */
  ctx->cring_head = (unsigned *)((char *)cq_ptr + p.cq_off.head);
  ctx->cring_tail = (unsigned *)((char *)cq_ptr + p.cq_off.tail);
  ctx->cring_mask = (unsigned *)((char *)cq_ptr + p.cq_off.ring_mask);
  ctx->cqes = (struct io_uring_cqe *)((char *)cq_ptr + p.cq_off.cqes);

  io_uring_enter(ctx->ring_fd, 0, 0, IORING_ENTER_SQ_WAKEUP);

  size_t pool_bytes = QUEUE_DEPTH * MSG_BUF_SIZE;
  ctx->msg_pool = (char *)aligned_alloc(4096, pool_bytes);
    assert(ctx->msg_pool != NULL);

     memset(ctx->msg_pool, 0, pool_bytes);
  make_svm_accessible(ctx->msg_pool, pool_bytes, gpu_agent);
  ctx->msg_pool_dev = ctx->msg_pool; // device view of the message pool

  struct iovec iov = { .iov_base = ctx->msg_pool, .iov_len = pool_bytes };
  ret = io_uring_register(ctx->ring_fd, IORING_REGISTER_BUFFERS, &iov, 1);
  assert(ret == 0);

    ctx->sq_tail_cache.store(*ctx->sring_tail, std::memory_order_relaxed);
  return 0;
}

// async submit print request to SQ
// should we mutex guard this?
#pragma omp declare target
void uring_perror(uring_ctx_t *ctx, const char *msg, size_t msg_len) {
  unsigned tail = ctx->sq_tail_cache.fetch_add(1, std::memory_order_relaxed);
  int host = omp_is_initial_device();
  unsigned *mask_ptr =
      host ? ctx->sring_mask : (unsigned *)ctx->sring_mask_dev;
  unsigned *tail_ptr =
      host ? ctx->sring_tail : (unsigned *)ctx->sring_tail_dev;
  unsigned *array_ptr =
      host ? ctx->sring_array : (unsigned *)ctx->sring_array_dev;
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
    free(ctx->msg_pool);
  }
}
