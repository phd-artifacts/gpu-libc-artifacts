#include "uring_fprintf.h"
#include "uring_ctx.h"

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

// print to stdout / stderr only
void uring_fprintf(void) {
    // dummy prototype
    int fd = open("hi.txt", O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (fd < 0) {
        perror("crashed");
        return;
    }

    // write message
    const char *msg = "Hello from uring_fprintf\n";
    size_t      len = strlen(msg);

    memcpy(buff /*global*/, msg, len);

    offset = 0; // global. dummy hardcoded

    // send to queue
    if (submit_to_sq(fd, IORING_OP_WRITE, len, offset) < 0) {
        fprintf(stderr, "crashn");
        close(fd);
        return;
    }

    // 'return' code + block wait
    int res = read_from_cq();
    if (res < 0) {
        fprintf(stderr, "No completion event\n");
    } else if ((size_t)res != len) {
        fprintf(stderr, "chrashed");
    }

    close(fd);
}