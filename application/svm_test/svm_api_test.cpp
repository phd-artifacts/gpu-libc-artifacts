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

/// Print the error code and exit if \p code indicates an error.
static void handle_error(hsa_status_t code, uint32_t line = 0) {
  if (code == HSA_STATUS_SUCCESS || code == HSA_STATUS_INFO_BREAK)
    return;

  const char *desc;
  if (hsa_status_string(code, &desc) != HSA_STATUS_SUCCESS)
    desc = "Unknown error";
  fprintf(stderr, "%s:%d %s\n", __FILE__, line, desc);
  exit(EXIT_FAILURE);
}

/// Generic interface for iterating using the HSA callbacks.
template <typename elem_ty, typename func_ty, typename callback_ty>
hsa_status_t iterate(func_ty func, callback_ty cb) {
  auto l = [](elem_ty elem, void *data) -> hsa_status_t {
    callback_ty *unwrapped = static_cast<callback_ty *>(data);
    return (*unwrapped)(elem);
  };
  return func(l, static_cast<void *>(&cb));
}

/// Generic interface for iterating using the HSA callbacks.
template <typename elem_ty, typename func_ty, typename func_arg_ty,
          typename callback_ty>
hsa_status_t iterate(func_ty func, func_arg_ty func_arg, callback_ty cb) {
  auto l = [](elem_ty elem, void *data) -> hsa_status_t {
    callback_ty *unwrapped = static_cast<callback_ty *>(data);
    return (*unwrapped)(elem);
  };
  return func(func_arg, l, static_cast<void *>(&cb));
}

/// Iterate through all availible agents.
template <typename callback_ty>
hsa_status_t iterate_agents(callback_ty callback) {
  return iterate<hsa_agent_t>(hsa_iterate_agents, callback);
}

/// Iterate through all availible memory pools.
template <typename callback_ty>
hsa_status_t iterate_agent_memory_pools(hsa_agent_t agent, callback_ty cb) {
  return iterate<hsa_amd_memory_pool_t>(hsa_amd_agent_iterate_memory_pools,
                                        agent, cb);
}

template <hsa_device_type_t flag>
hsa_status_t get_agent(hsa_agent_t *output_agent) {
  // Find the first agent with a matching device type.
  auto cb = [&](hsa_agent_t hsa_agent) -> hsa_status_t {
    hsa_device_type_t type;
    hsa_status_t status =
        hsa_agent_get_info(hsa_agent, HSA_AGENT_INFO_DEVICE, &type);
    if (status != HSA_STATUS_SUCCESS)
      return status;

    if (type == flag) {
      // Ensure that a GPU agent supports kernel dispatch packets.
      if (type == HSA_DEVICE_TYPE_GPU) {
        hsa_agent_feature_t features;
        status =
            hsa_agent_get_info(hsa_agent, HSA_AGENT_INFO_FEATURE, &features);
        if (status != HSA_STATUS_SUCCESS)
          return status;
        if (features & HSA_AGENT_FEATURE_KERNEL_DISPATCH)
          *output_agent = hsa_agent;
      } else {
        *output_agent = hsa_agent;
      }
      return HSA_STATUS_INFO_BREAK;
    }
    return HSA_STATUS_SUCCESS;
  };

  return iterate_agents(cb);
}

/// Retrieve a global memory pool with a \p flag from the agent.
template <hsa_amd_memory_pool_global_flag_t flag>
hsa_status_t get_agent_memory_pool(hsa_agent_t agent,
                                   hsa_amd_memory_pool_t *output_pool) {
  auto cb = [&](hsa_amd_memory_pool_t memory_pool) {
    uint32_t flags;
    hsa_amd_segment_t segment;
    if (auto err = hsa_amd_memory_pool_get_info(
            memory_pool, HSA_AMD_MEMORY_POOL_INFO_SEGMENT, &segment))
      return err;
    if (auto err = hsa_amd_memory_pool_get_info(
            memory_pool, HSA_AMD_MEMORY_POOL_INFO_GLOBAL_FLAGS, &flags))
      return err;

    if (segment != HSA_AMD_SEGMENT_GLOBAL)
      return HSA_STATUS_SUCCESS;

    if (flags & flag)
      *output_pool = memory_pool;

    return HSA_STATUS_SUCCESS;
  };
  return iterate_agent_memory_pools(agent, cb);
}

