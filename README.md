# GPU I/O with io_uring

This repository contains a demo that uses Linux `io_uring` to let a GPU issue print operations without resorting to RPC calls on the host, which is the current approach in [LLVM libc for GPUs](https://libc.llvm.org/gpu/). The ultimate goal is to improve performance by avoiding the two RPC hops, the shared buffer copy, and the necessity of user-level polling threads.

We use pinned memory and shared virtual memory (SVM) to allow the GPU to write directly to the `io_uring` submission queue, which the kernel polls in the background. 

The directories under `application/experiments` preserve the
steps taken while developing this demo, validating the enviroment and the feasibility of the experiment. They contain isolated tests for individual runtime features.

<img width="2826" height="1872" alt="uringprint" src="https://github.com/user-attachments/assets/aef03464-3a4d-44b5-b9a2-d1ba3e725148" />

As the figure below shows, the CPU starts an io_uring with a kernel polling thread, and allocate the queues using SVM. The GPU then writes to the submission queue without the need for a syscall. The CPU then read the queue and print the results.



# Helper scripts

The `helper.py` script orchestrates common tasks:

* `python helper.py build-llvm` – build LLVM using the script in `sh-scripts`.
* `python helper.py <application>` – run an application.

# Experiments

| Experiment               | What it tests                                                                                                                                                                                                   | Contribution toward the demo                                                                                                                                      |
| ------------------------ | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `io_uring_cpu_mt`        | Spawns 10 CPU threads that enqueue write requests into an `io_uring` ring while a waker thread nudges the polling thread to consume them                                                  | Provided the initial host‑side infrastructure for asynchronous logging via `io_uring`, validating the basic submission/completion workflow.                       |
| `io_uring_gpu_no_poll`   | Allocates an `io_uring` context in fine‑grained SVM memory and launches an OpenMP offload kernel that emits a message from the GPU without relying on a polling thread | Demonstrated that device code can directly populate the submission queue, proving GPU access to the ring buffers and paving the way for a unified CPU/GPU logger. |
| `io_uring_svm_cpu_only`  | Sets up `io_uring` with SVM buffers and a polling thread, but drives it solely from the CPU to verify the SVM configuration and wakeup mechanics                       | Confirmed the viability of fine‑grained SVM memory for the ring structures before introducing GPU writers.                                                        |
| `omp_deviceptr_demo`     | Minimal OpenMP target region returning whether execution occurred on the device                                                                                                            | Ensured the toolchain and runtime could offload code to the GPU, serving as a sanity check for later experiments.                                                 |
| `svm_target_edit_buffer` | Allocates fine‑grained host memory, accesses it from a GPU kernel, modifies a value, and observes the change on the host                                                             | Verified coherent host/GPU access to shared buffers, a requirement for sharing the `io_uring` structures with device code.                                        |
| `svm_migration_cg`       | Compares kernel‑launch times when writing to a migrated SVM page versus coarse‑grained GPU allocation                                                                                  | Quantified the cost of page migration, motivating memory placement choices for latency‑sensitive data in the demo.                                                |
| `svm_migration_fg`       | Benchmarks three memory types—SVM via `mmap`, coarse‑grained VRAM, and fine‑grained host memory—to measure write latency from the GPU                                                      | Provided comparative latency data that guided the final selection of fine‑grained SVM for the demo’s shared buffers.                                              |

Examples of experiments output can be found [here](exp_LOGS.md). To install the drivers and check for io_uring support, refer to the [INSTALLATION.md](INSTALLATION.md).