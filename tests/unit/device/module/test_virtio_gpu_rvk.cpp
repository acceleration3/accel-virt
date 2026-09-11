#include "core/support/status.hpp"
#include "device/context/device_context.hpp"
#include "device/context/device_context_table.hpp"
#include "device/module/virtio_gpu_rvk.hpp"
#include "device/support/text_buffer.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/host/host_violation.hpp"
#include "harness/host/mock_host.hpp"

#include <cstdint>

using av::HostViolation;
using av::MockHost;
using av::Status;
using av::TextBuffer;
using av::VirtioGpuRvk;

namespace av {

struct DeviceContextTableProbe {
    /* A running context placed in a free slot behind the table's back. */
    static void place(DeviceContextTable &table, std::uint32_t id)
    {
        for (ObjectStorage<DeviceContext> &slot : table.slots_)
            if (!slot.constructed()) {
                static_cast<void>(slot.construct(table.host_, id));
                static_cast<void>(slot.object().start());
                return;
            }
    }
};

struct VirtioGpuRvkProbe {
    /* A context the table holds on a device never realized. */
    static void orphan_context(VirtioGpuRvk &device, std::uint32_t id)
    {
        static_cast<void>(device.contexts_.create(id));
    }
    /* Two live contexts claiming one id. */
    static void duplicate_context(VirtioGpuRvk &device, std::uint32_t id)
    {
        static_cast<void>(device.contexts_.create(id));
        DeviceContextTableProbe::place(device.contexts_, id);
    }
};

}  // namespace av

namespace {

constexpr std::uint8_t first[16] = {1};
constexpr std::uint8_t second[16] = {2};

}  // namespace

TEST(virtio_gpu_rvk_realize, binds_once_and_contexts_follow)
{ MockHost host;
  host.add_gpu(first);
  host.add_gpu(second);
  VirtioGpuRvk device(host);
  char bytes[96];
  TextBuffer message(bytes, sizeof bytes);
  CHECK(!device.realized());
  CHECK_STATUS(device.create_context(1u), Status::invalid_state);
  CHECK_STATUS(device.realize("02000000-0000-0000-0000-000000000000", message), Status::ok);
  CHECK(device.realized());
  CHECK(device.invariants_hold());
  CHECK_EQ(device.gpu_index(), 1u);
  CHECK_STATUS(device.realize("auto", message), Status::invalid_state);
  CHECK_STATUS(device.create_context(1u), Status::ok);
  CHECK_STATUS(device.create_context(2u), Status::ok);
  CHECK(device.invariants_hold());
  CHECK_EQ(device.contexts(), 2u);
  CHECK_STATUS(device.destroy_context(1u), Status::ok);
  CHECK_STATUS(device.destroy_context(1u), Status::not_found);
  device.reset();
  CHECK(device.invariants_hold());
  CHECK_EQ(device.contexts(), 0u);
  CHECK_EQ(host.live_threads(), 0u); }

TEST(virtio_gpu_rvk_realize, a_binding_failure_leaves_it_unrealized)
{ MockHost host;
  VirtioGpuRvk device(host);
  char bytes[96];
  TextBuffer message(bytes, sizeof bytes);
  CHECK_STATUS(device.realize("auto", message), Status::not_found);
  CHECK(!device.realized());
  CHECK(device.invariants_hold()); }

TEST(virtio_gpu_rvk, lifecycle_is_main_loop_only)
{ MockHost host;
  host.add_gpu(first);
  VirtioGpuRvk device(host);
  void *thread = nullptr;
  CHECK_STATUS(host.start_thread(
                   [](void *context) {
                       auto *const d = static_cast<VirtioGpuRvk *>(context);
                       char bytes[8];
                       TextBuffer message(bytes, sizeof bytes);
                       static_cast<void>(d->realize("x", message));
                       d->reset();
                       static_cast<void>(d->create_context(1u));
                       static_cast<void>(d->destroy_context(1u));
                   },
                   &device, thread),
               Status::ok);
  host.join_thread(thread);
  /* realize and its binding, reset and its table, create, destroy and its table */
  for (int violation = 0; violation < 7; violation++) CHECK(host.claim(HostViolation::main_loop_required));
  CHECK_EQ(host.unclaimed_violations(), 0u);
  CHECK_EQ(host.logged(), 1u); }

TEST(virtio_gpu_rvk_invariants, a_context_on_an_unrealized_device_or_a_broken_table_is_detected)
{ MockHost host;
  VirtioGpuRvk unrealized(host);
  av::VirtioGpuRvkProbe::orphan_context(unrealized, 1u);
  CHECK(!unrealized.invariants_hold());
  unrealized.reset();
  host.add_gpu(first);
  VirtioGpuRvk broken(host);
  char bytes[64];
  TextBuffer message(bytes, sizeof bytes);
  CHECK_STATUS(broken.realize("auto", message), Status::ok);
  av::VirtioGpuRvkProbe::duplicate_context(broken, 4u);
  CHECK(!broken.invariants_hold());
  broken.reset(); }
