python3 helper.py experiments/io_uring_cpu_mt

output:
```
Thread 4 says hello
Thread 3 says hello
Thread 8 says hello
Thread 2 says hello
Thread 7 says hello
Thread 9 says hello
Thread 5 says hello
Thread 1 says hello
Thread 6 says hello
Thread 0 says hello
```



python3 helper.py experiments/io_uring_gpu_no_poll
output:

```
ring_mem host 0x79e509c00000
initial ring dword=0
sqe_mem host 0x79e509800000
ring_fd=5
host sq_ring_ptr=0x79e509c00000 sqes=0x79e509800000
>>uring sq address=0x79e509800000 idx=0 tail=0
>>uring sq address=0x79e509800040 idx=1 tail=1
host tail before kernel=2 cache=2
Hello from the CPU...
Hello again from CPU...
Hello from the CPkernel launched!
device sq_ring_ptr=0x79e509c00000 sqes=0x79e509800000
>>uring sq address=0x79e509800080 idx=2 tail=2
Done!
host tail after kernel=3 cache=3
>>uring sq address=0x79e5098000c0 idx=3 tail=3
Hello from the device!
```


python3 helper.py experiments/io_uring_svm_cpu_only
```
ring_mem host 0x7f1829800000
initial ring dword=0
sqe_mem host 0x7f1829400000
ring_fd=5
setup: sq_ring_ptr=0x7f1829800000 sqe_ptr=0x7f1829400000
setup: sring_head=0x7f1829800000 tail=0x7f1829800004 mask=0x7f1829800010 array=0x7f1829800240
setup: ring_fd=5 head=0 tail=0
host sq_ring_ptr=0x7f1829800000 sqes=0x7f1829400000
Hello from the CPU using SVM pointer...host tail=1 cache=1
cqe idx=0 res=39
```

