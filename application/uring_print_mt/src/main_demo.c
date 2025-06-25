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

#define NUM_THREADS 10

static uring_ctx_t g_ctx; 

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

  io_uring_enter(g_ctx.ring_fd, NUM_THREADS, 0, IORING_ENTER_SQ_WAKEUP);
  io_uring_enter(g_ctx.ring_fd, 0, NUM_THREADS, IORING_ENTER_GETEVENTS);
  uring_process_completions(&g_ctx);

  return 0;
}
