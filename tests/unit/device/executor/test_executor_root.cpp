/* The end of the ABI that answers. What a test asserts here is what the device
 * will act on during realize: the sub-APIs that reach the capset, and the
 * import kinds that decide whether a blob is memory or a copy. */
#include "device/abi/rvk_executor.h"
#include "device/executor/executor_root.hpp"
#include "harness/executor/mock_host_apis.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>

using av::ExecutorRoot;
using av::MockHostApis;
using av::Status;

namespace {

constexpr std::uint8_t intel[16] = {0x86, 0x80, 0xa6, 0x46, 0x0c, 0, 0, 0, 0, 0x02, 0, 0, 0, 0, 0, 0};
constexpr std::uint8_t nvidia[16] = {0x7b, 0x7a, 0xd2, 0xc5, 0x42, 0x19, 0x8c, 0x80,
                                     0xc4, 0xfd, 0xa6, 0x4b, 0x87, 0x75, 0xae, 0xdb};
constexpr std::uint8_t absent[16] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                                     0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

}  // namespace

TEST(executor_root, offers_what_the_host_runs_and_imports_what_each_gpu_can)
{ MockHostApis apis;
  apis.add_gpu(intel, RVK_EXECUTOR_SUB_API_VULKAN | RVK_EXECUTOR_SUB_API_OPENCL,
               RVK_BLOB_HOST_POINTER | RVK_BLOB_DMA_BUF, 0u, RVK_BLOB_DMA_BUF);
  apis.add_gpu(nvidia, RVK_EXECUTOR_SUB_API_VULKAN, RVK_BLOB_HOST_POINTER, 0u, 0u);
  ExecutorRoot root;
  CHECK_STATUS(root.initialize(apis), Status::ok);
  CHECK_EQ(root.sub_apis(), RVK_EXECUTOR_SUB_API_VULKAN | RVK_EXECUTOR_SUB_API_OPENCL);

  /* The asymmetry the design exists for, seen through the ABI: the same API
   * imports on one GPU and copies on the other. */
  CHECK_EQ(root.import_kinds(RVK_EXECUTOR_SUB_API_OPENCL, intel), std::uint32_t{RVK_BLOB_DMA_BUF});
  CHECK_EQ(root.import_kinds(RVK_EXECUTOR_SUB_API_OPENCL, nvidia), 0u);
  CHECK(root.invariants_hold()); }

/* A host with no GPU still loads and still answers; it just offers nothing,
 * which is what stops the device describing a capset it cannot honour. */
TEST(executor_root, a_host_with_no_gpu_offers_nothing)
{ MockHostApis apis;
  ExecutorRoot root;
  CHECK_STATUS(root.initialize(apis), Status::ok);
  CHECK_EQ(root.sub_apis(), 0u);
  CHECK_EQ(root.import_kinds(RVK_EXECUTOR_SUB_API_VULKAN, intel), 0u);
  CHECK(root.invariants_hold()); }

TEST(executor_root, a_gpu_nobody_has_imports_nothing)
{ MockHostApis apis;
  apis.add_gpu(intel, RVK_EXECUTOR_SUB_API_VULKAN, RVK_BLOB_HOST_POINTER, 0u, 0u);
  ExecutorRoot root;
  CHECK_STATUS(root.initialize(apis), Status::ok);
  CHECK_EQ(root.import_kinds(RVK_EXECUTOR_SUB_API_VULKAN, absent), 0u);
  CHECK_EQ(root.import_kinds(RVK_EXECUTOR_SUB_API_VULKAN, nullptr), 0u); }

/* Before it has asked, it offers nothing rather than guessing. */
TEST(executor_root, answers_nothing_until_it_has_asked)
{ ExecutorRoot root;
  CHECK_EQ(root.sub_apis(), 0u);
  CHECK_EQ(root.import_kinds(RVK_EXECUTOR_SUB_API_VULKAN, intel), 0u);
  CHECK(root.invariants_hold()); }

/* The host is asked once. Asking twice would mean the capset and the answer
 * could disagree. */
TEST(executor_root, asks_the_host_once)
{ MockHostApis apis;
  apis.add_gpu(intel, RVK_EXECUTOR_SUB_API_VULKAN, RVK_BLOB_HOST_POINTER, 0u, 0u);
  ExecutorRoot root;
  CHECK_STATUS(root.initialize(apis), Status::ok);
  CHECK_STATUS(root.initialize(apis), Status::invalid_state);
  CHECK_EQ(root.sub_apis(), std::uint32_t{RVK_EXECUTOR_SUB_API_VULKAN}); }

/* More GPUs than the set holds is still a host worth serving, with the ones it
 * kept. */
TEST(executor_root, a_host_with_more_gpus_than_it_holds_still_answers)
{ MockHostApis apis;
  std::uint8_t uuid[16] = {};
  for (std::uint32_t i = 0u; i < MockHostApis::max_gpus; i++) {
      uuid[0] = static_cast<std::uint8_t>(i + 1u);
      apis.add_gpu(uuid, RVK_EXECUTOR_SUB_API_VULKAN, RVK_BLOB_HOST_POINTER, 0u, 0u);
  }
  ExecutorRoot root;
  CHECK_STATUS(root.initialize(apis), Status::ok);
  CHECK_EQ(root.sub_apis(), std::uint32_t{RVK_EXECUTOR_SUB_API_VULKAN});
  CHECK(root.invariants_hold()); }
