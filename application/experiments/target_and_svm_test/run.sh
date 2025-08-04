#!/usr/bin/env bash
set -e

cd "$(dirname "$0")"

export CC=clang
export CXX=clang++
export LDFLAGS="-fuse-ld=lld"

export CPATH=/home/cc/spack/opt/spack/linux-zen3/hsa-rocr-dev-6.4.1-rlpc6ww5r7u5su3yyyz36sx4rrmdjemj/include/${CPATH:+:$CPATH}
export LIBRARY_PATH=/home/cc/spack/opt/spack/linux-zen3/hsa-rocr-dev-6.4.1-rlpc6ww5r7u5su3yyyz36sx4rrmdjemj/lib${LIBRARY_PATH:+:$LIBRARY_PATH}
export LD_LIBRARY_PATH=/home/cc/spack/opt/spack/linux-zen3/hsa-rocr-dev-6.4.1-rlpc6ww5r7u5su3yyyz36sx4rrmdjemj/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}

"$CXX" --version


clang++ -fopenmp \
  -g \
  --offload-arch=gfx908 \
  -resource-dir=/home/cc/gpu-libc-artifacts/llvm-infra/llvm-installs/apptainer-Debug/lib/clang/21 \
  -frtlib-add-rpath \
  -fopenmp-targets=amdgcn-amd-amdhsa \
  -lhsa-runtime64 \
  svm_target_demo.cpp

set +e

./a.out
