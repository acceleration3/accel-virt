#include "core/support/status.hpp"
#include "device/gpu/device_capset.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/host/host_violation.hpp"
#include "device/feasibility/host_feasibility.hpp"
#include "harness/host/mock_host.hpp"
#include "proto/context/capset_info.hpp"
#include "proto/context/gpu_identity.hpp"
#include "proto/context/sub_api.hpp"
#include "proto/context/protocol_version.hpp"

#include <cstdint>
#include <cstring>

using av::CapsetInfo;
using av::DeviceCapset;
using av::GpuIdentity;
using av::HostViolation;
using av::HostFeasibility;
using av::MockHost;
using av::Status;
using av::SubApi;

namespace {

constexpr std::uint8_t first[GpuIdentity::uuid_bytes] = {1};
constexpr std::uint8_t second[GpuIdentity::uuid_bytes] = {2};
std::uint64_t decided(MockHost &host)
{
    return HostFeasibility(host).features();
}

}

TEST(device_capset_describe, offers_vulkan_every_ring_order_and_the_bound_gpu)
{ MockHost host;
  host.add_gpu(first);
  host.add_gpu(GpuIdentity(0x1002u, 0x744Cu, GpuIdentity::integrated_gpu, second, 1u << 30, 2u << 30, "radeon"));
  DeviceCapset capset(host);
  CHECK(!capset.described());
  capset.describe(1u, static_cast<std::uint32_t>(SubApi::vulkan), decided(host));
  CHECK(capset.described());
  const CapsetInfo &info = capset.info();
  CHECK_EQ(info.version().major_version(), av::ProtocolVersion::current_major);
  CHECK_EQ(info.version().minor_version(), av::ProtocolVersion::current_minor);
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
  CHECK_EQ(info.gpu().shared_memory(), 0u);
  CHECK(std::strcmp(info.gpu().name(), "radeon") == 0);
  std::uint8_t bytes[CapsetInfo::encoded_size] = {};
  CHECK_STATUS(info.encode(bytes, sizeof bytes), Status::ok);
  CapsetInfo decoded;
  CHECK_STATUS(decoded.decode(bytes, sizeof bytes), Status::ok); }

TEST(device_capset_describe, offers_host_visible_memory_exactly_when_there_is_a_window)
{ MockHost host;
  host.add_gpu(first);
  host.set_hostmem_size(0x100000u);
  DeviceCapset capset(host);
  capset.describe(0u, static_cast<std::uint32_t>(SubApi::vulkan), decided(host));
  CHECK(capset.info().has_feature(CapsetInfo::feature_host_visible_memory));
  CHECK(!capset.info().has_feature(2u));
  CHECK(!capset.info().has_feature(4u));

  MockHost windowless;
  windowless.add_gpu(first);
  DeviceCapset none(windowless);
  none.describe(0u, static_cast<std::uint32_t>(SubApi::vulkan), decided(windowless));
  CHECK(!none.info().has_feature(CapsetInfo::feature_host_visible_memory)); }

TEST(device_capset_describe, offers_exactly_the_sub_apis_the_executor_offers)
{ MockHost host;
  host.add_gpu(first);
  DeviceCapset both(host);
  both.describe(0u, static_cast<std::uint32_t>(SubApi::opengl) | static_cast<std::uint32_t>(SubApi::opencl), 0u);
  CHECK(!both.info().offers(SubApi::vulkan));
  CHECK(both.info().offers(SubApi::opengl) && both.info().offers(SubApi::opencl));
  DeviceCapset none(host);
  none.describe(0u, 0u, 0u);
  CHECK(!none.described()); }

TEST(device_capset_describe, a_device_type_vulkan_does_not_define_is_reported_as_other)
{ MockHost host;
  host.add_gpu(GpuIdentity(1u, 2u, 7u, first, 0u, 0u, "odd"));
  DeviceCapset capset(host);
  capset.describe(0u, static_cast<std::uint32_t>(SubApi::vulkan), 0u);
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
  CHECK_STATUS(host.start_thread([](void *context) { static_cast<DeviceCapset *>(context)->describe(0u, 1u, 0u); }, &capset,
                                 thread),
               Status::ok);
  host.join_thread(thread);
  CHECK(host.claim(HostViolation::main_loop_required));
  CHECK(host.claim(HostViolation::main_loop_required)); }

TEST(device_capset_resolve_display, the_bit_follows_the_display_and_is_decided_once)
{ MockHost host;
  host.add_gpu(first);
  host.add_display_heads(1u);
  DeviceCapset capset(host);
  capset.describe(0u, 1u, CapsetInfo::feature_host_visible_memory);
  CHECK(!capset.display_resolved());
  CHECK(!capset.info().has_feature(CapsetInfo::feature_dmabuf_scanout));

  capset.resolve_display(true);
  CHECK(capset.display_resolved());
  CHECK(capset.info().has_feature(CapsetInfo::feature_dmabuf_scanout));
  CHECK(capset.info().has_feature(CapsetInfo::feature_host_visible_memory));
  capset.resolve_display(false);
  CHECK(capset.info().has_feature(CapsetInfo::feature_dmabuf_scanout)); }

TEST(device_capset_resolve_display, a_host_with_no_gl_display_carries_no_scanout_bit)
{ MockHost host;
  host.add_gpu(first);
  DeviceCapset capset(host);
  capset.describe(0u, 1u, 0u);
  capset.resolve_display(false);
  CHECK(capset.display_resolved());
  CHECK(!capset.info().has_feature(CapsetInfo::feature_dmabuf_scanout));
  capset.resolve_display(true);
  CHECK(!capset.info().has_feature(CapsetInfo::feature_dmabuf_scanout)); }

