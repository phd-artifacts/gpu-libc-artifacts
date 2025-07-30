#include <cstdio>
#include <cstdlib>
#include <hsa/hsa.h>
#include <hsa/hsa_ext_amd.h>
#include <assert.h>
#include <omp.h>

static void handle_error(hsa_status_t code, uint32_t line = 0) {
  if (code == HSA_STATUS_SUCCESS || code == HSA_STATUS_INFO_BREAK) return;
  const char *desc = nullptr;
  if (hsa_status_string(code, &desc) != HSA_STATUS_SUCCESS) desc = "Unknown";
  fprintf(stderr, "HSA error at %u: %s\n", line, desc ? desc : "");
  std::abort();
}

template <typename E, typename F, typename C>
static hsa_status_t iterate(F fn, C cb) {
  auto l = [](E e, void *d) -> hsa_status_t {
    C *u = static_cast<C *>(d);
    return (*u)(e);
  };
  return fn(l, static_cast<void *>(&cb));
}

template <typename E, typename F, typename A, typename C>
static hsa_status_t iterate(F fn, A arg, C cb) {
  auto l = [](E e, void *d) -> hsa_status_t {
    C *u = static_cast<C *>(d);
    return (*u)(e);
  };
  return fn(arg, l, static_cast<void *>(&cb));
}

template <typename C>
static hsa_status_t iterate_agents(C cb) {
  return iterate<hsa_agent_t>(hsa_iterate_agents, cb);
}

template <typename C>
static hsa_status_t iterate_agent_memory_pools(hsa_agent_t a, C cb) {
  return iterate<hsa_amd_memory_pool_t>(hsa_amd_agent_iterate_memory_pools, a, cb);
}

template <hsa_device_type_t DT>
static hsa_status_t get_agent(hsa_agent_t *out) {
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
static hsa_status_t get_agent_memory_pool(hsa_agent_t ag, hsa_amd_memory_pool_t *out) {
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

int main() {
  handle_error(hsa_init(), __LINE__);
  hsa_agent_t gpu{}, cpu{};
  handle_error(get_agent<HSA_DEVICE_TYPE_GPU>(&gpu), __LINE__);
  handle_error(get_agent<HSA_DEVICE_TYPE_CPU>(&cpu), __LINE__);

  hsa_amd_memory_pool_t fg_pool{};
  handle_error(get_agent_memory_pool<HSA_AMD_MEMORY_POOL_GLOBAL_FLAG_FINE_GRAINED>(cpu, &fg_pool), __LINE__);

  const size_t count = 4;
  int *buf = nullptr;
  handle_error(hsa_amd_memory_pool_allocate(fg_pool, count * sizeof(int), 0, (void **)&buf), __LINE__);
  handle_error(hsa_amd_agents_allow_access(1, &gpu, nullptr, buf), __LINE__);

  for (size_t i = 0; i < count; ++i)
    buf[i] = 2;

  fprintf(stderr, "host   ptr=%p first=%d\n", (void *)buf, buf[0]);

#pragma omp target
  {
    int is_initial_device = omp_is_initial_device();
    assert(!is_initial_device && "NOT ON DEVICE");
    printf("device ptr=%p value=%d\n", buf, buf[0]);
    buf[0] = 1;
    printf("device new value=%d\n", buf[0]);
  }

  fprintf(stderr, "host after=%d\n", buf[0]);

  hsa_amd_memory_pool_free(buf);
  hsa_shut_down();
  return 0;
}


