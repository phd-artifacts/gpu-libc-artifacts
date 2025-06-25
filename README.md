# GPU libc artifacts

This repository contains a minimal demonstration using Linux io_uring and multiple threads. The program is located in `application/uring_print_mt`.

## Execution flow
1. `setup_uring()` configures an `io_uring` instance in polling mode and maps the submission and completion rings.
2. Ten worker threads are created in `main_demo.c`. Each formats a message and dispatches it through `uring_perror()`.
3. `uring_perror()` enqueues a write operation using the io_uring API and uses memory barriers (from `liburing/barrier.h`) to publish descriptors.
4. After all threads have finished, `uring_process_completions()` drains the completion queue.

## Memory barriers
The program uses `io_uring_smp_store_release` and `io_uring_smp_load_acquire` from `liburing/barrier.h` to ensure ordering between producers and consumers.

## Data structure
`uring_ctx_t` defined in `application/uring_print_mt/include/uring_ctx.h` holds pointers to ring buffer fields, SQEs, CQEs, a message pool and a cached tail pointer.

An example output for a single run is stored in `application/uring_print_mt/example_output.txt`.
