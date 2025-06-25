#include "uring_ctx.h"
#include <assert.h>
#include <linux/io_uring.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>

#define NUM_THREADS 10

static uring_ctx_t g_ctx;

void *thread_func(void *arg) {
  char msg[64];

  // format the string
  int id = *((int *)arg);
  snprintf(msg, sizeof(msg), "Thread %d says hello\n", id);
  size_t len = strlen(msg);
  free(arg); // dumb i know

  // send buffer to uring printer
  uring_perror(&g_ctx, msg, len);

  return NULL;
}

void *waker_thread_func(void *arg) {
  uring_ctx_t *ctx = (uring_ctx_t *)arg;

  while (1) {
    // wake up the uring poller
    io_uring_enter(ctx->ring_fd, 0, 0, IORING_ENTER_SQ_WAKEUP);
    usleep(1000); // sleep for a bit to avoid busy waiting
  }

  return NULL;
}

int main(void) {
  setup_uring(&g_ctx);
  assert(&g_ctx != NULL);

  pthread_t threads[NUM_THREADS];

  // create a waker thread to wake up the uring poller
  pthread_t waker_thread;
  pthread_create(&waker_thread, NULL, waker_thread_func, &g_ctx);
  pthread_detach(waker_thread); // detach the waker thread


  // each thread will do an async print request
  for (int i = 0; i < NUM_THREADS; ++i) {
    int *arg = malloc(sizeof(int));
    *arg = i;
    pthread_create(&threads[i], NULL, thread_func, arg);
  }

  // finish threads
  for (int i = 0; i < NUM_THREADS; ++i) {
    pthread_join(threads[i], NULL);
  }


  // process completions
  // io_uring_enter(g_ctx.ring_fd, NUM_THREADS /*to submit */, 0 /*min complete*/,
  //                IORING_ENTER_SQ_WAKEUP /*ensure we wake up polling thread*/);
  // io_uring_enter(g_ctx.ring_fd, 0, NUM_THREADS, IORING_ENTER_GETEVENTS);
  // uring_process_completions(&g_ctx);



  return 0;
}
