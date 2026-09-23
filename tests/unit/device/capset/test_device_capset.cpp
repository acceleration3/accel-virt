#include "core/support/status.hpp"
#include "device/capset/device_capset.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/host/host_violation.hpp"
#include "device/probe/host_feasibility.hpp"
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
using av::HostFeasibility;
using av::MockHost;
using av::Status;
using av::SubApi;

namespace {

constexpr std::uint8_t first[GpuIdentity::uuid_bytes] = {1};
constexpr std::uint8_t second[GpuIdentity::uuid_bytes] = {2};
constexpr std::uint64_t probe_window = 0x100000u;

/* What a device offers is what its probes found, so every test here says which
 * host it ran on. */
std::uint64_t decided(MockHost &host)
{
    HostFeasibility feasibility(host);
    feasibility.decide(probe_window);
    return feasibility.features();
}

}  // namespace

TEST(device_capset_describe, offers_vulkan_every_ring_order_and_the_bound_gpu)
{ MockHost host;
  host.add_gpu(first);
  host.add_gpu(GpuIdentity(0x1002u, 0x744Cu, GpuIdentity::integrated_gpu, second, 1u << 30, 2u << 30, "radeon"));
  DeviceCapset capset(host);
  CHECK(!capset.described());
  capset.describe(1u, static_cast<std::uint32_t>(SubApi::vulkan), decided(host));
  CHECK(capset.described());
  const CapsetInfo &info = capset.info();
  CHECK_EQ(info.version().major_version(), 1u);
  CHECK_EQ(info.version().minor_version(), 1u);
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

/* Constitution VI: every bit the guest reads is one a probe passed for. */
TEST(device_capset_describe, offers_exactly_the_memory_paths_the_probes_proved)
{ MockHost host;
  host.add_gpu(first);
  host.set_hostmem_size(0x100000u);
  host.add_guest_ram(probe_window, 0x100000u);
  DeviceCapset capset(host);
  capset.describe(0u, static_cast<std::uint32_t>(SubApi::vulkan), decided(host));
  CHECK(capset.info().has_feature(CapsetInfo::feature_host_visible_memory));
  CHECK(capset.info().has_feature(CapsetInfo::feature_gpu_exported_memory));
  CHECK(capset.info().has_feature(CapsetInfo::feature_guest_memory_import));

  MockHost refusing;
  refusing.add_gpu(first);
  refusing.set_hostmem_size(0x100000u);
  refusing.add_guest_ram(probe_window, 0x100000u);
  refusing.refuse_export_at(1u, -2);
  refusing.refuse_import_at(1u, 0);
  DeviceCapset fallback(refusing);
  fallback.describe(0u, static_cast<std::uint32_t>(SubApi::vulkan), decided(refusing));
  CHECK(fallback.info().has_feature(CapsetInfo::feature_host_visible_memory));
  CHECK(!fallback.info().has_feature(CapsetInfo::feature_gpu_exported_memory));
  CHECK(!fallback.info().has_feature(CapsetInfo::feature_guest_memory_import)); }

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
  CHECK(host.claim(HostViolation::main_loop_required)); }

/* A guest blob reaching the host GPU without a copy needs two things: this
 * device able to build a handle, which its own probe proves, and the executor
 * able to take one, which only the executor knows. A bit set from the probe
 * alone promises what the library may not keep (feature 042). */
TEST(device_capset_withdraw, takes_back_the_import_bit_the_probe_alone_claimed)
{ MockHost host;
  host.add_gpu(first);
  DeviceCapset capset(host);
  capset.describe(0u, 1u,
                  CapsetInfo::feature_host_visible_memory | CapsetInfo::feature_guest_memory_import);
  CHECK(capset.described());
  CHECK(capset.info().has_feature(CapsetInfo::feature_guest_memory_import));

  capset.withdraw_guest_import();
  CHECK(!capset.info().has_feature(CapsetInfo::feature_guest_memory_import));
  /* Only that one: what the host visibly has is not withdrawn with it. */
  CHECK(capset.info().has_feature(CapsetInfo::feature_host_visible_memory)); }

TEST(device_capset_withdraw, withdrawing_from_a_capset_nobody_described_is_nothing)
{ MockHost host;
  DeviceCapset capset(host);
  capset.withdraw_guest_import();
  CHECK(!capset.described()); }

/* The display's answer is not the device's to know at realize: QEMU registers
 * its display backends in qemu_init_displays(), after qmp_x_exit_preconfig()
 * has realized every device, and it is the backend that gives a console its GL
 * context. So the bit is decided when the guest first reads the capset, once,
 * and the answer it was given is kept. */
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

/* The guest names its adapter from this field, so it carries the product and
 * not the host driver Mesa appends to it. */
TEST(device_capset_describe, names_the_gpu_without_the_host_driver_suffix)
{ MockHost host;
  host.add_gpu(GpuIdentity(0x1002u, 0x7550u, GpuIdentity::discrete_gpu, first, 16ull << 30, 0u,
                           "AMD Radeon RX 9070 XT (RADV GFX1201)"));
  DeviceCapset capset(host);
  capset.describe(0u, static_cast<std::uint32_t>(SubApi::vulkan), decided(host));
  CHECK(std::strcmp(capset.info().gpu().name(), "AMD Radeon RX 9070 XT") == 0);
  CHECK_EQ(capset.info().gpu().local_memory(), 16ull << 30); }
