# Installation reference

This is a quick reference to the installation steps that I followed. I tested this on Ubuntu 22.04 (6.11.0-29-generic) nodes of the Chameleon Cloud, with AMD MI100. Below are the steps that we used to isntasll ROCm, HSA, LLVM and io_uring.

# ROCm

The ROCm versino in the repository was too old, so we used the versinon from the [AMD website](https://amdgpu-install.readthedocs.io/en/latest/install-installing.html?highlight=ubuntu#ubuntu-and-debian-based-systems). An important step is to add the user to the video group:

```bash
sudo usermod -a -G video $USER
```

## Python environment

```bash
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
```

## Ubuntu packages

```bash
sudo apt install gh
sudo apt install cmake
sudo apt install ninja-build
sudo apt install clang
sudo apt install ccache
sudo apt-get update
sudo apt-get dist-upgrade

mkdir Downloads
cd Downloads/
wget https://repo.radeon.com/amdgpu-install/latest/ubuntu/jammy/amdgpu-install_6.4.60401-1_all.deb

sudo apt-get install ./amdgpu-install_6.4.60401-1_all.deb
sudo amdgpu-install --usecase=rocm,openmpsdk -y
```

```bash
sudo apt install rocminfo
sudo usermod -a -G render cc
```

```bash
rocminfo | grep 'Name:' | grep gfx
```

## ROCm 6.x environment

```bash
export PATH=/opt/rocm-6.4.1/bin:$PATH
ls /opt/rocm-6.4.1
```

# HSA

At some point we needed a debug version of HSA. We did install it with Spack, which manages the enviroment with `spack load hsa-rocr-dev`.  The version installed was 6.4.1.

# LLVM

LLVM was compiled from source, using the script in `sh-scripts/build-llvm.sh`. The script uses the `build-llvm` target of the `helper.py` script.

# io_uring tests

A quick reference to verify that your system and kernel correctly support `io_uring`.

## tests from liburing

```bash
mkdir io_uring_test && cd io_uring_test
git clone https://github.com/axboe/liburing.git
cd liburing
./configure
make clean && ./configure
./test/io_uring_setup.t
````

This is supposed to work. If not, you can likely pinpoint the problem by testing the configurations bellow.

## Kernel configuration

```bash
grep CONFIG_IO_URING /boot/config-$(uname -r)
```

This should return 'y'.

## Look for syscall support

```bash
ausyscall --dump | grep io_uring_setup
```

This should return the syscall number.

## Diagnose with strace

```bash
strace -e io_uring_setup ./test/io_uring_setup.t
```

May give a more precise error message on the failiure.

## Check for disabled io_uring

```bash
cat /proc/sys/kernel/io_uring_disabled
```

Should give 0. If not, check:


```bash
echo 0 | sudo tee /proc/sys/kernel/io_uring_disabled
# Persist in /etc/sysctl.conf:
#   kernel.io_uring_disabled = 0
sudo sysctl -p
```

## Also try

```bash
sysctl -a | grep io_uring
dmesg  | grep -i io_uring
```