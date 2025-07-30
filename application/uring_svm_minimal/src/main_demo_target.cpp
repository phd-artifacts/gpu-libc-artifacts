#include "uring_ctx.h"
#include <assert.h>
#include <stdio.h>
uring_ctx_t g_ctx;

int main(void) {
    if (setup_uring(&g_ctx) != 0)
        return 1;
    assert(&g_ctx != NULL);
    fprintf(stderr, "host sq_ring_ptr=%p sqes=%p\n", g_ctx.sq_ring_ptr, g_ctx.sqes);

    uring_perror(&g_ctx, "Hello from the CPU using SVM pointer...", 39);
    uring_flush(&g_ctx);
    fprintf(stderr, "host tail=%u cache=%u\n",
            *g_ctx.sring_tail, g_ctx.sq_tail_cache.load());
    uring_process_completions(&g_ctx);

    teardown_uring(&g_ctx);
    return 0;
}
