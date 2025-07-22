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
  printf("Hello from the AMD GPU!\n");
  uring_perror((uring_ctx_t *)ptr,
                  "Hello from the device!\n", 24);

}
#pragma omp end declare target

// stub since clang wants cpu version as well
#pragma omp begin declare variant match(device = {kind(cpu)})
void uring_fn(void *) { __builtin_trap(); }
#pragma omp end declare variant
