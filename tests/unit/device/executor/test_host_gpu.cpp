/* One host GPU, as the executor library sees it. What is asserted is what a
 * backend would act on: the name the device uses, what it may run, and what it
 * may be handed without a copy. */
#include "device/abi/rvk_executor.h"
#include "device/executor/host_gpu.hpp"
#include "harness/executor/mock_host_apis.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>

using av::HostGpu;
using av::MockHostApis;
using av::Status;

namespace {

constexpr std::uint8_t intel[16] = {0x86, 0x80, 0xa6, 0x46, 0x0c, 0, 0, 0, 0, 0x02, 0, 0, 0, 0, 0, 0};
constexpr std::uint8_t nvidia[16] = {0x7b, 0x7a, 0xd2, 0xc5, 0x42, 0x19, 0x8c, 0x80,
                                     0xc4, 0xfd, 0xa6, 0x4b, 0x87, 0x75, 0xae, 0xdb};
constexpr std::uint32_t all_three =
    RVK_EXECUTOR_SUB_API_VULKAN | RVK_EXECUTOR_SUB_API_OPENGL | RVK_EXECUTOR_SUB_API_OPENCL;

}  // namespace

TEST(host_gpu, answers_the_name_the_device_uses_and_what_it_runs)
{ MockHostApis apis;
  apis.add_gpu(intel, all_three, RVK_BLOB_HOST_POINTER | RVK_BLOB_DMA_BUF, RVK_BLOB_DMA_BUF,
               RVK_BLOB_DMA_BUF);
  HostGpu gpu;
  CHECK_STATUS(gpu.initialize(apis, 0u), Status::ok);
  CHECK(gpu.matches(intel));
  CHECK(!gpu.matches(nvidia));
  CHECK_EQ(gpu.sub_apis(), all_three);
  CHECK_EQ(gpu.index(), 0u);
  CHECK(gpu.invariants_hold()); }

/* The measured asymmetry this whole design exists for: Vulkan takes a host
 * pointer where OpenGL and OpenCL take nothing. */
TEST(host_gpu, import_kinds_differ_per_api_on_one_gpu)
{ MockHostApis apis;
  apis.add_gpu(nvidia, all_three, RVK_BLOB_HOST_POINTER, 0u, 0u);
  HostGpu gpu;
  CHECK_STATUS(gpu.initialize(apis, 0u), Status::ok);
  CHECK_EQ(gpu.import_kinds(RVK_EXECUTOR_SUB_API_VULKAN), std::uint32_t{RVK_BLOB_HOST_POINTER});
  CHECK_EQ(gpu.import_kinds(RVK_EXECUTOR_SUB_API_OPENGL), 0u);
  CHECK_EQ(gpu.import_kinds(RVK_EXECUTOR_SUB_API_OPENCL), 0u);
  CHECK(gpu.invariants_hold()); }

/* A sub-API this GPU cannot run imports nothing, whatever a loader would say
 * if asked - and it is not asked. */
TEST(host_gpu, an_api_it_cannot_run_imports_nothing)
{ MockHostApis apis;
  apis.add_gpu(intel, RVK_EXECUTOR_SUB_API_VULKAN, RVK_BLOB_HOST_POINTER, RVK_BLOB_DMA_BUF,
               RVK_BLOB_DMA_BUF);
  HostGpu gpu;
  CHECK_STATUS(gpu.initialize(apis, 0u), Status::ok);
  CHECK_EQ(gpu.sub_apis(), std::uint32_t{RVK_EXECUTOR_SUB_API_VULKAN});
  CHECK_EQ(gpu.import_kinds(RVK_EXECUTOR_SUB_API_OPENGL), 0u);
  CHECK(gpu.invariants_hold()); }

/* A value that is not one of the three bits, which is what a hostile or a
 * newer device could name. */
TEST(host_gpu, an_api_nobody_defined_imports_nothing)
{ MockHostApis apis;
  apis.add_gpu(intel, all_three, RVK_BLOB_HOST_POINTER, RVK_BLOB_DMA_BUF, RVK_BLOB_DMA_BUF);
  HostGpu gpu;
  CHECK_STATUS(gpu.initialize(apis, 0u), Status::ok);
  CHECK_EQ(gpu.import_kinds(0u), 0u);
  CHECK_EQ(gpu.import_kinds(8u), 0u);
  CHECK_EQ(gpu.import_kinds(RVK_EXECUTOR_SUB_API_VULKAN | RVK_EXECUTOR_SUB_API_OPENGL), 0u); }

TEST(host_gpu, an_index_the_host_does_not_have_is_not_found)
{ MockHostApis apis;
  apis.add_gpu(intel, all_three, 0u, 0u, 0u);
  HostGpu gpu;
  CHECK_STATUS(gpu.initialize(apis, 1u), Status::not_found);
  /* And it stays unknown rather than half-filled. */
  CHECK(!gpu.matches(intel));
  CHECK_EQ(gpu.sub_apis(), 0u);
  CHECK(gpu.invariants_hold()); }

TEST(host_gpu, a_gpu_nobody_asked_about_matches_nothing)
{ HostGpu gpu;
  CHECK(!gpu.matches(intel));
  CHECK(!gpu.matches(nullptr));
  CHECK_EQ(gpu.sub_apis(), 0u);
  CHECK(gpu.invariants_hold()); }
