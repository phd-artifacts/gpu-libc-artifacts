
clang --version

clang --print-targets | grep -i amd

amdgpu-arch



export LD_LIBRARY_PATH=/opt/rocm-6.4.1/lib:$LD_LIBRARY_PATH
unset OMP_TARGET_OFFLOAD
# export OMP_TARGET_OFFLOAD=MANDATORY

set -e
clang++ -fopenmp \
 -g \
 --offload-arch=gfx908 \
 -resource-dir=/home/cc/gpu-libc-artifacts/llvm-infra/llvm-installs/apptainer-Debug/lib/clang/21 \
 -frtlib-add-rpath \
 -fopenmp-targets=amdgcn-amd-amdhsa \
 input.cpp

set +e

LIBOMPTARGET_DEBUG=1 ./a.out

echo $?
