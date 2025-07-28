#include <cassert>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <hsa/hsa.h>
#include <hsa/hsa_ext_amd.h>
#include <iostream>
#include <omp.h>
#include <sys/mman.h>
#include <unistd.h>

static void handle_error(hsa_status_t code, uint32_t line = 0) {
  if (code == HSA_STATUS_SUCCESS || code == HSA_STATUS_INFO_BREAK) return;
  const char *desc;
  if (hsa_status_string(code, &desc) != HSA_STATUS_SUCCESS) desc = "Unknown";
  fprintf(stderr, "%s:%d %s\n", __FILE__, line, desc);
  exit(EXIT_FAILURE);
}

template <typename E, typename F, typename C>
hsa_status_t iterate(F fn, C cb) {
  auto l = [](E e, void *d) -> hsa_status_t {
    C *u = static_cast<C *>(d);
    return (*u)(e);
  };
  return fn(l, static_cast<void *>(&cb));
}

template <typename E, typename F, typename A, typename C>
hsa_status_t iterate(F fn, A arg, C cb) {
  auto l = [](E e, void *d) -> hsa_status_t {
    C *u = static_cast<C *>(d);
    return (*u)(e);
  };
  return fn(arg, l, static_cast<void *>(&cb));
}

template <typename C>
hsa_status_t iterate_agents(C cb) { return iterate<hsa_agent_t>(hsa_iterate_agents, cb); }

template <typename C>
hsa_status_t iterate_agent_memory_pools(hsa_agent_t a, C cb) {
  return iterate<hsa_amd_memory_pool_t>(hsa_amd_agent_iterate_memory_pools, a, cb);
}

template <hsa_device_type_t DT>
hsa_status_t get_agent(hsa_agent_t *out) {
  auto cb = [&](hsa_agent_t a) -> hsa_status_t {
    hsa_device_type_t type;
    if (hsa_agent_get_info(a, HSA_AGENT_INFO_DEVICE, &type)) return HSA_STATUS_ERROR;
    if (type != DT) return HSA_STATUS_SUCCESS;

    if (DT == HSA_DEVICE_TYPE_GPU) {
      hsa_agent_feature_t f;
      if (hsa_agent_get_info(a, HSA_AGENT_INFO_FEATURE, &f)) return HSA_STATUS_ERROR;
      if (!(f & HSA_AGENT_FEATURE_KERNEL_DISPATCH)) return HSA_STATUS_SUCCESS;
    }
    *out = a;
    return HSA_STATUS_INFO_BREAK;
  };
  return iterate_agents(cb);
}

template <hsa_amd_memory_pool_global_flag_t FL>
hsa_status_t get_agent_memory_pool(hsa_agent_t ag, hsa_amd_memory_pool_t *out) {
  auto cb = [&](hsa_amd_memory_pool_t p) {
    hsa_amd_segment_t seg;
    uint32_t flags;
    if (hsa_amd_memory_pool_get_info(p, HSA_AMD_MEMORY_POOL_INFO_SEGMENT, &seg)) return HSA_STATUS_ERROR;
    if (seg != HSA_AMD_SEGMENT_GLOBAL) return HSA_STATUS_SUCCESS;
    if (hsa_amd_memory_pool_get_info(p, HSA_AMD_MEMORY_POOL_INFO_GLOBAL_FLAGS, &flags)) return HSA_STATUS_ERROR;
    if (flags & FL) *out = p;
    return HSA_STATUS_SUCCESS;
  };
  return iterate_agent_memory_pools(ag, cb);
}

