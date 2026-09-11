#include "core/support/status.hpp"
#include "device/capset/device_capset.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/host/host_violation.hpp"
#include "harness/host/mock_host.hpp"
#include "proto/context/capset_info.hpp"
#include "proto/context/gpu_identity.hpp"
#include "proto/context/sub_api.hpp"

#include <cstdint>
#include <cstring>

using av::CapsetInfo;
using av::DeviceCapset;
using av::GpuIdentity;
using av::HostViolation;
using av::MockHost;
using av::Status;
using av::SubApi;

namespace {

constexpr std::uint8_t first[GpuIdentity::uuid_bytes] = {1};
constexpr std::uint8_t second[GpuIdentity::uuid_bytes] = {2};

}  // namespace

TEST(device_capset_describe, offers_vulkan_every_ring_order_and_the_bound_gpu)
{ MockHost host;
  host.add_gpu(first);
  host.add_gpu(GpuIdentity(0x1002u, 0x744Cu, GpuIdentity::integrated_gpu, second, 1u << 30, 2u << 30, "radeon"));
  DeviceCapset capset(host);
  CHECK(!capset.described());
  capset.describe(1u, static_cast<std::uint32_t>(SubApi::vulkan));
  CHECK(capset.described());
  const CapsetInfo &info = capset.info();
  CHECK_EQ(info.version().major_version(), 1u);
  CHECK_EQ(info.version().minor_version(), 0u);
  CHECK(info.offers(SubApi::vulkan));
  CHECK(!info.offers(SubApi::opengl));
  CHECK(!info.offers(SubApi::opencl));
  CHECK_EQ(info.max_contexts(), 64u);
  CHECK(info.accepts_ring_order(12u) && info.accepts_ring_order(24u));
  CHECK(!info.accepts_ring_order(11u) && !info.accepts_ring_order(25u));
  CHECK(!info.has_feature(CapsetInfo::feature_host_visible_memory));
  CHECK_EQ(info.gpu().vendor_id(), 0x1002u);
  CHECK_EQ(info.gpu().device_id(), 0x744Cu);
  CHECK_EQ(info.gpu().device_type(), GpuIdentity::integrated_gpu);
  CHECK(std::memcmp(info.gpu().uuid(), second, sizeof second) == 0);
  CHECK_EQ(info.gpu().shared_memory(), 2u << 30);
  CHECK(std::strcmp(info.gpu().name(), "radeon") == 0);
  std::uint8_t bytes[CapsetInfo::encoded_size] = {};
  CHECK_STATUS(info.encode(bytes, sizeof bytes), Status::ok);
  CapsetInfo decoded;
  CHECK_STATUS(decoded.decode(bytes, sizeof bytes), Status::ok); }

TEST(device_capset_describe, a_hostmem_region_offers_host_visible_memory)
{ MockHost host;
  host.add_gpu(first);
  host.set_hostmem_size(0x10000u);
  DeviceCapset capset(host);
  capset.describe(0u, static_cast<std::uint32_t>(SubApi::vulkan));
  CHECK(capset.info().has_feature(CapsetInfo::feature_host_visible_memory)); }

TEST(device_capset_describe, offers_exactly_the_sub_apis_the_executor_offers)
{ MockHost host;
  host.add_gpu(first);
  DeviceCapset both(host);
  both.describe(0u, static_cast<std::uint32_t>(SubApi::opengl) | static_cast<std::uint32_t>(SubApi::opencl));
  CHECK(!both.info().offers(SubApi::vulkan));
  CHECK(both.info().offers(SubApi::opengl) && both.info().offers(SubApi::opencl));
  DeviceCapset none(host);
  none.describe(0u, 0u);
  CHECK(!none.described()); }

TEST(device_capset_describe, a_device_type_vulkan_does_not_define_is_reported_as_other)
{ MockHost host;
  host.add_gpu(GpuIdentity(1u, 2u, 7u, first, 0u, 0u, "odd"));
  DeviceCapset capset(host);
  capset.describe(0u, static_cast<std::uint32_t>(SubApi::vulkan));
  CHECK_EQ(capset.info().gpu().device_type(), GpuIdentity::other_gpu);
  std::uint8_t bytes[CapsetInfo::encoded_size] = {};
  CHECK_STATUS(capset.info().encode(bytes, sizeof bytes), Status::ok);
  CapsetInfo decoded;
  CHECK_STATUS(decoded.decode(bytes, sizeof bytes), Status::ok); }

TEST(device_capset, describe_is_main_loop_only)
{ MockHost host;
  host.add_gpu(first);
  DeviceCapset capset(host);
  void *thread = nullptr;
  CHECK_STATUS(host.start_thread([](void *context) { static_cast<DeviceCapset *>(context)->describe(0u, 1u); }, &capset,
                                 thread),
               Status::ok);
  host.join_thread(thread);
  CHECK(host.claim(HostViolation::main_loop_required)); }
