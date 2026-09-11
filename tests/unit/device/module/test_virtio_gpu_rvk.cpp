#include "core/support/status.hpp"
#include "device/context/device_context.hpp"
#include "device/context/device_context_table.hpp"
#include "device/memory/blob_resource.hpp"
#include "device/memory/blob_table.hpp"
#include "device/module/driver_status.hpp"
#include "device/module/virtio_gpu_rvk.hpp"
#include "core/support/text_buffer.hpp"
#include "harness/executor/mock_executor_ops.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/host/gpu_request_builder.hpp"
#include "harness/host/host_violation.hpp"
#include "harness/host/mock_host.hpp"
#include "harness/host/mock_request.hpp"
#include "harness/ring/ring_blob_writer.hpp"
#include "proto/context/context_setup.hpp"
#include "proto/context/protocol_version.hpp"
#include "proto/context/sub_api.hpp"

#include <cstdint>
#include <cstring>

using av::DriverStatus;
using av::GpuRequestBuilder;
using av::HostViolation;
using av::MockHost;
using av::MockRequest;
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
                static_cast<void>(slot.construct(table.host_, table.blobs_, table.interrupts_, table.library_, id));
                static_cast<void>(slot.object().start());
                return;
            }
    }
};

struct BlobTableProbe {
    /* A host blob in a free slot, behind the table's back. */
    static void place(BlobTable &table, std::uint32_t id)
    {
        for (ObjectStorage<BlobResource> &slot : table.slots_)
            if (slot.construct(table.host_, id) == Status::ok) return;
    }
};

struct FenceInterruptsProbe {
    static void connect_without_lock(FenceInterrupts &interrupts) { interrupts.connected_ = true; }
    static void repair(FenceInterrupts &interrupts) { interrupts.connected_ = false; }
};

struct VirtioGpuRvkProbe {
    /* Interrupts connected with no lock to hold them apart. */
    static void break_interrupts(VirtioGpuRvk &device) { FenceInterruptsProbe::connect_without_lock(device.interrupts_); }
    static void repair_interrupts(VirtioGpuRvk &device) { FenceInterruptsProbe::repair(device.interrupts_); }
    /* Interrupts properly connected, but behind the status byte's back: the
     * state 035's abort left the device in, where a connection outlives the
     * driver that asked for it. */
    static Status connect_behind_the_status(VirtioGpuRvk &device) { return device.interrupts_.connect(); }
    static void   disconnect_behind_the_status(VirtioGpuRvk &device) { device.interrupts_.disconnect(); }
    /* The ring header a guest writes before its setup. */
    static void write_ring_header(VirtioGpuRvk &device, std::uint32_t id)
    {
        RingBlobWriter(device.blobs_).write_header(id, 12u);
    }
    /* Two blobs claiming one id. */
    static void duplicate_blob(VirtioGpuRvk &device, std::uint32_t id)
    {
        BlobTableProbe::place(device.blobs_, id);
        BlobTableProbe::place(device.blobs_, id);
    }
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
  CHECK_STATUS(device.realize("02000000-0000-0000-0000-000000000000", false, nullptr, message), Status::ok);
  CHECK(device.realized());
  CHECK(device.invariants_hold());
  CHECK_EQ(device.gpu_index(), 1u);
  CHECK_STATUS(device.realize("auto", false, nullptr, message), Status::invalid_state);
  CHECK_STATUS(device.create_context(1u), Status::invalid_state);   /* no interrupt carries its fences yet */
  device.status(DriverStatus::driver_ok);
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
  CHECK_STATUS(device.realize("auto", false, nullptr, message), Status::not_found);
  CHECK(!device.realized());
  CHECK(device.invariants_hold()); }

