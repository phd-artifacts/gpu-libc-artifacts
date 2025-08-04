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
#include <iostream>

static void handle_error(hsa_status_t code, uint32_t line = 0) {
  if (code == HSA_STATUS_SUCCESS || code == HSA_STATUS_INFO_BREAK)
    return;
  const char *desc;
  if (hsa_status_string(code, &desc) != HSA_STATUS_SUCCESS)
    desc = "Unknown";
  std::cerr << "HSA error:" << line << " " << (desc ? desc : "") << "\n";
  std::abort();
}

template <typename E, typename F, typename C>
static hsa_status_t iterate(F fn, C cb) {
  auto l = [](E e, void *d) -> hsa_status_t {
    C *u = static_cast<C *>(d);
    return (*u)(e);
  };
  return fn(l, static_cast<void *>(&cb));
}

template <typename E, typename F, typename A, typename C>
static hsa_status_t iterate(F fn, A arg, C cb) {
  auto l = [](E e, void *d) -> hsa_status_t {
    C *u = static_cast<C *>(d);
    return (*u)(e);
  };
  return fn(arg, l, static_cast<void *>(&cb));
}

template <typename C>
static hsa_status_t iterate_agents(C cb) {
  return iterate<hsa_agent_t>(hsa_iterate_agents, cb);
}

template <typename C>
static hsa_status_t iterate_agent_memory_pools(hsa_agent_t a, C cb) {
  return iterate<hsa_amd_memory_pool_t>(hsa_amd_agent_iterate_memory_pools, a,
                                        cb);
}

template <hsa_device_type_t DT>
static hsa_status_t get_agent(hsa_agent_t *out) {
  auto cb = [&](hsa_agent_t a) -> hsa_status_t {
    hsa_device_type_t type;
    if (hsa_agent_get_info(a, HSA_AGENT_INFO_DEVICE, &type))
      return HSA_STATUS_ERROR;
    if (type != DT)
      return HSA_STATUS_SUCCESS;

    if (DT == HSA_DEVICE_TYPE_GPU) {
      hsa_agent_feature_t f;
      if (hsa_agent_get_info(a, HSA_AGENT_INFO_FEATURE, &f))
        return HSA_STATUS_ERROR;
      if (!(f & HSA_AGENT_FEATURE_KERNEL_DISPATCH))
        return HSA_STATUS_SUCCESS;
    }
    *out = a;
    return HSA_STATUS_INFO_BREAK;
  };
  return iterate_agents(cb);
}

template <hsa_amd_memory_pool_global_flag_t FL>
static hsa_status_t get_agent_memory_pool(hsa_agent_t ag,
                                          hsa_amd_memory_pool_t *out) {
  auto cb = [&](hsa_amd_memory_pool_t p) {
    hsa_amd_segment_t seg;
    uint32_t flags;
    if (hsa_amd_memory_pool_get_info(p, HSA_AMD_MEMORY_POOL_INFO_SEGMENT, &seg))
      return HSA_STATUS_ERROR;
    if (seg != HSA_AMD_SEGMENT_GLOBAL)
      return HSA_STATUS_SUCCESS;
    if (hsa_amd_memory_pool_get_info(p, HSA_AMD_MEMORY_POOL_INFO_GLOBAL_FLAGS,
                                     &flags))
      return HSA_STATUS_ERROR;
    if (flags & FL)
      *out = p;
    return HSA_STATUS_SUCCESS;
  };
  return iterate_agent_memory_pools(ag, cb);
}

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
    exit(1);
    return;
  }
}

/*
 * Initialize io_uring in polling mode
   and mmap the SQ/CQ rings and SQE array.
 */
