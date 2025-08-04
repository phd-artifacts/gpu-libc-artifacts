#!/usr/bin/env bash
set -e

. ~/spack/share/spack/setup-env.sh # load spack
spack load hsa-rocr-dev@6.4.1 /rlpc6ww5r7u5su3yyyz36sx4rrmdjemj

clang --version

cd "$(dirname "$0")"
export CC=clang
export CXX=clang++
export LDFLAGS="-fuse-ld=lld"

export CPATH=/home/cc/spack/opt/spack/linux-zen3/hsa-rocr-dev-6.4.1-rlpc6ww5r7u5su3yyyz36sx4rrmdjemj/include/${CPATH:+:$CPATH}
export LIBRARY_PATH=/home/cc/spack/opt/spack/linux-zen3/hsa-rocr-dev-6.4.1-rlpc6ww5r7u5su3yyyz36sx4rrmdjemj/lib${LIBRARY_PATH:+:$LIBRARY_PATH}
export LD_LIBRARY_PATH=/home/cc/spack/opt/spack/linux-zen3/hsa-rocr-dev-6.4.1-rlpc6ww5r7u5su3yyyz36sx4rrmdjemj/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}


set -e
clang++ -fopenmp \
 -g \
 --offload-arch=gfx908 \
 -resource-dir=/home/cc/gpu-libc-artifacts/llvm-infra/llvm-installs/apptainer-Debug/lib/clang/21 \
 -frtlib-add-rpath \
 -fopenmp-targets=amdgcn-amd-amdhsa \
 -fuse-ld=lld \
 -lhsa-runtime64 \
 svm_api_test.cpp

set +e

export AMD_LOG_LEVEL=7

./a.out