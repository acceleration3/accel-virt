/* One host GPU, as the executor library sees it. What is asserted is what a
 * backend would act on: the name the device uses, what it may run, and where
 * it sat in the host's enumeration. */
#include "device/abi/av_executor.h"
#include "executor/host_gpu.hpp"
#include "harness/executor/mock_host_apis.hpp"
#include "harness/executor/host_gpu_probe.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>

using av::HostGpu;
using av::HostGpuProbe;
using av::MockHostApis;

namespace {

constexpr std::uint8_t intel[16] = {0x86, 0x80, 0xa6, 0x46, 0x0c, 0, 0, 0, 0, 0x02, 0, 0, 0, 0, 0, 0};
constexpr std::uint8_t nvidia[16] = {0x7b, 0x7a, 0xd2, 0xc5, 0x42, 0x19, 0x8c, 0x80,
                                     0xc4, 0xfd, 0xa6, 0x4b, 0x87, 0x75, 0xae, 0xdb};
constexpr std::uint32_t all_three =
    AV_EXECUTOR_SUB_API_VULKAN | AV_EXECUTOR_SUB_API_OPENGL | AV_EXECUTOR_SUB_API_OPENCL;

}  // namespace

TEST(host_gpu, answers_the_name_the_device_uses_and_what_it_runs)
{ MockHostApis apis;
  apis.add_gpu(intel, all_three);
  HostGpu gpu;
  gpu.describe(apis, 0u);
  CHECK(gpu.matches(intel));
  CHECK(!gpu.matches(nvidia));
  CHECK_EQ(gpu.sub_apis(), all_three);
  CHECK_EQ(gpu.index(), 0u);
  CHECK(gpu.invariants_hold()); }

/* The index is where the GPU sat, which is what a backend opens it by again. */
TEST(host_gpu, remembers_where_it_sat_in_the_enumeration)
{ MockHostApis apis;
  apis.add_gpu(intel, AV_EXECUTOR_SUB_API_VULKAN);
  apis.add_gpu(nvidia, AV_EXECUTOR_SUB_API_OPENGL);
  HostGpu gpu;
  gpu.describe(apis, 1u);
  CHECK(gpu.matches(nvidia));
  CHECK(!gpu.matches(intel));
  CHECK_EQ(gpu.sub_apis(), std::uint32_t{AV_EXECUTOR_SUB_API_OPENGL});
  CHECK_EQ(gpu.index(), 1u);
  CHECK(gpu.invariants_hold()); }

TEST(host_gpu, a_gpu_nobody_asked_about_matches_nothing)
{ HostGpu gpu;
  CHECK(!gpu.matches(intel));
  CHECK(!gpu.matches(nullptr));
  CHECK_EQ(gpu.sub_apis(), 0u);
  CHECK(gpu.invariants_hold()); }

/* A GPU never described that claims to run something is one a context could
 * be opened on with no device behind it. */
TEST(host_gpu_invariants, an_undescribed_gpu_that_runs_something_breaks_them)
{ HostGpu gpu;
  HostGpuProbe(gpu).set_sub_apis(AV_EXECUTOR_SUB_API_VULKAN);
  CHECK(!gpu.invariants_hold()); }

/* Described, it may run anything or nothing. */
TEST(host_gpu_invariants, a_described_gpu_holds_them_whatever_it_runs)
{ MockHostApis apis;
  apis.add_gpu(intel, 0u);
  HostGpu gpu;
  gpu.describe(apis, 0u);
  CHECK(gpu.invariants_hold());
  HostGpuProbe(gpu).set_sub_apis(all_three);
  CHECK(gpu.invariants_hold()); }