int setup_uring(uring_ctx_t *ctx) {
  handle_error(hsa_init(), __LINE__);
  hsa_agent_t gpu_agent{};
  handle_error(get_agent<HSA_DEVICE_TYPE_GPU>(&gpu_agent), __LINE__);
  hsa_agent_t cpu_agent{};
  handle_error(get_agent<HSA_DEVICE_TYPE_CPU>(&cpu_agent), __LINE__);

  hsa_amd_memory_pool_t fg_pool{};
  handle_error(
      get_agent_memory_pool<HSA_AMD_MEMORY_POOL_GLOBAL_FLAG_FINE_GRAINED>(
          cpu_agent, &fg_pool),
      __LINE__);

  struct io_uring_params p;
  void *ring_mem, *sqe_mem, *cq_ptr;
  int sring_sz, cring_sz;

  memset(&p, 0, sizeof(p)); /* io_uring expects a zeroed data struct */
  p.flags = IORING_SETUP_SQPOLL | IORING_SETUP_NO_MMAP;
  p.sq_thread_idle = UINT_MAX / 1000;

  size_t ring_bytes = 2 * 1024 * 1024; // plenty for ring metadata
  handle_error(hsa_amd_memory_pool_allocate(fg_pool, ring_bytes, 0, &ring_mem),
               __LINE__);
  handle_error(hsa_amd_agents_allow_access(1, &gpu_agent, nullptr, ring_mem),
               __LINE__);
  memset(ring_mem, 0, ring_bytes);
  if (mlock(ring_mem, ring_bytes) != 0)
    perror("mlock ring_mem");

  size_t sqe_bytes = 2 * 1024 * 1024; // submission queue entries
  handle_error(hsa_amd_memory_pool_allocate(fg_pool, sqe_bytes, 0, &sqe_mem),
               __LINE__);
  handle_error(hsa_amd_agents_allow_access(1, &gpu_agent, nullptr, sqe_mem),
               __LINE__);
  memset(sqe_mem, 0, sqe_bytes);
  if (mlock(sqe_mem, sqe_bytes) != 0)
    perror("mlock sqe_mem");

  p.cq_off.user_addr = (uint64_t)ring_mem; /* SQ/CQ rings */
  p.sq_off.user_addr = (uint64_t)sqe_mem;  /* SQEs */

  ctx->ring_fd = io_uring_setup(QUEUE_DEPTH, &p);
  if (ctx->ring_fd < 0) {
    perror("io_uring_setup");
    return -1;
  }

  /* register STDERR in the list of fixed descriptors */
  int fds[] = {STDERR_FILENO};
  int ret = io_uring_register(ctx->ring_fd,
                            IORING_REGISTER_FILES,
                            fds, /* pointer to your array */
                            1  /* number of entries */ );
  if (ret != 0) {
    perror("io_uring_register FILES");
    return -1;
  }

  /* Compute how many bytes we need to mmap for SQ ring and CQ ring. */
  sring_sz = p.sq_off.array + p.sq_entries * sizeof(unsigned);
  cring_sz = p.cq_off.cqes + p.cq_entries * sizeof(struct io_uring_cqe);

  /* Use caller provided ring memory */
  ctx->sq_ring_ptr = ring_mem;
  ctx->sq_ring_sz = sring_sz;

  /* Map the completion ring buffer (or reuse the same if SINGLE_MMAP). */
  cq_ptr = ring_mem;
  ctx->cq_ring_ptr = cq_ptr;
  ctx->cq_ring_sz = cring_sz;

  /* Grab pointers to SQ ring head/tail/mask/array fields. */
  ctx->sring_head = (unsigned *)((char *)ring_mem + p.sq_off.head);
  ctx->sring_tail = (unsigned *)((char *)ring_mem + p.sq_off.tail);
  ctx->sring_mask = (unsigned *)((char *)ring_mem + p.sq_off.ring_mask);
  ctx->sring_array = (unsigned *)((char *)ring_mem + p.sq_off.array);
  ctx->sring_flags = (unsigned *)((char *)ring_mem + p.sq_off.flags);

  /* Use caller provided SQE memory */
  ctx->sqes = (struct io_uring_sqe *)sqe_mem;

  /* Grab pointers to CQ ring head/tail/mask fields and the CQE array. */
  ctx->cring_head = (unsigned *)((char *)cq_ptr + p.cq_off.head);
  ctx->cring_tail = (unsigned *)((char *)cq_ptr + p.cq_off.tail);
  ctx->cring_mask = (unsigned *)((char *)cq_ptr + p.cq_off.ring_mask);
  ctx->cqes = (struct io_uring_cqe *)((char *)cq_ptr + p.cq_off.cqes);

  size_t pool_bytes = QUEUE_DEPTH * MSG_BUF_SIZE;
  void *pool_mem = nullptr;
  handle_error(
      hsa_amd_memory_pool_allocate(fg_pool, pool_bytes, 0, &pool_mem), __LINE__);
  handle_error(hsa_amd_agents_allow_access(1, &gpu_agent, nullptr, pool_mem),
               __LINE__);
  ctx->msg_pool = static_cast<char *>(pool_mem);
  memset(ctx->msg_pool, 0, pool_bytes);
  if (mlock(pool_mem, pool_bytes) != 0)
    perror("mlock msg_pool");

  struct iovec iov = { .iov_base = ctx->msg_pool, .iov_len = pool_bytes };
  ret = io_uring_register(ctx->ring_fd, IORING_REGISTER_BUFFERS, &iov, 1);
  if (ret != 0) {
    perror("io_uring_register BUFFERS");
    return -1;
  }

  /* Wake the polling thread now that everything is registered */
  ret = io_uring_enter(ctx->ring_fd, 0, 0, IORING_ENTER_SQ_WAKEUP);
  if (ret < 0)
    perror("io_uring_enter wakeup");

  ctx->sq_tail_cache.store(*ctx->sring_tail, std::memory_order_relaxed);
  return 0;
}

