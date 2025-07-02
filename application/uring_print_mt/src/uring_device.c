#include "uring_ctx.h"

// kernel that calls our print
#pragma omp declare target
void uring_fn(void *ptr)
{
  uring_perror((uring_ctx_t *)ptr,
                  "Hello from the device!\n", 24);

}
#pragma omp end declare target

// stub since clang wants cpu version as well
#pragma omp begin declare variant match(device = {kind(cpu)})
void uring_fn(void *) { __builtin_trap(); }
#pragma omp end declare variant
