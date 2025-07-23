#include "uring_ctx.h"
#include <assert.h>
#include <omp.h>

void uring_fn(void *);

uring_ctx_t g_ctx;

//this global may exist on the device
#pragma omp declare target(g_ctx)

int main(void)
{
    setup_uring(&g_ctx);
    assert(&g_ctx != NULL);

    // copy ring ctx to device 
    // use c++
    // hsa pin the memory
    // test writting/reading pointer 
    // minimal test: mmap and register and write/read test
    #pragma omp target update to(g_ctx)

    // run kernel
    #pragma omp target
    uring_fn(&g_ctx);

    teardown_uring(&g_ctx);
    return 0;
}