TEST(device_capset_resolve_display, a_capset_nobody_described_announces_nothing)
{ MockHost host;
  DeviceCapset capset(host);
  capset.resolve_display(true);
  CHECK(capset.display_resolved());
  CHECK(!capset.described());
  CHECK(!capset.info().has_feature(CapsetInfo::feature_dmabuf_scanout)); }

TEST(device_capset_describe, names_the_gpu_without_the_host_driver_suffix)
{ MockHost host;
  host.add_gpu(GpuIdentity(0x1002u, 0x7550u, GpuIdentity::discrete_gpu, first, 16ull << 30, 0u,
                           "AMD Radeon RX 9070 XT (RADV GFX1201)"));
  DeviceCapset capset(host);
  capset.describe(0u, static_cast<std::uint32_t>(SubApi::vulkan), decided(host));
  CHECK(std::strcmp(capset.info().gpu().name(), "AMD Radeon RX 9070 XT") == 0);
  CHECK_EQ(capset.info().gpu().local_memory(), 14ull << 30); }

TEST(device_capset_describe, tells_the_guest_the_capped_local_memory_so_every_budget_it_derives_is_the_cap)
{ MockHost host;
  host.add_gpu(GpuIdentity(0x1002u, 0x7550u, GpuIdentity::discrete_gpu, first, 16ull << 30, 0u, "AMD Radeon"));
  DeviceCapset capset(host);
  capset.reserve_vram(4096u);
  capset.describe(0u, static_cast<std::uint32_t>(SubApi::vulkan), decided(host));
  CHECK_EQ(capset.info().gpu().local_memory(), 12ull << 30);
  CHECK_EQ(capset.info().vram_reserve_mib(), 4096u); }

namespace {

void meminfo(MockHost &host, const char *text) { host.add_file("/proc/meminfo", text, std::strlen(text)); }

}

TEST(device_capset_host_ram, the_guest_is_told_the_hosts_memory_less_its_own_ram_and_the_desktop_s_share)
{ MockHost host;
  host.add_gpu(GpuIdentity(0x1002u, 0x7550u, GpuIdentity::discrete_gpu, first, 16ull << 30, 0u, "AMD Radeon"));
  meminfo(host, "MemTotal:       31457280 kB\nMemFree: 1 kB\n");
  host.set_guest_memory_bytes(9ull << 30);
  DeviceCapset capset(host);
  capset.describe(0u, static_cast<std::uint32_t>(SubApi::vulkan), decided(host));
  CHECK_EQ(capset.info().gpu().shared_memory(), (13ull << 30) + (1ull << 29)); }

TEST(device_capset_host_ram, an_explicit_reserve_replaces_the_desktop_s_auto_share)
{ MockHost host;
  host.add_gpu(first);
  meminfo(host, "MemTotal:       31457280 kB\n");
  host.set_guest_memory_bytes(9ull << 30);
  DeviceCapset capset(host);
  capset.reserve_host_ram(2048u);
  capset.describe(0u, static_cast<std::uint32_t>(SubApi::vulkan), decided(host));
  CHECK_EQ(capset.info().gpu().shared_memory(), 19ull << 30); }

TEST(device_capset_host_ram, a_host_whose_meminfo_cannot_be_read_or_parsed_declares_no_budget)
{ MockHost without;
  without.add_gpu(first);
  without.set_guest_memory_bytes(9ull << 30);
  DeviceCapset none(without);
  none.describe(0u, static_cast<std::uint32_t>(SubApi::vulkan), decided(without));
  CHECK_EQ(none.info().gpu().shared_memory(), 0u);
  MockHost garbled;
  garbled.add_gpu(first);
  meminfo(garbled, "nothing useful\n");
  DeviceCapset broken(garbled);
  broken.describe(0u, static_cast<std::uint32_t>(SubApi::vulkan), decided(garbled));
  CHECK_EQ(broken.info().gpu().shared_memory(), 0u); }

TEST(device_capset_host_ram, a_guest_as_large_as_the_host_still_leaves_the_least_budget)
{ MockHost host;
  host.add_gpu(first);
  meminfo(host, "MemTotal:       8388608 kB\n");
  host.set_guest_memory_bytes(7ull << 30);
  DeviceCapset capset(host);
  capset.describe(0u, static_cast<std::uint32_t>(SubApi::vulkan), decided(host));
  CHECK_EQ(capset.info().gpu().shared_memory(), 1ull << 30); }

TEST(device_capset_host_ram, the_executor_is_told_the_floor_host_memory_may_not_fall_below)
{ MockHost host;
  host.add_gpu(first);
  meminfo(host, "MemTotal:       31457280 kB\n");
  host.set_guest_memory_bytes(16ull << 30);
  DeviceCapset capset(host);
  capset.describe(0u, static_cast<std::uint32_t>(SubApi::vulkan), decided(host));
  CHECK_EQ(capset.info().host_ram_floor(), 7ull << 30);
  MockHost asked;
  asked.add_gpu(first);
  meminfo(asked, "MemTotal:       31457280 kB\n");
  asked.set_guest_memory_bytes(16ull << 30);
  DeviceCapset explicit_floor(asked);
  explicit_floor.reserve_host_ram(2048u);
  explicit_floor.describe(0u, static_cast<std::uint32_t>(SubApi::vulkan), decided(asked));
  CHECK_EQ(explicit_floor.info().host_ram_floor(), 2ull << 30); }
