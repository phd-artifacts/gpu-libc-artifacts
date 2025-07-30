#include "uring_ctx.h"
#include <assert.h>
#include <stdio.h>
#include <omp.h>

void uring_fn(void *);

uring_ctx_t g_ctx;

//this global may exist on the device
#pragma omp declare target(g_ctx)

int main(void)
{
    if (setup_uring(&g_ctx) != 0)
        return 1;
    assert(&g_ctx != NULL);
    fprintf(stderr, "host sq_ring_ptr=%p sqes=%p\n", g_ctx.sq_ring_ptr, g_ctx.sqes);

    // copy ring ctx to device 
    // use c++
    // hsa pin the memory
    // test writting/reading pointer 
    // minimal test: mmap and register and write/read test
    #pragma omp target update to(g_ctx)

    uring_perror(&g_ctx, "Hello from the CPU...", 24);
    uring_flush(&g_ctx);
    fprintf(stderr, "host tail before kernel=%u cache=%u\n",
            *g_ctx.sring_tail, g_ctx.sq_tail_cache.load());

    // run kernel
    #pragma omp target
    uring_fn(&g_ctx);
    uring_flush(&g_ctx); // flush device submission
    #pragma omp target update from(g_ctx)
    fprintf(stderr, "host tail after kernel=%u cache=%u\n",
            *g_ctx.sring_tail, g_ctx.sq_tail_cache.load());
    uring_process_completions(&g_ctx);

    uring_perror(&g_ctx, "Hello from the CPU after kernel...", 36);
    uring_flush(&g_ctx);
    uring_process_completions(&g_ctx);

    teardown_uring(&g_ctx);
    return 0;
}
