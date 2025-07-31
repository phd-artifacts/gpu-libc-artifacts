#include "uring_ctx.h"
#include <omp.h>
#include <stdio.h>
#include <assert.h>

// kernel that calls our print
#pragma omp declare target
void uring_fn(void *ptr)
{
  int is_initial_device = omp_is_initial_device();
  assert(!is_initial_device && "NOT ON DEVICE");
  // printf("kernel launched!\n");
  uring_ctx_t *ctx = (uring_ctx_t *)ptr;
  // printf("device sq_ring_ptr=%p sqes=%p\n", ctx->sq_ring_ptr, ctx->sqes_dev);
  uring_perror((uring_ctx_t *)ptr,
                  "Hello from the device!\n\0", 24);
  uring_perror((uring_ctx_t *)ptr,
                  "Hello again from the device!\n\0", 34);

  // printf("Done!\n");

}
#pragma omp end declare target

// stub since clang wants cpu version as well
#pragma omp begin declare variant match(device = {kind(cpu)})
void uring_fn(void *unused) { (void)unused; __builtin_trap(); }
#pragma omp end declare variant