void print_time(const char *str, unsigned long duration) {
  if (duration < 1000)
    std::cout << str << duration << " nanoseconds" << std::endl;
  else if (duration < 1000000)
    std::cout << str << duration / 1000.0 << " microseconds" << std::endl;
  else if (duration < 1000000000)
    std::cout << str << duration / 1000000.0 << " milliseconds" << std::endl;
  else
    std::cout << str << duration / 1000000000.0 << " seconds" << std::endl;
}

void check_timing(uint8_t *ptr, size_t size) {
  int reps = 1024;
  unsigned long duration = 0;
  unsigned long min = -1;
  unsigned long max = 0;
  for (int i = 0; i < reps; ++i) {
    auto start = std::chrono::high_resolution_clock::now();
#pragma omp target teams distribute parallel for is_device_ptr(ptr)
    for (size_t i = 0; i < size; ++i)
      ptr[i] = -1;
    auto end = std::chrono::high_resolution_clock::now();
    unsigned long elapsed =
        std::chrono::duration_cast<std::chrono::nanoseconds>(end - start)
            .count();
    duration += elapsed;
    min = std::min(min, elapsed);
    max = std::max(max, elapsed);
  }

  duration /= reps;
  print_time("Average time: ", duration);
  print_time("Minimum time: ", min);
  print_time("Maximum time: ", max);
}

int main() {
  hsa_agent_t dev_agent;
  hsa_agent_t host_agent;
  if (hsa_status_t err = get_agent<HSA_DEVICE_TYPE_GPU>(&dev_agent))
    handle_error(err, __LINE__);
  if (hsa_status_t err = get_agent<HSA_DEVICE_TYPE_CPU>(&host_agent))
    handle_error(err, __LINE__);

  hsa_amd_memory_pool_t coarsegrained_pool;
  if (hsa_status_t err =
          get_agent_memory_pool<HSA_AMD_MEMORY_POOL_GLOBAL_FLAG_COARSE_GRAINED>(
              dev_agent, &coarsegrained_pool))
    handle_error(err, __LINE__);
  size_t granule_size;
  if (hsa_status_t err = hsa_amd_memory_pool_get_info(
          coarsegrained_pool, HSA_AMD_MEMORY_POOL_INFO_RUNTIME_ALLOC_GRANULE,
          &granule_size))
    handle_error(err, __LINE__);

  bool svm_supported;
  if (hsa_status_t err = hsa_system_get_info(HSA_AMD_SYSTEM_INFO_SVM_SUPPORTED,
                                             &svm_supported))
    handle_error(err, __LINE__);

  bool svm_default;
  if (hsa_status_t err = hsa_system_get_info(
          HSA_AMD_SYSTEM_INFO_SVM_ACCESSIBLE_BY_DEFAULT, &svm_default))
    handle_error(err, __LINE__);

  size_t size = 128ul * 1024 * 1024 * 1024 /* 512 MiB */;
  auto *mmap_ptr = (uint8_t *)mmap(NULL, size, PROT_READ | PROT_WRITE,
                                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  assert(mmap_ptr != MAP_FAILED);

  hsa_amd_svm_attribute_pair_t attrs[] = {
      {HSA_AMD_SVM_ATTRIB_AGENT_ACCESSIBLE,
       __builtin_bit_cast(uint64_t, dev_agent)}};

  size = 2 * 1024 * 1024;
  if (hsa_status_t err = hsa_amd_svm_attributes_set(mmap_ptr, size, attrs,
                                                    /*attribute_count=*/1))
    handle_error(err, __LINE__);

  hsa_signal_t signal{0ul};
  if (hsa_status_t err =
          hsa_amd_svm_prefetch_async(mmap_ptr, size, dev_agent,
                                     /*num_dep_signals=*/0, nullptr, signal))
    handle_error(err, __LINE__);

  void *cg_ptr;
  if (hsa_status_t err = hsa_amd_memory_pool_allocate(coarsegrained_pool, size,
                                                      /*flags=*/0, &cg_ptr))
    handle_error(err);
  hsa_amd_agents_allow_access(1, &dev_agent, nullptr, cg_ptr);

  std::cout << "Migrated page timing:\n";
  check_timing(mmap_ptr, size);
  std::cout << "Coarsegrained timing:\n";
  check_timing(reinterpret_cast<uint8_t *>(cg_ptr), size);

  if (hsa_status_t err = hsa_amd_memory_pool_free(cg_ptr))
    handle_error(err, __LINE__);
  std::cout << (int)*mmap_ptr << "\n";
  munmap(mmap_ptr, size);
}