// async submit print request to SQ
#pragma omp declare target
void uring_perror(uring_ctx_t *ctx, const char *msg, size_t msg_len) {
  unsigned tail = ctx->sq_tail_cache.fetch_add(1, std::memory_order_relaxed);
  unsigned *mask_ptr = ctx->sring_mask;
  unsigned *tail_ptr = ctx->sring_tail;
  unsigned *array_ptr = ctx->sring_array;
  unsigned *flags_ptr = ctx->sring_flags;
  struct io_uring_sqe *sqe_base = ctx->sqes;
  char *base = ctx->msg_pool;

  // TODO: this is an std::atomic_load_explicit
  //       does this work on the GPU?
  unsigned head = io_uring_smp_load_acquire(ctx->sring_head);
  unsigned entries = *mask_ptr + 1;
  while (tail - head >= entries)
    head = io_uring_smp_load_acquire(ctx->sring_head);

  unsigned idx = tail & *mask_ptr;
  struct io_uring_sqe *sqe = &sqe_base[idx];

  char *buff = base + idx * MSG_BUF_SIZE;
  memset(buff, 0, MSG_BUF_SIZE);
  assert(msg_len < MSG_BUF_SIZE);
  memcpy(buff, msg, msg_len);

  /* prepare SQE for stderr write */
  memset(sqe, 0, sizeof(*sqe));
  sqe->opcode = IORING_OP_WRITE;
  sqe->fd = 0; /* index of our STDERR in the fixed file descriptor list*/
  sqe->flags = IOSQE_FIXED_FILE; /* use registered file descriptor,
                                  required for io_uring polling mode */
  sqe->addr = (unsigned long)buff;
  sqe->len = msg_len;
  sqe->off = -1;
  sqe->user_data = idx; // cookie for completion

  /* publish and advance the submission ring */
  array_ptr[idx] = idx;
  // TODO: this is an std::atomic_store_explicit
  //       does this work on the GPU?
  io_uring_smp_store_release(tail_ptr, tail + 1);
}
#pragma omp end declare target

void teardown_uring(uring_ctx_t *ctx) {
  auto ring_fd = ctx->ring_fd;
  io_uring_enter(ring_fd, 0, 0, IORING_ENTER_SQ_WAKEUP);
  sleep(3);

  if (ctx->msg_pool)
    hsa_amd_memory_pool_free(ctx->msg_pool);
  if (ctx->sq_ring_ptr)
    hsa_amd_memory_pool_free(ctx->sq_ring_ptr);
  if (ctx->sqes)
    hsa_amd_memory_pool_free(ctx->sqes);
}

bool uring_has_pending(const uring_ctx_t *ctx) {
  unsigned sq_head = io_uring_smp_load_acquire(ctx->sring_head);
  unsigned sq_tail = *ctx->sring_tail;
  unsigned cq_head = io_uring_smp_load_acquire(ctx->cring_head);
  unsigned cq_tail = *ctx->cring_tail;
  return (sq_head != sq_tail) || (cq_head != cq_tail);
}

