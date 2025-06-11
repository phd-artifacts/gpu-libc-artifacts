#include "uring_ctx.h"
#include <unistd.h>
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <linux/io_uring.h>
#include <unistd.h>

#define NUM_THREADS 4

static uring_ctx_t g_ctx; 
/*  Todo: clean this up. From arch wiki example:
 * Raw syscall wrappers (glibc may not have them yet).
 */
static int io_uring_setup(unsigned entries, struct io_uring_params *p) {
  return (int)syscall(__NR_io_uring_setup, entries, p);
}
static int io_uring_enter(int ring_fd, unsigned to_submit,
                          unsigned min_complete, unsigned flags) {
  return (int)syscall(__NR_io_uring_enter, ring_fd, to_submit, min_complete,
                      flags, NULL, 0);
}


void *thread_func(void *arg) {
  char msg[64];

  // format the string
  int id = *((int*) arg);
  snprintf(msg, sizeof(msg), "Thread %d says hello\n", id);
  size_t len = strlen(msg);
  free(arg); // dumb i know

  // send buffer to uring printer
  uring_perror(&g_ctx, msg, len);
//   perror("-> sent to uring printer");

  return NULL;
}

int main(void) {
  setup_uring(&g_ctx);
  assert(&g_ctx != NULL);

  pthread_t threads[NUM_THREADS];

  for (int i = 0; i < NUM_THREADS; ++i) {
    int *arg = malloc(sizeof(int));
    *arg = i;
    pthread_create(&threads[i], NULL, thread_func, arg);
  }

  for (int i = 0; i < NUM_THREADS; ++i) {
    pthread_join(threads[i], NULL);
  }
  
  io_uring_enter(g_ctx.ring_fd, 1, 1, IORING_ENTER_GETEVENTS);
  io_uring_enter(g_ctx.ring_fd, 1, 1, IORING_ENTER_GETEVENTS);
  io_uring_enter(g_ctx.ring_fd, 1, 1, IORING_ENTER_GETEVENTS);
  io_uring_enter(g_ctx.ring_fd, 1, 1, IORING_ENTER_GETEVENTS);

  sleep(5);

  return 0;
}
