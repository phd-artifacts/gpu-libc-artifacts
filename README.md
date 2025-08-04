# GPU I/O with io_uring

This demo uses Linux `io_uring` to let a GPU issue print operations without resorting to RPC calls on the host, which is the current approach in [LLVM libc for GPUs](https://libc.llvm.org/gpu/). The ultimate goal is to improve performance by avoiding the two RPC hops, the shared buffer copy, and the necessity of user-level polling threads.

We use pinned memory and shared virtual memory (SVM) to allow the GPU to write directly to the `io_uring` submission queue, which the kernel polls in the background. 

The directories under `application/experiments` preserve the
steps taken while developing this demo, validating the enviroment and the feasibility of the experiment. They contain isolated tests for individual runtime features.


todo: describe the system, simplify python script, explain sh scripts
descriv ethe enviroment

## Installation on Ubuntu

The programs were developed on Ubuntu 22.04 nodes of the Chameleon Cloud.
To reproduce the environment install Clang, CMake and ROCm:

```bash
sudo apt-get update
sudo apt-get install -y clang build-essential cmake liburing-dev \
    gnupg wget lsb-release
# ROCm repository
DISTRO=$(lsb_release -cs)
wget -qO - https://repo.radeon.com/rocm/rocm.gpg.key | sudo apt-key add -
echo "deb [arch=amd64] https://repo.radeon.com/rocm/apt/6.4/ ${DISTRO} main" | \
    sudo tee /etc/apt/sources.list.d/rocm.list
sudo apt-get update
sudo apt-get install -y rocm-hip-sdk
```


## Continuous Integration

CI checks that Python helpers compile and the final application builds
successfully using system Clang and ROCm packages.

