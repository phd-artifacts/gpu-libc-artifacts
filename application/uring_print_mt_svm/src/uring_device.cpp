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
  printf("kernel launched!\n");
  uring_perror((uring_ctx_t *)ptr,
                  "Hello from the device!\n", 24);

  // for some reason, if we do not wait
  // the text turns into garbage
  for(int i = 0; i < 1e8; i++){}
  printf("Done!\n");

}
#pragma omp end declare target

// stub since clang wants cpu version as well
#pragma omp begin declare variant match(device = {kind(cpu)})
void uring_fn(void *unused) { (void)unused; __builtin_trap(); }
#pragma omp end declare variant
