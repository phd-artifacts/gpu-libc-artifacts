#include "uring_ctx.h"
#include <omp.h>
#include <stdio.h>
#include <assert.h>

/* kernel that calls our print */
#pragma omp declare target
void uring_fn(void *ptr)
{
  int is_initial_device = omp_is_initial_device();
  assert(!is_initial_device && "NOT ON DEVICE");
  uring_ctx_t *ctx = (uring_ctx_t *)ptr;
  constexpr char msg1[] = "First hello from the device!\n";
  constexpr char msg2[] = "Second hello from the device! - yes, again\n";
  uring_perror(ctx, msg1, sizeof(msg1) - 1);
  uring_perror(ctx, msg2, sizeof(msg2) - 1);
}
#pragma omp end declare target

/* stub since clang wants cpu version as well */
#pragma omp begin declare variant match(device = {kind(cpu)})
void uring_fn(void *unused) { (void)unused; __builtin_trap(); }
#pragma omp end declare variant
