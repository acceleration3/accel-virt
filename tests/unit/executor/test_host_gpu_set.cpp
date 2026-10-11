/* Every host GPU this process found. The device names one by its Vulkan device
 * UUID, because enumeration order does not survive a reboot. */
#include "device/abi/av_executor.h"
#include "executor/host_gpu_set.hpp"
#include "harness/executor/mock_host_apis.hpp"
#include "harness/executor/host_gpu_probe.hpp"
#include "harness/executor/host_gpu_set_probe.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>

using av::HostGpu;
using av::HostGpuProbe;
using av::HostGpuSet;
using av::HostGpuSetProbe;
using av::MockHostApis;

namespace {

constexpr std::uint8_t intel[16] = {0x86, 0x80, 0xa6, 0x46, 0x0c, 0, 0, 0, 0, 0x02, 0, 0, 0, 0, 0, 0};
constexpr std::uint8_t nvidia[16] = {0x7b, 0x7a, 0xd2, 0xc5, 0x42, 0x19, 0x8c, 0x80,
                                     0xc4, 0xfd, 0xa6, 0x4b, 0x87, 0x75, 0xae, 0xdb};
constexpr std::uint8_t absent[16] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                                     0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
constexpr std::uint32_t all_three =
    AV_EXECUTOR_SUB_API_VULKAN | AV_EXECUTOR_SUB_API_OPENGL | AV_EXECUTOR_SUB_API_OPENCL;

void fill(MockHostApis &apis, std::uint32_t how_many)
{ std::uint8_t uuid[16] = {};
  for (std::uint32_t i = 0u; i < how_many; i++) {
      uuid[0] = static_cast<std::uint8_t>(i + 1u);
      apis.add_gpu(uuid, AV_EXECUTOR_SUB_API_VULKAN, AV_BLOB_HOST_POINTER, 0u, 0u);
  } }

}  // namespace

TEST(host_gpu_set, finds_each_gpu_by_the_name_the_device_uses)
{ MockHostApis apis;
  apis.add_gpu(intel, all_three, AV_BLOB_HOST_POINTER | AV_BLOB_DMA_BUF, AV_BLOB_DMA_BUF,
               AV_BLOB_DMA_BUF);
  apis.add_gpu(nvidia, AV_EXECUTOR_SUB_API_VULKAN, AV_BLOB_HOST_POINTER, 0u, 0u);
  HostGpuSet set;
  set.initialize(apis);
  CHECK(!set.overflowed());
  CHECK_EQ(set.count(), 2u);

  const HostGpu *const first = set.find(intel);
  CHECK(first != nullptr);
  CHECK_EQ(first->index(), 0u);
  CHECK_EQ(first->import_kinds(AV_EXECUTOR_SUB_API_OPENCL), std::uint32_t{AV_BLOB_DMA_BUF});

  const HostGpu *const second = set.find(nvidia);
  CHECK(second != nullptr);
  CHECK_EQ(second->index(), 1u);
  CHECK_EQ(second->import_kinds(AV_EXECUTOR_SUB_API_OPENCL), 0u);
  CHECK(set.invariants_hold()); }

/* A domain remembers its GPU by index once the device named it by UUID. */
TEST(host_gpu_set, answers_each_gpu_by_its_index)
{ MockHostApis apis;
  apis.add_gpu(intel, all_three, 0u, 0u, 0u);
  apis.add_gpu(nvidia, AV_EXECUTOR_SUB_API_VULKAN, 0u, 0u, 0u);
  HostGpuSet set;
  set.initialize(apis);
  CHECK(set.at(0u) == set.find(intel));
  CHECK(set.at(1u) == set.find(nvidia));
  CHECK(set.at(2u) == nullptr); }

TEST(host_gpu_set, a_uuid_no_gpu_has_is_not_found)
{ MockHostApis apis;
  apis.add_gpu(intel, all_three, 0u, 0u, 0u);
  HostGpuSet set;
  set.initialize(apis);
  CHECK(!set.overflowed());
  CHECK(set.find(absent) == nullptr);
  CHECK(set.find(nullptr) == nullptr); }

/* A host with no GPU is an answer, not a failure: the library still loads and
 * offers nothing, which is what stops the device advertising a capset it
 * cannot honour. */
TEST(host_gpu_set, a_host_with_no_gpu_is_empty_and_offers_nothing)
{ MockHostApis apis;
  HostGpuSet set;
  set.initialize(apis);
  CHECK(!set.overflowed());
  CHECK_EQ(set.count(), 0u);
  CHECK_EQ(set.sub_apis(), 0u);
  CHECK(set.find(intel) == nullptr);
  CHECK(set.invariants_hold()); }

/* What the ABI answers is every API any GPU can run, because a context names
 * one GPU and one API and the capset carries the union. */
TEST(host_gpu_set, offers_the_union_of_what_its_gpus_run)
{ MockHostApis apis;
  apis.add_gpu(intel, AV_EXECUTOR_SUB_API_VULKAN | AV_EXECUTOR_SUB_API_OPENCL, 0u, 0u, 0u);
  apis.add_gpu(nvidia, AV_EXECUTOR_SUB_API_OPENGL, 0u, 0u, 0u);
  HostGpuSet set;
  set.initialize(apis);
  CHECK(!set.overflowed());
  CHECK_EQ(set.sub_apis(), all_three); }

TEST(host_gpu_set, exactly_as_many_as_it_holds_is_not_an_overflow)
{ MockHostApis apis;
  fill(apis, HostGpuSet::max_gpus);
  HostGpuSet set;
  set.initialize(apis);
  CHECK(!set.overflowed());
  CHECK_EQ(set.count(), HostGpuSet::max_gpus);
  CHECK(set.invariants_hold()); }

/* A host with more GPUs than the set holds keeps the ones it can and says the
 * rest did not fit, rather than pretending they are not there. */
TEST(host_gpu_set, more_gpus_than_it_holds_keeps_what_it_can_and_says_so)
{ MockHostApis apis;
  fill(apis, MockHostApis::max_gpus);
  HostGpuSet set;
  set.initialize(apis);
  CHECK(set.overflowed());
  CHECK_EQ(set.count(), HostGpuSet::max_gpus);
  CHECK(set.invariants_hold()); }

/* A count past the array is one `find` would walk off the end of. Every slot
 * holds a GPU that is itself sound, so nothing but the count is wrong. */
TEST(host_gpu_set_invariants, a_count_past_the_array_breaks_them)
{ MockHostApis apis;
  fill(apis, HostGpuSet::max_gpus);
  HostGpuSet set;
  set.initialize(apis);
  CHECK(!set.overflowed());
  HostGpuSetProbe(set).set_count(HostGpuSet::max_gpus + 1u);
  CHECK(!set.invariants_hold()); }

/* The set answers the capset from its GPUs, so one broken GPU is a broken set. */
TEST(host_gpu_set_invariants, a_gpu_that_breaks_its_own_breaks_them)
{ MockHostApis apis;
  apis.add_gpu(intel, all_three, AV_BLOB_HOST_POINTER, 0u, AV_BLOB_DMA_BUF);
  apis.add_gpu(nvidia, AV_EXECUTOR_SUB_API_VULKAN, AV_BLOB_HOST_POINTER, 0u, 0u);
  HostGpuSet set;
  set.initialize(apis);
  CHECK(!set.overflowed());
  HostGpuProbe(HostGpuSetProbe(set).gpu_at(0u)).set_sub_apis(AV_EXECUTOR_SUB_API_VULKAN);
  CHECK(!set.invariants_hold()); }