static void print_time(const char *s, unsigned long ns) {
  if (ns < 1'000)       std::cout << s << ns                    << " ns\n";
  else if (ns < 1'000'000) std::cout << s << ns / 1'000.0      << " μs\n";
  else if (ns < 1'000'000'000) std::cout << s << ns / 1'000'000.0 << " ms\n";
  else                     std::cout << s << ns / 1'000'000'000.0 << " s\n";
}

static void check_timing(uint8_t *ptr, size_t n) {
  const int reps = 1024;
  unsigned long sum = 0, mn = (unsigned long)-1, mx = 0;
  for (int r = 0; r < reps; ++r) {
    auto st = std::chrono::high_resolution_clock::now();
#pragma omp target teams distribute parallel for is_device_ptr(ptr)
    for (size_t i = 0; i < n; ++i) ptr[i] = -1;
    auto ed = std::chrono::high_resolution_clock::now();
    unsigned long delta = std::chrono::duration_cast<std::chrono::nanoseconds>(ed - st).count();
    sum += delta; mn = std::min(mn, delta); mx = std::max(mx, delta);
  }
  print_time("Average: ", sum / reps);
  print_time("Minimum: ", mn);
  print_time("Maximum: ", mx);
}

int main() {
  /* discover agents */
  hsa_agent_t gpu{}, cpu{};
  handle_error(get_agent<HSA_DEVICE_TYPE_GPU>(&gpu), __LINE__);
  handle_error(get_agent<HSA_DEVICE_TYPE_CPU>(&cpu), __LINE__);

  // Obtain pools similar to the example from the `hsa_amd_memory_pool` API. The
  // fine-grained pool acts as pinned host memory that the device can DMA from,
  // the coarse-grained pool is used for allocations on the device and the
  // kernargs pool is used to pass kernel arguments.
  hsa_amd_memory_pool_t kernargs_pool{};
  hsa_amd_memory_pool_t fg_pool{};
  hsa_amd_memory_pool_t cg_pool{};
  handle_error(get_agent_memory_pool<HSA_AMD_MEMORY_POOL_GLOBAL_FLAG_KERNARG_INIT>(cpu, &kernargs_pool), __LINE__);
  handle_error(get_agent_memory_pool<HSA_AMD_MEMORY_POOL_GLOBAL_FLAG_FINE_GRAINED>(cpu, &fg_pool), __LINE__);
  handle_error(get_agent_memory_pool<HSA_AMD_MEMORY_POOL_GLOBAL_FLAG_COARSE_GRAINED>(gpu, &cg_pool), __LINE__);

  /* check that fine-grained pool is allocatable */
  bool fg_alloc_ok = false;
  handle_error(hsa_amd_memory_pool_get_info(fg_pool, HSA_AMD_MEMORY_POOL_INFO_RUNTIME_ALLOC_ALLOWED, &fg_alloc_ok), __LINE__);
  assert(fg_alloc_ok && "fine-grained pool not allocatable");

  /* misc capability queries (unchanged) */
  bool svm_supported{}, svm_default{};
  handle_error(hsa_system_get_info(HSA_AMD_SYSTEM_INFO_SVM_SUPPORTED, &svm_supported), __LINE__);
  handle_error(hsa_system_get_info(HSA_AMD_SYSTEM_INFO_SVM_ACCESSIBLE_BY_DEFAULT, &svm_default), __LINE__);

  /* allocate test sizes */
  size_t size = 128ul * 1024 * 1024 /* 128 MiB for mmap-SVM demo */;
  uint8_t *mmap_ptr = static_cast<uint8_t *>(mmap(nullptr, size, PROT_READ | PROT_WRITE,
                                                  MAP_SHARED | MAP_ANONYMOUS, -1, 0));
  assert(mmap_ptr != MAP_FAILED);

  /* mark mmap region GPU-accessible */
  hsa_amd_svm_attribute_pair_t attr[] = {
      {HSA_AMD_SVM_ATTRIB_AGENT_ACCESSIBLE, __builtin_bit_cast(uint64_t, gpu)}};
  handle_error(hsa_amd_svm_attributes_set(mmap_ptr, size, attr, 1), __LINE__);

  /* prefetch */
  hsa_signal_t sig{0};
  // handle_error(hsa_amd_svm_prefetch_async(mmap_ptr, size, gpu, 0, nullptr, sig), __LINE__);

  /* allocate coarse-grained VRAM */
  void *cg_ptr = nullptr;
  handle_error(hsa_amd_memory_pool_allocate(cg_pool, size, 0, &cg_ptr), __LINE__);
  handle_error(hsa_amd_agents_allow_access(1, &gpu, nullptr, cg_ptr), __LINE__);

  // Allocate a coarse-grained integer on the device to mimic the return value
  // buffer from the example code and initialize it to zero.
  void *dev_ret = nullptr;
  handle_error(hsa_amd_memory_pool_allocate(cg_pool, sizeof(int), 0, &dev_ret), __LINE__);
  hsa_amd_memory_fill(dev_ret, 0, /*count=*/1);
  handle_error(hsa_amd_agents_allow_access(1, &gpu, nullptr, dev_ret), __LINE__);

  /* allocate fine-grained DRAM */
  void *fg_ptr = nullptr;
  handle_error(hsa_amd_memory_pool_allocate(fg_pool, size, 0, &fg_ptr), __LINE__);
  handle_error(hsa_amd_agents_allow_access(1, &gpu, nullptr, fg_ptr), __LINE__);

  // Allocate fine-grained memory to simulate the RPC buffer shared between the
  // host and device. The exact size is arbitrary for this test.
  void *rpc_buffer = nullptr;
  const size_t rpc_size = 4096;
  handle_error(hsa_amd_memory_pool_allocate(fg_pool, rpc_size, 0, &rpc_buffer), __LINE__);
  handle_error(hsa_amd_agents_allow_access(1, &gpu, nullptr, rpc_buffer), __LINE__);

  /* benchmark */
  std::cout << "Migrated (mmap + SVM) timing:\n";
  check_timing(mmap_ptr, size);

  std::cout << "Coarse-grained VRAM timing:\n";
  check_timing(static_cast<uint8_t *>(cg_ptr), size);

  std::cout << "Fine-grained host timing:\n";
  check_timing(static_cast<uint8_t *>(fg_ptr), size);

  /* cleanup */
  handle_error(hsa_amd_memory_pool_free(cg_ptr), __LINE__);
  handle_error(hsa_amd_memory_pool_free(fg_ptr), __LINE__);
  handle_error(hsa_amd_memory_pool_free(dev_ret), __LINE__);
  handle_error(hsa_amd_memory_pool_free(rpc_buffer), __LINE__);
  std::cout << (int)*mmap_ptr << '\n';
  munmap(mmap_ptr, size);
}
