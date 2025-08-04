# Applications

This folder contains the final demonstration program and a set of
experimental programs used during development.

- **uring_print_mt_svm** – final multi-threaded io_uring example using
  shared virtual memory for GPU communication.
- **experiments/** – early attempts and feature tests:
  - `uring_print_mt` – first version of the multi-thread demo without
    GPU interaction.
  - `uring_svm_minimal` – minimal shared virtual memory example.
  - `uring_print_mt_svm_nopolling` – SVM demo without io_uring polling.
  - `target_and_svm_test`, `svm_test`, `svm_test_fine`, and
    `deviceptr_test` – isolated tests for compiler flags and SVM APIs.

Each subdirectory provides a `run.sh` script illustrating how the
application was compiled during our experiments.