python3 helper.py experiments/omp_deviceptr_demo
```
omptarget --> Init offload library!
OMPT --> Entering connectLibrary
OMPT --> OMPT: Trying to load library libomp.so
OMPT --> OMPT: Trying to get address of connection routine ompt_libomp_connect
OMPT --> OMPT: Library connection handle = 0x75035b3ac790
OMPT --> Exiting connectLibrary
omptarget --> Loading RTLs...
omptarget --> RTLs loaded!
omptarget --> Registered plugin AMDGPU with 2 visible device(s)
omptarget --> Image 0x0000604fa66030d8 is compatible with RTL AMDGPU device 0!
omptarget --> Registering image 0x0000604fa66030d8 with RTL AMDGPU!
omptarget --> Image 0x0000604fa66030d8 is compatible with RTL AMDGPU device 1!
omptarget --> Registering image 0x0000604fa66030d8 with RTL AMDGPU!
omptarget --> Done registering entries!
omptarget --> Entering target region for device -1 with entry point 0x0000604fa6603004
omptarget --> Default TARGET OFFLOAD policy is now mandatory (devices were found)
omptarget --> Use default device id 0
omptarget --> Call to omp_get_num_devices returning 2
omptarget --> Call to omp_get_num_devices returning 2
omptarget --> Call to omp_get_initial_device returning 2
omptarget --> Entry  0: Base=0x00007fff2cb07b68, Begin=0x00007fff2cb07b68, Size=4, Type=0x22, Name=isDevice
omptarget --> Trans table 0x604fa660b048 : 0x604fa660b080
PluginInterface --> Load data from image 0x0000604fa66030d8
PluginInterface --> MemoryManagerTy::allocate: size 48 with host pointer 0x0000000000000000.
PluginInterface --> findBucket: Size 48 is floored to 32.
PluginInterface --> Cannot find a node in the FreeLists. Allocate on device.
PluginInterface --> Node address 0x0000604fda2ca400, target pointer 0x0000750353c7c000, size 48
PluginInterface --> MemoryManagerTy::free: target memory 0x0000750353c7c000.
PluginInterface --> findBucket: Size 48 is floored to 32.
PluginInterface --> Found its node 0x0000604fda2ca400. Insert it to bucket 4.
PluginInterface --> Successfully write 48 bytes associated with global symbol '__omp_rtl_device_environment' to the device (0x750353c90168 -> 0x7fff2cb06dd8).
PluginInterface --> MemoryManagerTy::allocate: size 536870912 with host pointer 0x0000000000000000.
PluginInterface --> 536870912 is greater than the threshold 8192. Allocate it directly from device
PluginInterface --> Got target pointer 0x0000750112600000. Return directly.
PluginInterface --> MemoryManagerTy::allocate: size 32 with host pointer 0x0000000000000000.
PluginInterface --> findBucket: Size 32 is floored to 32.
PluginInterface --> Cannot find a node in the FreeLists. Allocate on device.
PluginInterface --> Node address 0x0000604fda2cb320, target pointer 0x0000750353c78000, size 32
PluginInterface --> MemoryManagerTy::free: target memory 0x0000750353c78000.
PluginInterface --> findBucket: Size 32 is floored to 32.
PluginInterface --> Found its node 0x0000604fda2cb320. Insert it to bucket 4.
PluginInterface --> Successfully write 32 bytes associated with global symbol '__omp_rtl_device_memory_pool_tracker' to the device (0x750353c90148 -> 0x604fda17d998).
PluginInterface --> MemoryManagerTy::allocate: size 16 with host pointer 0x0000000000000000.
PluginInterface --> findBucket: Size 16 is floored to 16.
PluginInterface --> Cannot find a node in the FreeLists. Allocate on device.
PluginInterface --> Node address 0x0000604fda2c8580, target pointer 0x0000750353c76000, size 16
PluginInterface --> MemoryManagerTy::free: target memory 0x0000750353c76000.
PluginInterface --> findBucket: Size 16 is floored to 16.
PluginInterface --> Found its node 0x0000604fda2c8580. Insert it to bucket 3.
PluginInterface --> Successfully write 16 bytes associated with global symbol '__omp_rtl_device_memory_pool' to the device (0x750353c90138 -> 0x604fda17d988).
PluginInterface --> Global symbol '__omp_offloading_803_18ad6c_main_l6_kernel_environment' was found in the ELF image and 48 bytes will copied from 0x604fa6604128 to 0x604fda17baa8.
TARGET AMDGPU RTL --> ELFABIVersion: 3
omptarget --> Entry point 0x0000604fa6603004 maps to __omp_offloading_803_18ad6c_main_l6 (0x0000604fda17ba88)
omptarget --> loop trip count is 0.
omptarget --> Looking up mapping(HstPtrBegin=0x00007fff2cb07b68, Size=4)...
PluginInterface --> MemoryManagerTy::allocate: size 4 with host pointer 0x00007fff2cb07b68.
PluginInterface --> findBucket: Size 4 is floored to 4.
PluginInterface --> findBucket: Size 4 goes to bucket 0
PluginInterface --> Cannot find a node in the FreeLists. Allocate on device.
PluginInterface --> Node address 0x0000604fda2c63c0, target pointer 0x0000750151000000, size 4
omptarget --> Creating new map entry with HstPtrBase=0x00007fff2cb07b68, HstPtrBegin=0x00007fff2cb07b68, TgtAllocBegin=0x0000750151000000, TgtPtrBegin=0x0000750151000000, Size=4, DynRefCount=1, HoldRefCount=0, Name=isDevice
omptarget --> Notifying about new mapping: HstPtr=0x00007fff2cb07b68, Size=4
omptarget --> There are 4 bytes allocated at target address 0x0000750151000000 - is new
omptarget --> Looking up mapping(HstPtrBegin=0x00007fff2cb07b68, Size=4)...
omptarget --> Mapping exists with HstPtrBegin=0x00007fff2cb07b68, TgtPtrBegin=0x0000750151000000, Size=4, DynRefCount=1 (update suppressed), HoldRefCount=0
omptarget --> Obtained target argument 0x0000750151000000 from host pointer 0x00007fff2cb07b68
omptarget --> Launching target execution __omp_offloading_803_18ad6c_main_l6 with pointer 0x0000604fda17ba88 (index=0).
PluginInterface --> Launching kernel __omp_offloading_803_18ad6c_main_l6 with [1,1,1] blocks and [256,1,1] threads in Generic mode
PluginInterface --> MemoryManagerTy::allocate: size 272 with host pointer 0x0000000000000000.
PluginInterface --> findBucket: Size 272 is floored to 256.
PluginInterface --> Cannot find a node in the FreeLists. Allocate on device.
PluginInterface --> Node address 0x0000604fda2cb000, target pointer 0x0000750353c74000, size 272
omptarget --> Looking up mapping(HstPtrBegin=0x00007fff2cb07b68, Size=4)...
omptarget --> Mapping exists with HstPtrBegin=0x00007fff2cb07b68, TgtPtrBegin=0x0000750151000000, Size=4, DynRefCount=0 (decremented, delayed deletion), HoldRefCount=0
omptarget --> There are 4 bytes allocated at target address 0x0000750151000000 - is last
omptarget --> Moving 4 bytes (tgt:0x0000750151000000) -> (hst:0x00007fff2cb07b68)
omptarget --> Call to omp_get_num_devices returning 2
omptarget --> Call to omp_get_initial_device returning 2
PluginInterface --> MemoryManagerTy::allocate: size 4 with host pointer 0x0000000000000000.
PluginInterface --> findBucket: Size 4 is floored to 4.
PluginInterface --> findBucket: Size 4 goes to bucket 0
PluginInterface --> Cannot find a node in the FreeLists. Allocate on device.
PluginInterface --> Node address 0x0000604fda2cafb0, target pointer 0x0000750353c72000, size 4
PluginInterface --> MemoryManagerTy::free: target memory 0x0000750353c74000.
PluginInterface --> findBucket: Size 272 is floored to 256.
PluginInterface --> Found its node 0x0000604fda2cb000. Insert it to bucket 7.
PluginInterface --> MemoryManagerTy::free: target memory 0x0000750353c72000.
PluginInterface --> findBucket: Size 4 is floored to 4.
PluginInterface --> findBucket: Size 4 goes to bucket 0
PluginInterface --> Found its node 0x0000604fda2cafb0. Insert it to bucket 0.
omptarget --> Removing map entry with HstPtrBegin=0x00007fff2cb07b68, TgtPtrBegin=0x0000750151000000, Size=4, Name=isDevice
omptarget --> Deleting tgt data 0x0000750151000000 of size 4 by freeing allocation starting at 0x0000750151000000
PluginInterface --> MemoryManagerTy::free: target memory 0x0000750151000000.
PluginInterface --> findBucket: Size 4 is floored to 4.
PluginInterface --> findBucket: Size 4 goes to bucket 0
PluginInterface --> Found its node 0x0000604fda2c63c0. Insert it to bucket 0.
omptarget --> Notifying about an unmapping: HstPtr=0x00007fff2cb07b68
omptarget --> Unloading target library!
omptarget --> Unregistered image 0x0000604fa66030d8 from RTL
omptarget --> Done unregistering images!
omptarget --> Removing translation table for descriptor 0x0000604fa660b048
omptarget --> Done unregistering library!
omptarget --> Deinit offload library!
omptarget --> Unloading RTLs...
omptarget --> RTLs unloaded!
0
```


python3 helper.py experiments/svm_target_edit_buffer
```
host   ptr=0x77b20fe64000 first=2
device ptr=0x77b20fe64000 value=2
device new value=1
host after=1
```

python3 helper.py experiments/svm_migration_cg

```
Migrated page timing:
Average time: 4.76582 milliseconds
Minimum time: 4.61207 milliseconds
Maximum time: 10.3203 milliseconds
Coarsegrained timing:
Average time: 2.68007 milliseconds
Minimum time: 2.1344 milliseconds
Maximum time: 7.87332 milliseconds
```

python3 helper.py experiments/svm_migration_fg
```
Migrated (mmap + SVM) timing:
Average: 68.9807 ms
Minimum: 67.8347 ms
Maximum: 74.8041 ms
Coarse-grained VRAM timing:
Average: 68.6895 ms
Minimum: 68.0743 ms
Maximum: 72.0279 ms
Fine-grained host timing:
Average: 68.8172 ms
Minimum: 67.8055 ms
Maximum: 72.3524 ms
```