TEST(virtio_gpu_rvk_realize, blob_on_is_refused_because_the_device_answers_blob_requests_itself)
{ MockHost host;
  host.add_gpu(first);
  VirtioGpuRvk device(host);
  char bytes[128];
  TextBuffer message(bytes, sizeof bytes);
  CHECK_STATUS(device.realize("auto", true, nullptr, message), Status::invalid_parameter);
  CHECK(std::strcmp(message.text(), "virtio-gpu-rvk: blob=on is not supported; the device answers blob requests itself") == 0);
  CHECK(!device.realized());
  CHECK_EQ(host.logged(), 1u); }

TEST(virtio_gpu_rvk_control, answers_blob_requests_and_reset_releases_every_blob)
{ MockHost host;
  host.add_gpu(first);
  host.set_hostmem_size(0x10000u);
  VirtioGpuRvk device(host);
  char bytes[96];
  TextBuffer message(bytes, sizeof bytes);
  CHECK_STATUS(device.realize("auto", false, nullptr, message), Status::ok);
  GpuRequestBuilder create(0x010Cu);
  create.u32(1u).u32(2u).u32(1u).u32(0u).u64(0u).u64(0x1000u);
  MockRequest request(create.bytes(), create.size());
  CHECK(device.control(&request, 0x010Cu));
  CHECK_EQ(request.response_type(), 0x1100u);
  CHECK_EQ(device.blobs(), 1u);
  CHECK(device.invariants_hold());
  GpuRequestBuilder display_info(0x0100u);
  MockRequest base(display_info.bytes(), display_info.size());
  CHECK(!device.control(&base, 0x0100u));
  CHECK(!base.answered());
  device.reset();
  CHECK_EQ(device.blobs(), 0u);
  CHECK_EQ(host.live_host_memory(), 0u);
  CHECK(device.invariants_hold()); }

TEST(virtio_gpu_rvk_control, an_unrealized_device_answers_nothing_and_offers_no_capset)
{ MockHost host;
  VirtioGpuRvk device(host);
  CHECK_EQ(device.capsets(), 0u);
  GpuRequestBuilder info(0x0108u);
  info.u32(0u).u32(0u);
  MockRequest request(info.bytes(), info.size());
  CHECK(!device.control(&request, 0x0108u));
  CHECK(!request.answered()); }

TEST(virtio_gpu_rvk_realize, without_an_executor_there_is_no_capset_and_no_context)
{ MockHost host;
  host.add_gpu(first);
  VirtioGpuRvk device(host);
  char bytes[96];
  TextBuffer message(bytes, sizeof bytes);
  CHECK_STATUS(device.realize("auto", false, nullptr, message), Status::ok);
  device.status(DriverStatus::driver_ok);
  CHECK(device.realized());
  CHECK_EQ(device.capsets(), 0u);
  GpuRequestBuilder info(0x0108u);
  info.u32(0u).u32(0u);
  MockRequest info_request(info.bytes(), info.size());
  CHECK(device.control(&info_request, 0x0108u));
  CHECK_EQ(info_request.response_type(), 0x1205u);
  GpuRequestBuilder create(0x0200u);
  create.context(7u).u32(0u).u32(0x72u).zeros(64u);
  MockRequest create_request(create.bytes(), create.size());
  CHECK(device.control(&create_request, 0x0200u));
  CHECK_EQ(create_request.response_type(), 0x1205u);
  CHECK_EQ(device.contexts(), 0u); }

TEST(virtio_gpu_rvk_realize, an_executor_table_this_abi_does_not_accept_is_refused)
{ MockHost host;
  host.add_gpu(first);
  av::MockExecutorOps executor(RVK_EXECUTOR_SUB_API_VULKAN);
  executor.editable().abi_version = 2u;
  VirtioGpuRvk device(host);
  char bytes[128];
  TextBuffer message(bytes, sizeof bytes);
  CHECK_STATUS(device.realize("auto", false, &executor.table(), message), Status::invalid_parameter);
  CHECK(std::strstr(message.text(), "not ABI version 1") != nullptr);
  CHECK(!device.realized());
  CHECK_EQ(host.logged(), 1u); }

