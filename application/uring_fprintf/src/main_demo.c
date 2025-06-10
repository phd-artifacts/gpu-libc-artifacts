#include <stdio.h>
#include "uring_ctx.h"
#include "uring_fprintf.h"

int main(void) {
    if (app_setup_uring() < 0) {
        fprintf(stderr, "crashed in initilization\n");
        return 1;
    }

    // call hardcoded printf
    // 
    uring_fprintf();

    return 0;
}