TEST(virtio_gpu_rvk_control, answers_context_requests_and_reset_destroys_contexts_before_their_blobs)
{ MockHost host;
  host.add_gpu(first);
  host.set_hostmem_size(0x10000u);
  VirtioGpuRvk device(host);
  av::MockExecutorOps executor(RVK_EXECUTOR_SUB_API_VULKAN);
  char bytes[96];
  TextBuffer message(bytes, sizeof bytes);
  CHECK_STATUS(device.realize("auto", false, &executor.table(), message), Status::ok);
  device.status(DriverStatus::driver_ok);
  CHECK_EQ(device.capsets(), 1u);
  GpuRequestBuilder info(0x0108u);
  info.u32(0u).u32(0u);
  MockRequest info_request(info.bytes(), info.size());
  CHECK(device.control(&info_request, 0x0108u));
  CHECK_EQ(info_request.response_type(), 0x1102u);
  for (std::uint32_t id = 1u; id <= 2u; id++) {
      GpuRequestBuilder blob(0x010Cu);
      blob.u32(id).u32(2u).u32(1u).u32(0u).u64(0u).u64(0x2000u);
      MockRequest blob_request(blob.bytes(), blob.size());
      CHECK(device.control(&blob_request, 0x010Cu));
  }
  av::VirtioGpuRvkProbe::write_ring_header(device, 1u);
  GpuRequestBuilder create(0x0200u);
  create.context(7u).u32(0u).u32(0x72u).zeros(64u);
  MockRequest create_request(create.bytes(), create.size());
  CHECK(device.control(&create_request, 0x0200u));
  CHECK_EQ(create_request.response_type(), 0x1100u);
  std::uint8_t setup_bytes[av::ContextSetup::encoded_size] = {};
  CHECK_STATUS(av::ContextSetup(av::ProtocolVersion(1u, 0u), av::SubApi::vulkan, 1u, 12u, 2u, 0u, 0u)
                   .encode(setup_bytes, sizeof setup_bytes),
               Status::ok);
  GpuRequestBuilder submit(0x0207u);
  submit.context(7u).u32(32u).u32(0u).append(setup_bytes, sizeof setup_bytes);
  MockRequest submit_request(submit.bytes(), submit.size());
  CHECK(device.control(&submit_request, 0x0207u));
  CHECK_EQ(submit_request.response_type(), 0x1100u);
  CHECK(device.invariants_hold());
  device.reset();
  CHECK_EQ(device.contexts(), 0u);
  CHECK_EQ(device.blobs(), 0u);
  CHECK(device.invariants_hold()); }

TEST(virtio_gpu_rvk_status, the_fence_interrupts_follow_the_drivers_status_byte)
{ MockHost host;
  host.add_gpu(first);
  VirtioGpuRvk device(host);
  device.status(DriverStatus::driver_ok);   /* unrealized: nothing */
  CHECK(!host.fence_interrupts_connected());
  char bytes[96];
  TextBuffer message(bytes, sizeof bytes);
  CHECK_STATUS(device.realize("auto", false, nullptr, message), Status::ok);
  device.status(0x0Bu);                     /* acknowledge, driver, features ok */
  CHECK(!host.fence_interrupts_connected());
  device.status(0x0Fu);
  CHECK(host.fence_interrupts_connected());
  CHECK(device.invariants_hold());
  device.status(0x0Fu);
  /* A driver that stops writes this and never resets: 035's abort. */
  device.status(0x0Bu);
  CHECK(!host.fence_interrupts_connected());
  CHECK(device.invariants_hold());
  device.status(0x0Fu);
  device.status(0u);
  CHECK(!host.fence_interrupts_connected());
  device.reset();                           /* the reset QEMU delivers after it */
  CHECK(!host.fence_interrupts_connected());
  host.fail_fence_connect_at(1u);
  device.status(DriverStatus::driver_ok);
  CHECK(!host.fence_interrupts_connected());
  CHECK_EQ(host.logged(), 1u);
  CHECK_STATUS(device.create_context(1u), Status::invalid_state);
  device.status(DriverStatus::driver_ok);
  CHECK(host.fence_interrupts_connected());
  CHECK(device.invariants_hold());
  device.reset(); }

TEST(virtio_gpu_rvk_realize, a_host_without_a_mutex_for_fence_interrupts_is_refused)
{ MockHost host;
  host.add_gpu(first);
  VirtioGpuRvk device(host);
  char bytes[96];
  TextBuffer message(bytes, sizeof bytes);
  host.fail_mutex_at(1u);
  CHECK_STATUS(device.realize("auto", false, nullptr, message), Status::no_memory);
  CHECK(std::strstr(message.text(), "no mutex for fence interrupts") != nullptr);
  CHECK(!device.realized());
  TextBuffer again(bytes, sizeof bytes);
  CHECK_STATUS(device.realize("auto", false, nullptr, again), Status::ok);
  CHECK_EQ(host.logged(), 1u); }

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
                       static_cast<void>(d->realize("x", false, nullptr, message));
                       d->reset();
                       static_cast<void>(d->create_context(1u));
                       static_cast<void>(d->destroy_context(1u));
                       static_cast<void>(d->control(nullptr, 0x0100u));
                   },
                   &device, thread),
               Status::ok);
  host.join_thread(thread);
  /* realize, its fence interrupts and its binding, reset, both tables, the
   * status and the interrupts under it, create, destroy and its table, and
   * control, which an unrealized device answers no further */
  for (int violation = 0; violation < 12; violation++) CHECK(host.claim(HostViolation::main_loop_required));
  CHECK_EQ(host.unclaimed_violations(), 0u);
  CHECK_EQ(host.logged(), 1u); }

TEST(virtio_gpu_rvk_invariants, a_context_on_an_unrealized_device_or_a_broken_table_or_blob_table_is_detected)
{ MockHost host;
  VirtioGpuRvk unrealized(host);
  av::VirtioGpuRvkProbe::orphan_context(unrealized, 1u);
  CHECK(!unrealized.invariants_hold());
  unrealized.reset();
  host.add_gpu(first);
  VirtioGpuRvk broken(host);
  char bytes[64];
  TextBuffer message(bytes, sizeof bytes);
  CHECK_STATUS(broken.realize("auto", false, nullptr, message), Status::ok);
  av::VirtioGpuRvkProbe::duplicate_context(broken, 4u);
  CHECK(!broken.invariants_hold());
  broken.reset();
  VirtioGpuRvk broken_blobs(host);
  av::VirtioGpuRvkProbe::duplicate_blob(broken_blobs, 9u);
  CHECK(!broken_blobs.invariants_hold());
  broken_blobs.reset();
  VirtioGpuRvk broken_interrupts(host);
  av::VirtioGpuRvkProbe::break_interrupts(broken_interrupts);
  CHECK(!broken_interrupts.invariants_hold());
  av::VirtioGpuRvkProbe::repair_interrupts(broken_interrupts); }

/* A connection the status byte does not account for is the state 035's run
 * aborted from: the driver has gone and its notifiers are still registered. */
TEST(virtio_gpu_rvk_invariants, fence_interrupts_connected_without_driver_ok_are_detected)
{ MockHost host;
  host.add_gpu(first);
  VirtioGpuRvk device(host);
  char bytes[64];
  TextBuffer message(bytes, sizeof bytes);
  CHECK_STATUS(device.realize("auto", false, nullptr, message), Status::ok);
  device.status(DriverStatus::driver_ok);
  CHECK(device.invariants_hold());
  device.status(0x0Bu);                    /* the driver stopped: nothing may stay connected */
  CHECK(device.invariants_hold());
  CHECK_STATUS(av::VirtioGpuRvkProbe::connect_behind_the_status(device), Status::ok);
  CHECK(!device.invariants_hold());
  av::VirtioGpuRvkProbe::disconnect_behind_the_status(device);
  CHECK(device.invariants_hold()); }
