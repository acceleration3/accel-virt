#include "core/support/status.hpp"
#include "core/trace/state_writer.hpp"
#include "device/context/device_context.hpp"
#include "device/context/device_context_table.hpp"
#include "device/memory/blob_resource.hpp"
#include "device/memory/blob_table.hpp"
#include "device/module/driver_status.hpp"
#include "device/module/virtio_gpu_av.hpp"
#include "proto/virtio_gpu/display_mode_set.hpp"
#include "device/recording/record_kind.hpp"
#include "device/recording/record_kinds.hpp"
#include "core/support/text_buffer.hpp"
#include "harness/context/local_context_plane_probe.hpp"
#include "harness/executor/mock_executor_ops.hpp"
#include "harness/export/fake_export_consumer.hpp"
#include "harness/display/display_heads_probe.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/virtio_gpu/scanout_plane_request.hpp"
#include "harness/host/gpu_request_builder.hpp"
#include "harness/host/host_violation.hpp"
#include "harness/host/loopback_renderer_peer.hpp"
#include "harness/host/mock_host.hpp"
#include "harness/recording/recorded_file.hpp"
#include "harness/host/mock_request.hpp"
#include "harness/context/ring_blob_writer.hpp"
#include "proto/context/capset_info.hpp"
#include "proto/context/context_setup.hpp"
#include "proto/context/protocol_version.hpp"
#include "proto/context/sub_api.hpp"
#include "proto/export/export_answer.hpp"
#include "proto/export/export_request.hpp"

#include <cstdint>
#include <cstring>

using av::DriverStatus;
using av::GpuRequestBuilder;
using av::HostViolation;
using av::MockHost;
using av::MockRequest;
using av::Status;
using av::TextBuffer;
using av::VirtioGpuAv;

namespace av {

struct DeviceContextTableProbe {
    /* A running context placed in a free slot behind the table's back. */
    static void place(DeviceContextTable &table, std::uint32_t id)
    {
        for (ObjectStorage<DeviceContext> &slot : table.slots_)
            if (!slot.constructed()) {
                static_cast<void>(slot.construct(table.host_, table.blobs_, table.owners_, table.interrupts_,
                                                 table.library_, table.shares_, table.recorder_, table.ordering_, id));
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

struct VirtioGpuAvProbe {
    /* Interrupts connected with no lock to hold them apart. */
    static void break_interrupts(VirtioGpuAv &device) { FenceInterruptsProbe::connect_without_lock(device.interrupts_); }
    static void repair_interrupts(VirtioGpuAv &device) { FenceInterruptsProbe::repair(device.interrupts_); }
    /* Interrupts properly connected, but behind the status byte's back: the
     * state 035's abort left the device in, where a connection outlives the
     * driver that asked for it. */
    static Status connect_behind_the_status(VirtioGpuAv &device) { return device.interrupts_.connect(); }
    static void   disconnect_behind_the_status(VirtioGpuAv &device) { device.interrupts_.disconnect(); }
    /* A head that believes it is showing on a device whose display never
     * agreed to show anything, which is what the heads' own invariant
     * catches and the device's has to carry (feature 058). */
    static av::DisplayHeads &heads(VirtioGpuAv &device) { return device.heads_; }
    /* The ring header a guest writes before its setup. */
    static void write_ring_header(VirtioGpuAv &device, std::uint32_t id)
    {
        RingBlobWriter(device.blobs_).write_header(id, 12u);
    }
    /* Two blobs claiming one id. */
    static void duplicate_blob(VirtioGpuAv &device, std::uint32_t id)
    {
        BlobTableProbe::place(device.blobs_, id);
        BlobTableProbe::place(device.blobs_, id);
    }
    /* A context the table holds on a device never realized. */
    static void orphan_context(VirtioGpuAv &device, std::uint32_t id)
    {
        static_cast<void>(device.contexts_->create(id));
    }
    /* Two live contexts claiming one id. */
    static void duplicate_context(VirtioGpuAv &device, std::uint32_t id)
    {
        static_cast<void>(device.contexts_->create(id));
        DeviceContextTableProbe::place(LocalContextPlaneProbe::table(device.local_), id);
    }
};

}  // namespace av

namespace {

constexpr std::uint8_t first[16] = {1};
constexpr std::uint8_t second[16] = {2};

}  // namespace

TEST(virtio_gpu_av_realize, binds_once_and_contexts_follow)
{ MockHost host;
  host.add_gpu(first);
  host.add_gpu(second);
  VirtioGpuAv device(host);
  char bytes[96];
  TextBuffer message(bytes, sizeof bytes);
  CHECK(!device.realized());
  CHECK_STATUS(device.create_context(1u), Status::invalid_state);
  CHECK_STATUS(device.realize("02000000-0000-0000-0000-000000000000", false, nullptr, nullptr, nullptr, nullptr, message), Status::ok);
  CHECK(device.realized());
  CHECK(device.invariants_hold());
  CHECK_EQ(device.gpu_index(), 1u);
  CHECK_STATUS(device.realize("auto", false, nullptr, nullptr, nullptr, nullptr, message), Status::invalid_state);
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

/* The PCI identity is the GPU the device bound, not the first one listed. */
TEST(virtio_gpu_av_realize, pci_identity_is_the_bound_gpus_and_nothing_before_realize)
{ MockHost host;
  host.add_gpu(first);
  host.add_gpu(second);
  host.place_gpu_on_pci(0u, MockHost::PciBus{0u, 0x03u, 0u, 0u});
  host.place_gpu_on_pci(1u, MockHost::PciBus{0u, 0x7Bu, 0u, 0u});
  host.add_file("/sys/bus/pci/devices/0000:03:00.0/vendor", "0x1002\n", 7u);
  host.add_file("/sys/bus/pci/devices/0000:03:00.0/device", "0x7550\n", 7u);
  host.add_file("/sys/bus/pci/devices/0000:03:00.0/revision", "0xc0\n", 5u);
  host.add_file("/sys/bus/pci/devices/0000:7b:00.0/vendor", "0x1002\n", 7u);
  host.add_file("/sys/bus/pci/devices/0000:7b:00.0/device", "0x13c0\n", 7u);
  host.add_file("/sys/bus/pci/devices/0000:7b:00.0/revision", "0xc9\n", 5u);
  VirtioGpuAv device(host);
  AvPciIdentity identity = {};
  CHECK(!device.pci_identity(identity));
  char bytes[96];
  TextBuffer message(bytes, sizeof bytes);
  CHECK_STATUS(device.realize("02000000-0000-0000-0000-000000000000", false, nullptr, nullptr, nullptr, nullptr, message), Status::ok);
  CHECK(device.pci_identity(identity));
  CHECK_EQ(identity.vendor_id, 0x1002u);
  CHECK_EQ(identity.device_id, 0x13C0u);
  CHECK_EQ(identity.revision, 0xC9u);
  device.reset(); }

TEST(virtio_gpu_av_realize, a_binding_failure_leaves_it_unrealized)
{ MockHost host;
  VirtioGpuAv device(host);
  char bytes[96];
  TextBuffer message(bytes, sizeof bytes);
  CHECK_STATUS(device.realize("auto", false, nullptr, nullptr, nullptr, nullptr, message), Status::not_found);
  CHECK(!device.realized());
  CHECK(device.invariants_hold()); }

TEST(virtio_gpu_av_realize, blob_on_is_refused_because_the_device_answers_blob_requests_itself)
{ MockHost host;
  host.add_gpu(first);
  VirtioGpuAv device(host);
  char bytes[128];
  TextBuffer message(bytes, sizeof bytes);
  CHECK_STATUS(device.realize("auto", true, nullptr, nullptr, nullptr, nullptr, message), Status::invalid_parameter);
  CHECK(std::strcmp(message.text(), "virtio-gpu-av: blob=on is not supported; the device answers blob requests itself") == 0);
  CHECK(!device.realized());
  CHECK_EQ(host.logged(), 1u); }

TEST(virtio_gpu_av_control, answers_blob_requests_and_reset_releases_every_blob)
{ MockHost host;
  host.add_gpu(first);
  host.set_hostmem_size(0x10000u);
  host.add_guest_ram(0x100000u, 0x1000u);
  VirtioGpuAv device(host);
  char bytes[96];
  TextBuffer message(bytes, sizeof bytes);
  CHECK_STATUS(device.realize("auto", false, nullptr, nullptr, nullptr, nullptr, message), Status::ok);
  GpuRequestBuilder create(0x010Cu);
  create.u32(1u).u32(1u).u32(0u).u32(1u).u64(0u).u64(0x1000u).u64(0x100000u).u32(0x1000u).u32(0u);
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
  CHECK_EQ(host.live_guest_mappings(), 0u);
  CHECK(device.invariants_hold()); }

/* A host blob is the library's memory, made for a live context and placed
 * where the guest maps it: what the guest writes there the library reads in
 * its own memory. A reset lets go of every one, placement and all. */
TEST(virtio_gpu_av_control, a_host_blob_is_the_librarys_memory_where_the_guest_maps_it_until_a_reset)
{ MockHost host;
  host.add_gpu(first);
  host.set_hostmem_size(0x10000u);
  av::MockExecutorOps executor(AV_EXECUTOR_SUB_API_VULKAN);
  executor.set_memory_caching(AV_MEMORY_WRITE_COMBINED);
  VirtioGpuAv device(host);
  char bytes[96];
  TextBuffer message(bytes, sizeof bytes);
  CHECK_STATUS(device.realize("auto", false, &executor.table(), nullptr, nullptr, nullptr, message), Status::ok);
  device.status(DriverStatus::driver_ok);
  CHECK_STATUS(device.create_context(5u), Status::ok);
  constexpr std::uint32_t host_blob = 0x01000000u;
  const std::uint64_t     upload = 1u;   /* HostMemoryRequest::Kind::upload */
  GpuRequestBuilder create(0x010Cu);
  create.context(5u).u32(host_blob).u32(2u).u32(1u).u32(0u).u64(upload).u64(0x2000u);
  MockRequest made(create.bytes(), create.size());
  CHECK(device.control(&made, 0x010Cu));
  CHECK_EQ(made.response_type(), 0x1100u);
  CHECK_EQ(executor.held_memory(), 1u);
  GpuRequestBuilder map(0x0208u);
  map.u32(host_blob).u32(0u).u64(0x4000u);
  MockRequest mapped(map.bytes(), map.size());
  CHECK(device.control(&mapped, 0x0208u));
  CHECK_EQ(mapped.response_type(), 0x1106u);
  CHECK_EQ(mapped.response_u32(24u), 3u);   /* VIRTIO_GPU_MAP_CACHE_WC, as the library maps it */
  auto *const window = static_cast<std::uint8_t *>(host.placed_at(0x4000u));
  std::uint8_t *const library = executor.memory_bytes(executor.last_opened_domain(), host_blob);
  CHECK(window != nullptr && library != nullptr);
  if (window != nullptr && library != nullptr) {
      window[0x1FFFu] = 0xA5u;
      CHECK_EQ(library[0x1FFFu], 0xA5u);
  }
  CHECK(device.invariants_hold());
  device.reset();
  CHECK_EQ(host.live_placements(), 0u);
  CHECK_EQ(executor.held_memory(), 0u);
  CHECK_EQ(executor.memory_frees(), 1u);
  CHECK(device.invariants_hold()); }

TEST(virtio_gpu_av_report, an_unrealized_device_says_so_and_has_no_contexts)
{ MockHost host;
  VirtioGpuAv device(host);
  char dump[256];
  av::StateWriter writer(dump, sizeof dump);
  writer.open("device", 0u);
  device.report(writer);
  CHECK(std::strcmp(writer.dump(), "device/0 realized=false\ndevice/0/host_blobs/0 live=0\n"
                                  "device/0/contexts/0 live=0") == 0); }

TEST(virtio_gpu_av_control, an_unrealized_device_answers_nothing_and_offers_no_capset)
{ MockHost host;
  VirtioGpuAv device(host);
  CHECK_EQ(device.capsets(), 0u);
  GpuRequestBuilder info(0x0108u);
  info.u32(0u).u32(0u);
  MockRequest request(info.bytes(), info.size());
  CHECK(!device.control(&request, 0x0108u));
  CHECK(!request.answered()); }

TEST(virtio_gpu_av_realize, without_an_executor_there_is_no_capset_and_no_context)
{ MockHost host;
  host.add_gpu(first);
  VirtioGpuAv device(host);
  char bytes[96];
  TextBuffer message(bytes, sizeof bytes);
  CHECK_STATUS(device.realize("auto", false, nullptr, nullptr, nullptr, nullptr, message), Status::ok);
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

TEST(virtio_gpu_av_realize, an_executor_table_this_abi_does_not_accept_is_refused)
{ MockHost host;
  host.add_gpu(first);
  av::MockExecutorOps executor(AV_EXECUTOR_SUB_API_VULKAN);
  /* A version this device was not built with is what it must refuse. */
  executor.editable().abi_version = AV_EXECUTOR_ABI_VERSION + 1u;
  VirtioGpuAv device(host);
  char bytes[128];
  TextBuffer message(bytes, sizeof bytes);
  CHECK_STATUS(device.realize("auto", false, &executor.table(), nullptr, nullptr, nullptr, message), Status::invalid_parameter);
  CHECK(std::strstr(message.text(), "not this ABI version") != nullptr);
  CHECK(!device.realized());
  CHECK_EQ(host.logged(), 1u); }

/* A reset starts the guest's shared-object ids again, so the executor is told
 * everything it shares is released - in the domain the device opened on the
 * GPU it is bound to, the one the capset names. */
TEST(virtio_gpu_av_control, a_reset_releases_every_shared_object_on_the_bound_gpu)
{ MockHost host;
  host.add_gpu(first);
  host.add_gpu(second);
  av::MockExecutorOps executor(AV_EXECUTOR_SUB_API_VULKAN);
  VirtioGpuAv device(host);
  char bytes[96];
  TextBuffer message(bytes, sizeof bytes);
  CHECK_STATUS(device.realize("02000000-0000-0000-0000-000000000000", false, &executor.table(), nullptr, nullptr,
                              nullptr, message),
               Status::ok);
  CHECK_EQ(executor.shared_releases(), 0u);
  device.reset();
  CHECK_EQ(executor.shared_releases(), 1u);
  CHECK(executor.last_shared_release() == 0u);
  CHECK_EQ(executor.domains_opened(), 1u);
  CHECK_EQ(executor.last_domain_gpu(), second[0]);
  CHECK(executor.last_domain() != 0u); }

namespace {

/* The ring and the fence page a bound context reads, made once: guest blobs
 * over guest RAM of their own, as the guest's driver makes them. */
void make_the_ring(MockHost &host, VirtioGpuAv &device)
{
    constexpr std::uint64_t ring_ram = 0x200000u;
    host.add_guest_ram(ring_ram, 0x4000u);
    for (std::uint32_t id = 1u; id <= 2u; id++) {
        GpuRequestBuilder blob(0x010Cu);
        blob.u32(id).u32(1u).u32(0u).u32(1u).u64(0u).u64(0x2000u);
        blob.u64(ring_ram + (id - 1u) * 0x2000u).u32(0x2000u).u32(0u);
        MockRequest blob_request(blob.bytes(), blob.size());
        CHECK(device.control(&blob_request, 0x010Cu));
        CHECK_EQ(blob_request.response_type(), 0x1100u);
    }
    av::VirtioGpuAvProbe::write_ring_header(device, 1u);
}

}  // namespace

TEST(virtio_gpu_av_control, answers_context_requests_and_reset_destroys_contexts_before_their_blobs)
{ MockHost host;
  host.add_gpu(first);
  host.set_hostmem_size(0x10000u);
  av::MockExecutorOps executor(AV_EXECUTOR_SUB_API_VULKAN);
  VirtioGpuAv device(host);
  char bytes[96];
  TextBuffer message(bytes, sizeof bytes);
  CHECK_STATUS(device.realize("auto", false, &executor.table(), nullptr, nullptr, nullptr, message), Status::ok);
  device.status(DriverStatus::driver_ok);
  CHECK_EQ(device.capsets(), 1u);
  GpuRequestBuilder info(0x0108u);
  info.u32(0u).u32(0u);
  MockRequest info_request(info.bytes(), info.size());
  CHECK(device.control(&info_request, 0x0108u));
  CHECK_EQ(info_request.response_type(), 0x1102u);
  make_the_ring(host, device);
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
  /* The device's own fields go on the line its module opened; a bound
   * context adds what it is bound to. */
  char dump[1024];
  av::StateWriter writer(dump, sizeof dump);
  writer.open("device", 3u);
  device.report(writer);
  CHECK(std::strncmp(writer.dump(), "device/3 realized=true gpu=0 capsets=1 blobs=2 scanouts=0 contexts=1 driver_ok=true\n"
                                    "device/3/host_blobs/0 live=0\n"
                                    "device/3/contexts/0 live=1\n"
                                    "device/3/contexts/0/context/7 bound=true sub_api=1 version=1.0 ring=1 "
                                    "ring_order=12 fence=2 doorbell=0 reply=0 attached=",
                     std::strlen("device/3 realized=true gpu=0 capsets=1 blobs=2 scanouts=0 contexts=1 driver_ok=true\n"
                                 "device/3/host_blobs/0 live=0\n"
                                 "device/3/contexts/0 live=1\n"
                                 "device/3/contexts/0/context/7 bound=true sub_api=1 version=1.0 ring=1 "
                                 "ring_order=12 fence=2 doorbell=0 reply=0 attached=")) == 0);
  /* The worker attaches the ring and makes the executor on its own thread, so
   * only the worker's line is settled here. */
  CHECK(std::strstr(writer.dump(), "\ndevice/3/contexts/0/context/7/worker/0 running=true ") != nullptr);
  device.reset();
  CHECK_EQ(device.contexts(), 0u);
  CHECK_EQ(device.blobs(), 0u);
  CHECK(device.invariants_hold()); }

TEST(virtio_gpu_av_status, the_fence_interrupts_follow_the_drivers_status_byte)
{ MockHost host;
  host.add_gpu(first);
  VirtioGpuAv device(host);
  device.status(DriverStatus::driver_ok);   /* unrealized: nothing */
  CHECK(!host.fence_interrupts_connected());
  char bytes[96];
  TextBuffer message(bytes, sizeof bytes);
  CHECK_STATUS(device.realize("auto", false, nullptr, nullptr, nullptr, nullptr, message), Status::ok);
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
  const std::uint32_t before = host.logged();
  device.status(DriverStatus::driver_ok);
  CHECK(!host.fence_interrupts_connected());
  CHECK_EQ(host.logged(), before + 1u);
  CHECK_STATUS(device.create_context(1u), Status::invalid_state);
  device.status(DriverStatus::driver_ok);
  CHECK(host.fence_interrupts_connected());
  CHECK(device.invariants_hold());
  device.reset(); }

TEST(virtio_gpu_av_realize, a_host_without_a_mutex_for_fence_interrupts_is_refused)
{ MockHost host;
  host.add_gpu(first);
  VirtioGpuAv device(host);
  char bytes[96];
  TextBuffer message(bytes, sizeof bytes);
  host.fail_mutex_at(1u);
  CHECK_STATUS(device.realize("auto", false, nullptr, nullptr, nullptr, nullptr, message), Status::no_memory);
  CHECK(std::strstr(message.text(), "no mutex for fence interrupts") != nullptr);
  CHECK(!device.realized());
  TextBuffer again(bytes, sizeof bytes);
  CHECK_STATUS(device.realize("auto", false, nullptr, nullptr, nullptr, nullptr, again), Status::ok);
  /* The refusal, and nothing else: a realize probes nothing. */
  CHECK_EQ(host.logged(), 1u); }

TEST(virtio_gpu_av_realize, a_host_without_a_mutex_for_the_blob_table_is_refused)
{ MockHost host;
  host.add_gpu(first);
  VirtioGpuAv device(host);
  char bytes[96];
  TextBuffer message(bytes, sizeof bytes);
  host.fail_mutex_at(2u);
  CHECK_STATUS(device.realize("auto", false, nullptr, nullptr, nullptr, nullptr, message), Status::no_memory);
  CHECK(std::strstr(message.text(), "no mutex for the blob table") != nullptr);
  CHECK(!device.realized());
  TextBuffer again(bytes, sizeof bytes);
  CHECK_STATUS(device.realize("auto", false, nullptr, nullptr, nullptr, nullptr, again), Status::ok);
  CHECK(device.invariants_hold());
  device.reset(); }

/* The host blob table holds a creation apart from a worker asking who owns a
 * blob; a host with no lock for it refuses the realize, said. */
TEST(virtio_gpu_av_realize, a_host_without_a_mutex_for_the_host_blob_table_is_refused)
{ MockHost host;
  host.add_gpu(first);
  VirtioGpuAv device(host);
  char bytes[96];
  TextBuffer message(bytes, sizeof bytes);
  host.fail_mutex_at(3u);
  CHECK_STATUS(device.realize("auto", false, nullptr, nullptr, nullptr, nullptr, message), Status::no_memory);
  CHECK(std::strstr(message.text(), "no mutex for the host blob table") != nullptr);
  CHECK(!device.realized());
  TextBuffer again(bytes, sizeof bytes);
  CHECK_STATUS(device.realize("auto", false, nullptr, nullptr, nullptr, nullptr, again), Status::ok);
  CHECK(device.invariants_hold());
  device.reset(); }

/* Feature 050: the contexts' shared executors need a lock of their own, and a
 * host with none refuses the realize with that said; the next one completes.
 * The Direct3D streams' ordering takes the lock before theirs, and its
 * refusal is the contexts' too. */
TEST(virtio_gpu_av_realize, a_host_with_no_mutex_for_shared_executors_refuses_the_realize)
{ MockHost host;
  host.add_gpu(first);
  VirtioGpuAv device(host);
  char bytes[128];
  TextBuffer message(bytes, sizeof bytes);
  host.fail_mutex_at(5u);
  CHECK_STATUS(device.realize("auto", false, nullptr, nullptr, nullptr, nullptr, message), Status::no_memory);
  CHECK(std::strstr(message.text(), "no mutex for the contexts' ordering and executors") != nullptr);
  CHECK(!device.realized());
  TextBuffer again(bytes, sizeof bytes);
  CHECK_STATUS(device.realize("auto", false, nullptr, nullptr, nullptr, nullptr, again), Status::ok);
  CHECK(device.realized());
  device.reset(); }

/* The stress probe is a measurement, not a capability: a host that has no
 * mutex left for it realizes anyway, with nothing counting interrupts. */
TEST(virtio_gpu_av_realize, a_host_with_no_mutex_for_the_stress_probe_still_realizes)
{ MockHost host;
  host.add_gpu(first);
  VirtioGpuAv device(host);
  char bytes[96];
  TextBuffer message(bytes, sizeof bytes);
  host.fail_mutex_at(6u);   /* the fence interrupts, the blob table, the host blob table, the D3D
                             * ordering and the shared executors take the first five */
  CHECK_STATUS(device.realize("auto", false, nullptr, nullptr, nullptr, nullptr, message), Status::ok);
  CHECK(device.realized());
  CHECK(device.invariants_hold());
  device.reset(); }

TEST(virtio_gpu_av, lifecycle_is_main_loop_only)
{ MockHost host;
  host.add_gpu(first);
  VirtioGpuAv device(host);
  void *thread = nullptr;
  CHECK_STATUS(host.start_thread(
                   [](void *context) {
                       auto *const d = static_cast<VirtioGpuAv *>(context);
                       char bytes[8];
                       TextBuffer message(bytes, sizeof bytes);
                       static_cast<void>(d->realize("x", false, nullptr, nullptr, nullptr, nullptr, message));
                       d->reset();
                       static_cast<void>(d->create_context(1u));
                       static_cast<void>(d->destroy_context(1u));
                       static_cast<void>(d->control(nullptr, 0x0100u));
                   },
                   &device, thread),
               Status::ok);
  host.join_thread(thread);
  /* realize, its fence interrupts, its blob table, its host blob table, its
   * context table and its binding, reset, its display clock, its present
   * engine, every table, the host blobs, the export socket's revocation, the
   * status and the interrupts under it, create, destroy and its table, and
   * control, which an unrealized device answers no further */
  for (int violation = 0; violation < 19; violation++) CHECK(host.claim(HostViolation::main_loop_required));
  CHECK_EQ(host.unclaimed_violations(), 0u);
  CHECK_EQ(host.logged(), 1u); }

/* One device per test: a device holds the host blob table's slots inline,
 * so a few of them would not fit on one test's stack. */
TEST(virtio_gpu_av_invariants, a_context_on_an_unrealized_device_is_detected)
{ MockHost host;
  VirtioGpuAv unrealized(host);
  av::VirtioGpuAvProbe::orphan_context(unrealized, 1u);
  CHECK(!unrealized.invariants_hold());
  unrealized.reset(); }

TEST(virtio_gpu_av_invariants, a_broken_context_table_is_detected)
{ MockHost host;
  host.add_gpu(first);
  VirtioGpuAv broken(host);
  char bytes[64];
  TextBuffer message(bytes, sizeof bytes);
  CHECK_STATUS(broken.realize("auto", false, nullptr, nullptr, nullptr, nullptr, message), Status::ok);
  av::VirtioGpuAvProbe::duplicate_context(broken, 4u);
  CHECK(!broken.invariants_hold());
  broken.reset(); }

TEST(virtio_gpu_av_invariants, a_broken_blob_table_is_detected)
{ MockHost host;
  host.add_gpu(first);
  VirtioGpuAv broken_blobs(host);
  av::VirtioGpuAvProbe::duplicate_blob(broken_blobs, 9u);
  CHECK(!broken_blobs.invariants_hold());
  broken_blobs.reset(); }

TEST(virtio_gpu_av_invariants, interrupts_connected_without_a_lock_are_detected)
{ MockHost host;
  host.add_gpu(first);
  VirtioGpuAv broken_interrupts(host);
  av::VirtioGpuAvProbe::break_interrupts(broken_interrupts);
  CHECK(!broken_interrupts.invariants_hold());
  av::VirtioGpuAvProbe::repair_interrupts(broken_interrupts); }

/* A connection the status byte does not account for is the state 035's run
 * aborted from: the driver has gone and its notifiers are still registered. */
TEST(virtio_gpu_av_invariants, fence_interrupts_connected_without_driver_ok_are_detected)
{ MockHost host;
  host.add_gpu(first);
  VirtioGpuAv device(host);
  char bytes[64];
  TextBuffer message(bytes, sizeof bytes);
  CHECK_STATUS(device.realize("auto", false, nullptr, nullptr, nullptr, nullptr, message), Status::ok);
  device.status(DriverStatus::driver_ok);
  CHECK(device.invariants_hold());
  device.status(0x0Bu);                    /* the driver stopped: nothing may stay connected */
  CHECK(device.invariants_hold());
  CHECK_STATUS(av::VirtioGpuAvProbe::connect_behind_the_status(device), Status::ok);
  CHECK(!device.invariants_hold());
  av::VirtioGpuAvProbe::disconnect_behind_the_status(device);
  CHECK(device.invariants_hold()); }

namespace {

/* The capset's feature word, as a guest reads it: GET_CAPSET, then eight bytes
 * at 24 past the 24-byte response header. */
std::uint32_t features_of(VirtioGpuAv &device)
{ GpuRequestBuilder request(0x0109u);
  request.u32(0x72u).u32(1u);
  MockRequest answer(request.bytes(), request.size());
  if (!device.control(&answer, 0x0109u)) return 0xFFFFFFFFu;
  return answer.response_u32(24u + 24u); }

}  // namespace

/* Every host-visible byte is host GPU memory placed in the window
 * (docs/host-memory.md): a device with a window offers host-visible memory
 * whatever its library, and neither retired memory bit. */
TEST(virtio_gpu_av_realize, offers_host_visible_memory_with_a_window_and_no_retired_memory_bit)
{ constexpr std::uint32_t retired = 2u | 4u;
  MockHost host;
  host.add_gpu(first);
  host.set_hostmem_size(0x100000u);
  av::MockExecutorOps library(AV_EXECUTOR_SUB_API_VULKAN);
  VirtioGpuAv device(host);
  char bytes[96];
  TextBuffer message(bytes, sizeof bytes);
  CHECK_STATUS(device.realize("auto", false, &library.table(), nullptr, nullptr, nullptr, message), Status::ok);
  device.status(DriverStatus::driver_ok);
  const std::uint32_t features = features_of(device);
  CHECK((features & static_cast<std::uint32_t>(av::CapsetInfo::feature_host_visible_memory)) != 0u);
  CHECK_EQ(features & retired, 0u);

  MockHost windowless;
  windowless.add_gpu(first);
  VirtioGpuAv without(windowless);
  TextBuffer other(bytes, sizeof bytes);
  CHECK_STATUS(without.realize("auto", false, &library.table(), nullptr, nullptr, nullptr, other), Status::ok);
  without.status(DriverStatus::driver_ok);
  CHECK_EQ(features_of(without) & static_cast<std::uint32_t>(av::CapsetInfo::feature_host_visible_memory), 0u); }

TEST(virtio_gpu_av_realize, a_device_with_no_record_property_records_nothing)
{ MockHost host;
  host.add_gpu(first);
  VirtioGpuAv device(host);
  char bytes[128];
  TextBuffer message(bytes, sizeof bytes);
  const AvRecordingOptions off = {nullptr, 0u, nullptr, 0};
  CHECK_STATUS(device.realize("auto", false, nullptr, &off, nullptr, nullptr, message), Status::ok);
  CHECK(device.realized());
  CHECK_EQ(host.recording_length("run.av"), 0u);
  CHECK_EQ(host.live_recordings(), 0u); }

TEST(virtio_gpu_av_realize, record_kinds_that_name_nothing_fail_the_realize_with_the_reason)
{ MockHost host;
  host.add_gpu(first);
  VirtioGpuAv device(host);
  char bytes[160];
  TextBuffer message(bytes, sizeof bytes);
  const AvRecordingOptions nonsense = {"run.av", 0u, "batch,nonsense", 0};
  CHECK_STATUS(device.realize("auto", false, nullptr, &nonsense, nullptr, nullptr, message), Status::invalid_parameter);
  CHECK(std::strstr(message.text(), "record-kinds") != nullptr);
  CHECK_EQ(host.live_recordings(), 0u);
  CHECK(device.invariants_hold()); }

/* The recording a realize opens: its header names the GPU this device bound
 * and the wire its contexts will speak, and nothing has happened yet. What a
 * context then records is the loopback suite's, over a real guest. */
TEST(virtio_gpu_av_realize, opens_the_recording_its_properties_asked_for)
{ MockHost host;
  host.add_gpu(first);
  host.set_hostmem_size(0x20000u);
  av::MockExecutorOps executor(AV_EXECUTOR_SUB_API_VULKAN);
  VirtioGpuAv device(host);
  char bytes[160];
  TextBuffer message(bytes, sizeof bytes);
  /* record-wrap on, so the recording wraps at its ceiling rather than stopping. */
  const AvRecordingOptions on = {"run.av", 1u << 20, "all", 1};
  CHECK_STATUS(device.realize("auto", false, &executor.table(), &on, nullptr, nullptr, message), Status::ok);
  device.status(DriverStatus::driver_ok);

  av::RecordedFile header_only(host, "run.av");
  /* MockHost's GPU, which is what this device bound. */
  CHECK_EQ(header_only.header().vendor_id(), 0x10DEu);
  CHECK_EQ(header_only.header().uuid()[0], first[0]);
  CHECK_EQ(header_only.header().kinds(), av::RecordKinds::every_kind);
  CHECK(header_only.header().limit() == (1u << 20));
  CHECK_EQ(header_only.header().wire_version().major_version(), 1u);
  CHECK_EQ(header_only.remaining(), 0u);
  device.reset();
  CHECK(device.invariants_hold()); }

namespace {


/* Binds `context` on that ring, which is what makes a realized device record a
 * setup, then takes the context away again so the next one may have the ring. */
void bind_and_destroy(VirtioGpuAv &device, std::uint32_t context)
{
    GpuRequestBuilder create(0x0200u);
    create.context(context).u32(0u).u32(0x72u).zeros(64u);
    MockRequest create_request(create.bytes(), create.size());
    CHECK(device.control(&create_request, 0x0200u));
    CHECK_EQ(create_request.response_type(), 0x1100u);
    std::uint8_t setup_bytes[av::ContextSetup::encoded_size] = {};
    CHECK_STATUS(av::ContextSetup(av::ProtocolVersion(1u, 0u), av::SubApi::vulkan, 1u, 12u, 2u, 0u, 0u)
                     .encode(setup_bytes, sizeof setup_bytes),
                 Status::ok);
    GpuRequestBuilder submit(0x0207u);
    submit.context(context).u32(av::ContextSetup::base_size).u32(0u).append(setup_bytes, sizeof setup_bytes);
    MockRequest submit_request(submit.bytes(), submit.size());
    CHECK(device.control(&submit_request, 0x0207u));
    CHECK_EQ(submit_request.response_type(), 0x1100u);
    GpuRequestBuilder destroy(0x0201u);
    destroy.context(context).zeros(8u);
    MockRequest destroy_request(destroy.bytes(), destroy.size());
    CHECK(device.control(&destroy_request, 0x0201u));
    CHECK_EQ(destroy_request.response_type(), 0x1100u);
}

}  // namespace

/* FR-001 and US1 scenario 5: `record-wrap` is what decides whether a recording
 * that reaches its ceiling rotates or stops, and the property has to reach the
 * recorder for either to happen. The ceiling here holds the file header and
 * exactly one setup record, so the second context is the one at the edge. */
TEST(virtio_gpu_av_realize, record_wrap_decides_what_the_ceiling_does)
{ struct Case { int wrap; std::uint32_t wraps; std::uint32_t kept; };
  const Case cases[] = {{1, 1u, 8u}, {0, 0u, 7u}};
  for (const Case &c : cases) {
      MockHost host;
      host.add_gpu(first);
      host.set_hostmem_size(0x20000u);
      av::MockExecutorOps executor(AV_EXECUTOR_SUB_API_VULKAN);
      VirtioGpuAv        device(host);
      char                bytes[160];
      TextBuffer          message(bytes, sizeof bytes);
      const std::uint64_t ceiling =
          av::RecordingHeader::encoded_size + av::RecordHeader::encoded_size + av::ContextSetup::base_size;
      const AvRecordingOptions options = {"run.av", ceiling, "setup", c.wrap};
      CHECK_STATUS(device.realize("auto", false, &executor.table(), &options, nullptr, nullptr, message), Status::ok);
      device.status(DriverStatus::driver_ok);
      make_the_ring(host, device);
      bind_and_destroy(device, 7u);
      bind_and_destroy(device, 8u);
      av::RecordedFile file(host, "run.av");
      CHECK_EQ(file.header().wraps(), c.wraps);
      /* Either way the file never passes the ceiling, and holds one record:
       * the newest when it wrapped, the oldest when it stopped. */
      CHECK(host.recording_length("run.av") <= ceiling);
      av::RecordHeader    record;
      const std::uint8_t *payload = file.expect(av::RecordKind::context_setup, record);
      CHECK(payload != nullptr);
      CHECK_EQ(record.context(), c.kept);
      CHECK_EQ(file.remaining(), 0u);
      device.reset();
      CHECK(device.invariants_hold());
  } }

/* The whole sequence a guest desktop performs, on a device whose host has a
 * GL display: a blob over its own pages, bound to head 0, flushed once per
 * frame, rebound to a second blob, and let go. Nothing is transferred, which
 * is the device-side form of SC-001 - the frame is never copied out of guest
 * memory, because the display samples the guest's pages directly. */
TEST(virtio_gpu_av_scanout, a_desktop_is_bound_flushed_rebound_and_let_go_without_one_transfer)
{ constexpr std::uint64_t ram = 0x40000000u;
  constexpr std::uint32_t width = 64u, height = 16u;
  constexpr std::uint64_t frame_bytes = 4096u;
  MockHost host;
  host.add_gpu(first);
  host.add_guest_ram(ram, 0x40000u);
  host.add_display_heads(1u);
  av::MockExecutorOps executor(AV_EXECUTOR_SUB_API_VULKAN);
  VirtioGpuAv        device(host);
  char                bytes[96];
  TextBuffer          message(bytes, sizeof bytes);
  CHECK_STATUS(device.realize("auto", false, &executor.table(), nullptr, nullptr, nullptr, message), Status::ok);

  const auto send = [&device, &host](const GpuRequestBuilder &builder) {
      std::uint32_t type = 0u;
      std::memcpy(&type, builder.bytes(), sizeof type);
      MockRequest request(builder.bytes(), builder.size());
      static_cast<void>(host);
      return device.control(&request, type) ? request.response_type() : 0u;
  };
  const auto create_blob = [&send](std::uint32_t id, std::uint64_t address) {
      GpuRequestBuilder create(0x010Cu);
      create.u32(id).u32(1u).u32(0u).u32(1u).u64(0u).u64(frame_bytes);
      create.u64(address).u32(static_cast<std::uint32_t>(frame_bytes)).u32(0u);
      CHECK_EQ(send(create), 0x1100u);
  };
  const auto show = [&send](std::uint32_t head, std::uint32_t blob) {
      GpuRequestBuilder scanout(0x010Du);
      scanout.u32(0u).u32(0u).u32(width).u32(height);
      scanout.u32(head).u32(blob).u32(width).u32(height).u32(1u).u32(0u);
      scanout.u32(width * 4u).u32(0u).u32(0u).u32(0u);
      scanout.u32(0u).u32(0u).u32(0u).u32(0u);
      return send(scanout);
  };
  const auto flush = [&send](std::uint32_t blob) {
      GpuRequestBuilder request(0x0104u);
      request.u32(0u).u32(0u).u32(width).u32(height).u32(blob).u32(0u);
      return send(request);
  };

  /* The capset the guest reads leaves the scanout bit clear even with a GL
   * display: the scanout shows host images by key, which a blob's dma-buf
   * cannot carry, so the guest keeps the 2D path. The device still scans a
   * blob out when asked. The features are a u64 24 bytes into the capset,
   * which is 24 bytes into the response. */
  GpuRequestBuilder capset(0x0109u);
  capset.u32(av::CapsetInfo::capset_id).u32(1u);
  MockRequest read(capset.bytes(), capset.size());
  CHECK(device.control(&read, 0x0109u));
  CHECK_EQ(read.response_u32(48u) & av::CapsetInfo::feature_dmabuf_scanout, 0u);

  create_blob(1u, ram);
  create_blob(2u, ram + 0x10000u);
  CHECK_EQ(show(0u, 1u), 0x1100u);
  CHECK_EQ(device.scanouts(), 1u);
  for (std::uint32_t frame = 0u; frame < 20u; frame++) CHECK_EQ(flush(1u), 0x1100u);
  CHECK_EQ(host.head(0u).updates, 20u);
  CHECK_EQ(host.head(0u).shows, 1u);
  CHECK_EQ(host.head(0u).hides, 0u);

  CHECK_EQ(show(0u, 2u), 0x1100u);
  CHECK_EQ(host.head(0u).shows, 2u);
  CHECK_EQ(host.head(0u).hides, 0u);
  CHECK(host.head(0u).blob != nullptr);
  /* The blob nobody shows any more is the guest's to release again; the one on
   * screen is not. */
  GpuRequestBuilder unref_first(0x0102u);
  unref_first.u32(1u).u32(0u);
  CHECK_EQ(send(unref_first), 0x1100u);
  GpuRequestBuilder unref_shown(0x0102u);
  unref_shown.u32(2u).u32(0u);
  CHECK_EQ(send(unref_shown), 0x1205u);
  CHECK(device.invariants_hold());

  /* The device's invariant folds in its heads': a head out of step with what
   * it shows is a state neither of them may report as sound. */
  { av::DisplayHeadsProbe heads(av::VirtioGpuAvProbe::heads(device));
    heads.renumber_head(0u, 1u);
    CHECK(!device.invariants_hold());
    heads.renumber_head(0u, 0u);
    CHECK(device.invariants_hold()); }

  device.reset();
  CHECK_EQ(device.scanouts(), 0u);
  CHECK_EQ(device.blobs(), 0u);
  CHECK_EQ(host.live_ranges(), 0u);
  CHECK(device.invariants_hold()); }

/* The same device on a host whose display has no head: every scanout is
 * refused, the guest keeps the 2D path of feature 038, and nothing is left
 * behind. */
TEST(virtio_gpu_av_scanout, a_host_whose_display_has_no_head_refuses_every_scanout_and_keeps_its_blobs)
{ MockHost host;
  host.add_gpu(first);
  host.add_guest_ram(0x40000000u, 0x10000u);
  VirtioGpuAv device(host);
  char         bytes[96];
  TextBuffer   message(bytes, sizeof bytes);
  CHECK_STATUS(device.realize("auto", false, nullptr, nullptr, nullptr, nullptr, message), Status::ok);
  GpuRequestBuilder create(0x010Cu);
  create.u32(1u).u32(1u).u32(0u).u32(1u).u64(0u).u64(4096u);
  create.u64(0x40000000u).u32(4096u).u32(0u);
  MockRequest made(create.bytes(), create.size());
  CHECK(device.control(&made, 0x010Cu));
  CHECK_EQ(made.response_type(), 0x1100u);

  GpuRequestBuilder scanout(0x010Du);
  scanout.u32(0u).u32(0u).u32(64u).u32(16u);
  scanout.u32(0u).u32(1u).u32(64u).u32(16u).u32(1u).u32(0u);
  scanout.u32(256u).u32(0u).u32(0u).u32(0u);
  scanout.u32(0u).u32(0u).u32(0u).u32(0u);
  MockRequest shown(scanout.bytes(), scanout.size());
  CHECK(device.control(&shown, 0x010Du));
  CHECK_EQ(shown.response_type(), 0x1200u);
  CHECK_EQ(device.scanouts(), 0u);

  GpuRequestBuilder unref(0x0102u);
  unref.u32(1u).u32(0u);
  MockRequest released(unref.bytes(), unref.size());
  CHECK(device.control(&released, 0x0102u));
  CHECK_EQ(released.response_type(), 0x1100u);
  CHECK_EQ(device.blobs(), 0u);
  CHECK(device.invariants_hold()); }

/* In QEMU's own process there is no renderer to lose: a report of one
 * changes nothing - no context lost, no interrupt raised. */
TEST(virtio_gpu_av_renderer_lost, with_the_contexts_in_process_nothing_happens)
{ MockHost host;
  host.add_gpu(first);
  av::MockExecutorOps executor(AV_EXECUTOR_SUB_API_VULKAN);
  VirtioGpuAv device(host);
  char bytes[96];
  TextBuffer message(bytes, sizeof bytes);
  CHECK_STATUS(device.realize("auto", false, &executor.table(), nullptr, nullptr, nullptr, message), Status::ok);
  device.status(DriverStatus::driver_ok);
  CHECK_STATUS(device.create_context(1u), Status::ok);
  device.renderer_lost();
  CHECK_EQ(device.contexts(), 1u);
  CHECK_EQ(host.raised(0u), 0u);
  CHECK_EQ(host.renderer_starts(), 0u);
  CHECK_STATUS(device.create_context(2u), Status::ok);
  CHECK(device.invariants_hold());
  device.reset(); }

namespace {

/* A device realized with renderer=: guest RAM shared as region 0, and the
 * renderer one loopback socket away. */
class RendererDevice {
public:
    RendererDevice()
        : executor_(AV_EXECUTOR_SUB_API_VULKAN), peer_(host_, executor_.table()), device_(host_), message_(bytes_, sizeof bytes_)
    {
        host_.add_gpu(first);
        host_.set_hostmem_size(0x100000u);
        host_.add_guest_ram(0x100000u, 0x100000u);
        host_.add_guest_region(0u, host_.guest_ram(0x100000u), 0x100000u);
        host_.connect_renderer(peer_);
    }
    ~RendererDevice() { device_.reset(); }

    MockHost                 &host() { return host_; }
    av::LoopbackRendererPeer &peer() { return peer_; }
    VirtioGpuAv             &device() { return device_; }
    TextBuffer               &message() { return message_; }
    Status realize(const char *renderer, const char *executor_path, const AvExecutorOps *executor = nullptr)
    {
        return device_.realize("auto", false, executor, nullptr, renderer, executor_path, message_);
    }

private:
    MockHost                 host_;
    av::MockExecutorOps      executor_;
    av::LoopbackRendererPeer peer_;
    VirtioGpuAv             device_;
    char                     bytes_[192] = {};
    TextBuffer               message_;
};

}  // namespace

/* With renderer= the contexts run in the renderer realize started, and the
 * capset offers what its Ready said, with host-visible memory from the window
 * QEMU keeps. */
TEST(virtio_gpu_av_realize, with_a_renderer_the_capset_is_what_the_renderer_offers)
{ RendererDevice importing;
  CHECK_STATUS(importing.realize("/usr/libexec/av-renderer", "/opt/av/libav_executor.so"), Status::ok);
  CHECK_EQ(importing.host().renderer_starts(), 1u);
  CHECK(importing.peer().service().ready());
  importing.device().status(DriverStatus::driver_ok);
  CHECK_EQ(importing.device().capsets(), 1u);
  CHECK((features_of(importing.device()) & av::CapsetInfo::feature_host_visible_memory) != 0u);
  CHECK_EQ(features_of(importing.device()) & (2u | 4u), 0u);
  /* The renderer's snapshots are in its memory, which the engine here cannot read. */
  CHECK_EQ(features_of(importing.device()) & av::CapsetInfo::feature_present_from_image, 0u);
  CHECK((features_of(importing.device()) & av::CapsetInfo::feature_present_engine) != 0u);
  CHECK_EQ(importing.peer().resources().interrupt_lines(), 64u);   /* DRIVER_OK reached the renderer */
  CHECK_STATUS(importing.device().create_context(1u), Status::ok);
  CHECK_EQ(importing.device().contexts(), 1u);
  CHECK(importing.device().invariants_hold()); }

/* The glue's report of a dead renderer reaches the plane: the renderer is
 * started again. A reset tells the renderer the interrupts are off. */
TEST(virtio_gpu_av_renderer_lost, with_a_renderer_it_is_started_again)
{ RendererDevice rig;
  CHECK_STATUS(rig.realize("/usr/libexec/av-renderer", "/opt/av/libav_executor.so"), Status::ok);
  rig.device().status(DriverStatus::driver_ok);
  CHECK_STATUS(rig.device().create_context(1u), Status::ok);
  rig.host().renderer_gone();
  rig.device().renderer_lost();
  CHECK_EQ(rig.host().renderer_starts(), 2u);
  CHECK_EQ(rig.peer().generation(), 2u);
  CHECK_EQ(rig.peer().resources().interrupt_lines(), 64u);
  CHECK_EQ(rig.device().contexts(), 1u);   /* lost, until the guest destroys it */
  rig.device().reset();
  CHECK_EQ(rig.device().contexts(), 0u);
  CHECK_EQ(rig.peer().resources().interrupt_lines(), 0u);
  CHECK(rig.device().invariants_hold()); }

/* renderer= needs executor= to name the library the renderer loads, and not
 * a table QEMU loaded itself; a renderer that does not start fails the
 * realize with its status. */
TEST(virtio_gpu_av_realize, a_renderer_without_its_library_is_refused)
{ RendererDevice rig;
  CHECK_STATUS(rig.realize("/usr/libexec/av-renderer", nullptr), Status::invalid_parameter);
  CHECK(std::strstr(rig.message().text(), "renderer= needs executor=") != nullptr);
  CHECK_EQ(rig.host().renderer_starts(), 0u); }

TEST(virtio_gpu_av_realize, a_renderer_beside_a_library_qemu_loaded_is_refused)
{ RendererDevice rig;
  av::MockExecutorOps loaded(AV_EXECUTOR_SUB_API_VULKAN);
  CHECK_STATUS(rig.realize("/usr/libexec/av-renderer", "/opt/av/libav_executor.so", &loaded.table()),
               Status::invalid_parameter);
  CHECK_EQ(rig.host().renderer_starts(), 0u); }

TEST(virtio_gpu_av_realize, a_renderer_that_does_not_start_fails_the_realize_with_its_status)
{ RendererDevice rig;
  rig.host().fail_renderer_start_at(1u);
  CHECK_STATUS(rig.realize("/usr/libexec/av-renderer", "/opt/av/libav_executor.so"), Status::not_found);
  CHECK(std::strstr(rig.message().text(), "the renderer did not start") != nullptr);
  CHECK(std::strstr(rig.message().text(), "status 3") != nullptr); }

/* An empty renderer= is no renderer: the contexts stay in QEMU. */
TEST(virtio_gpu_av_realize, an_empty_renderer_keeps_the_contexts_in_qemu)
{ RendererDevice rig;
  CHECK_STATUS(rig.realize("", nullptr), Status::ok);
  CHECK_EQ(rig.host().renderer_starts(), 0u); }

/* The modes the display offers: told, or the host monitors' for none; the
 * guest's GET_DISPLAY_MODES reads them; av_device_set_modes' answers. */
TEST(virtio_gpu_av_modes, told_modes_are_answered_and_a_bad_text_changes_nothing)
{ MockHost host;
  host.add_gpu(first);
  const char status[] = "connected\n";
  const char listed[] = "2560x1440\n1920x1080\n";
  host.add_file("/sys/class/drm/card1-DP-2/status", status, sizeof status - 1u);
  host.add_file("/sys/class/drm/card1-DP-2/modes", listed, sizeof listed - 1u);
  VirtioGpuAv device(host);
  char bytes[96];
  TextBuffer message(bytes, sizeof bytes);
  CHECK_STATUS(device.realize("auto", false, nullptr, nullptr, nullptr, nullptr, message), Status::ok);
  std::uint32_t width = 0u;
  std::uint32_t height = 0u;
  CHECK_EQ(device.offer_modes("", &width, &height), 1);
  CHECK_EQ(width, 2560u);
  CHECK_EQ(height, 1440u);
  CHECK_EQ(device.offer_modes("80x80,1920x1080", &width, &height), 1);
  CHECK_EQ(width, 80u);
  CHECK_EQ(height, 80u);
  CHECK_EQ(device.offer_modes("80x", &width, &height), -1);
  GpuRequestBuilder asked(av::DisplayModeSet::request_type);
  MockRequest       request(asked.bytes(), asked.size());
  CHECK(device.control(&request, av::DisplayModeSet::request_type));
  av::DisplayModeSet read;
  CHECK_STATUS(read.decode(request.response_bytes(), request.response_size()), Status::ok);
  CHECK_EQ(read.count(), 2u);
  CHECK_EQ(read.width(read.preferred()), 80u);
  /* No text and no monitor: nothing to offer, and the guest keeps the rectangle. */
  MockHost bare;
  bare.add_gpu(first);
  VirtioGpuAv headless(bare);
  CHECK_STATUS(headless.realize("auto", false, nullptr, nullptr, nullptr, nullptr, message), Status::ok);
  CHECK_EQ(headless.offer_modes(nullptr, nullptr, nullptr), 0);
  std::uint32_t none = 7u;
  CHECK_STATUS(headless.set_modes("", none, none), Status::not_found);
  CHECK_EQ(none, 0u);
  CHECK(device.invariants_hold()); }

namespace {

std::uint32_t export_key_51(void *, std::uint32_t, std::uint64_t key, AvExportedImage *image)
{
    if (key != 0x51u) return AV_UNKNOWN;
    image->fd = 12;
    image->slot = 1u;
    image->width = 640u;
    image->height = 480u;
    image->stride = 2560u;
    return AV_EXECUTED;
}

/* A device showing images on head 0 and its clock's page registered. */
struct ClockedDevice {
    MockHost             host;
    av::MockExecutorOps  executor;
    VirtioGpuAv         device;

    ClockedDevice() : host(), executor(AV_EXECUTOR_SUB_API_D3D), device(host)
    {
        host.add_gpu(first);
        host.add_guest_ram(0x400000u, 0x10000u);
        host.add_display_heads(2u);
        host.set_clock(1000u);
        executor.editable().export_image = export_key_51;
        char       bytes[96];
        TextBuffer message(bytes, sizeof bytes);
        static_cast<void>(device.realize("auto", false, &executor.table(), nullptr, nullptr, nullptr, message));
    }
    ~ClockedDevice() { device.reset(); }

    std::uint32_t register_page(std::uint64_t address)
    {
        GpuRequestBuilder request(0x02F4u);
        request.u64(address);
        MockRequest mock(request.bytes(), request.size());
        return device.control(&mock, 0x02F4u) ? mock.response_type() : 0u;
    }
};

}  // namespace

TEST(virtio_gpu_av_display_clock, a_frame_is_shown_at_once_until_a_page_is_registered)
{ ClockedDevice rig;
  CHECK(!rig.device.post_frame(0u, 5u, 0x51u));   /* no page: the glue shows it */
  CHECK_EQ(rig.register_page(0x401000u), 0x1100u);
  CHECK(!rig.device.post_frame(0u, 0u, 0x51u));   /* frame 0: at once, as before the clock */
  CHECK(!rig.device.post_frame(av::DisplayClock::heads, 5u, 0x51u));
  CHECK(!rig.device.post_frame(2u, 5u, 0x51u));   /* a console with no GL shows it at once */
  CHECK(rig.device.post_frame(0u, 5u, 0x51u));
  CHECK_EQ(rig.host.images_shown(0u), 0u);
  CHECK(rig.device.invariants_hold()); }

TEST(virtio_gpu_av_display_clock, the_frame_latched_at_a_blank_is_exported_and_shown_on_its_head)
{ ClockedDevice rig;
  CHECK_EQ(rig.register_page(0x401000u), 0x1100u);
  CHECK(rig.device.post_frame(0u, 5u, 0x51u));
  CHECK(rig.device.post_frame(1u, 6u, 0x52u));   /* published under no key the host knows */
  rig.host.set_clock(1000u + 16666667u);
  CHECK(rig.host.wait_for_main_loop_work(1u, 2000u));
  CHECK_EQ(rig.host.run_main_loop(), 1u);
  CHECK_EQ(rig.host.images_shown(0u), 1u);
  CHECK_EQ(rig.host.last_image(0u).fd, 12);
  CHECK_EQ(rig.host.last_image(0u).width, 640u);
  CHECK_EQ(rig.host.images_shown(1u), 0u);   /* the head keeps what it had */
  rig.device.show_latched_frames();   /* run again: nothing new was latched */
  CHECK_EQ(rig.host.images_shown(0u), 1u);
  rig.device.reset();
  CHECK(!rig.device.post_frame(0u, 7u, 0x51u));   /* a reset forgets the page */
  CHECK(rig.device.invariants_hold()); }

TEST(virtio_gpu_av_display_clock, a_host_with_no_lock_for_the_clock_is_not_realized)
{ MockHost host;
  host.add_gpu(first);
  std::uint32_t mutexes = 0u;
  {
      VirtioGpuAv counted(host);
      char       bytes[96];
      TextBuffer message(bytes, sizeof bytes);
      CHECK_STATUS(counted.realize("auto", false, nullptr, nullptr, nullptr, nullptr, message), Status::ok);
      mutexes = host.live_mutexes();
  }
  VirtioGpuAv device(host);
  host.fail_mutex_at(mutexes - 1u);   /* the clock's, before the present engine's, the last a realize makes */
  char       bytes[128];
  TextBuffer message(bytes, sizeof bytes);
  CHECK_STATUS(device.realize("auto", false, nullptr, nullptr, nullptr, nullptr, message), Status::no_memory);
  CHECK(std::strstr(message.text(), "display clock") != nullptr);
  VirtioGpuAv engineless(host);
  host.fail_mutex_at(mutexes);
  TextBuffer other(bytes, sizeof bytes);
  CHECK_STATUS(engineless.realize("auto", false, nullptr, nullptr, nullptr, nullptr, other), Status::no_memory);
  CHECK(std::strstr(other.text(), "present engine") != nullptr); }

/* The present engine is offered in the capset, takes its page and presents
 * through the control queue, and stops at a reset, forgetting the page. */
TEST(virtio_gpu_av_present_engine, the_engine_is_offered_takes_presents_and_stops_at_a_reset)
{ ClockedDevice rig;
  CHECK((features_of(rig.device) & av::CapsetInfo::feature_present_engine) != 0u);
  /* Its contexts run here, on a library that keeps snapshots. */
  CHECK((features_of(rig.device) & av::CapsetInfo::feature_present_from_image) != 0u);
  GpuRequestBuilder page(0x02F6u);
  page.u64(0x402000u);
  MockRequest registered(page.bytes(), page.size());
  CHECK(rig.device.control(&registered, 0x02F6u));
  CHECK_EQ(registered.response_type(), 0x1100u);
  std::uint32_t magic = 0u;
  std::memcpy(&magic, rig.host.guest_ram(0x402000u), sizeof magic);
  CHECK_EQ(magic, 0x00505641u);
  const std::uint32_t threads = rig.host.live_threads();
  CHECK(threads >= 1u);
  GpuRequestBuilder present(0x02F7u);
  present.u64(2u);   /* not the first ticket: refused, and completed at once */
  MockRequest queued(present.bytes(), present.size());
  CHECK(rig.device.control(&queued, 0x02F7u));
  CHECK_EQ(queued.response_type(), 0x1205u);
  std::uint64_t completed = 0u;
  std::memcpy(&completed, rig.host.guest_ram(0x402000u + 8u), sizeof completed);
  CHECK_EQ(completed, 1u);
  CHECK(rig.device.invariants_hold());
  rig.device.reset();
  CHECK_EQ(rig.host.live_threads(), threads - 1u);
  MockRequest after(present.bytes(), present.size());
  CHECK(rig.device.control(&after, 0x02F7u));
  CHECK_EQ(after.response_type(), 0x1205u);   /* no page: nothing counted */
  std::memcpy(&completed, rig.host.guest_ram(0x402000u + 8u), sizeof completed);
  CHECK_EQ(completed, 1u); }

/* The export socket's connections reach the realized device alone: hello names
 * the bound GPU by its UUID and PCI address, a surface is exported through the
 * executor the device loaded, the guest's release of it reaches the consumer,
 * and so does a reset - every surface, as 0. */
TEST(virtio_gpu_av_export, serves_the_bound_gpu_s_surfaces_and_revokes_them_at_a_release_and_a_reset)
{ MockHost host;
  host.add_gpu(first);
  host.place_gpu_on_pci(0u, MockHost::PciBus{0u, 3u, 0u, 0u});
  av::MockExecutorOps executor(AV_EXECUTOR_SUB_API_D3D);
  VirtioGpuAv device(host);
  av::FakeExportConsumer early;
  CHECK(!device.export_accepted(early.device_end(), true));
  early.refused();
  executor.serve_surface(0x51u, AvExportedImage{host.open_descriptor(), 0u, 16u, 16u, 64u, 0u, 0x34325241u, 0u, 0u},
                         4u, -1);
  char bytes[96];
  TextBuffer message(bytes, sizeof bytes);
  CHECK_STATUS(device.realize("auto", false, &executor.table(), nullptr, nullptr, nullptr, message), Status::ok);
  av::FakeExportConsumer stranger;
  CHECK(!device.export_accepted(stranger.device_end(), false));
  stranger.refused();
  av::FakeExportConsumer consumer;
  CHECK(device.export_accepted(consumer.device_end(), true));
  av::ExportAnswer answer;
  CHECK(consumer.send(av::ExportRequest(av::ExportOperation::hello, 0u, 1u)));
  CHECK_STATUS(consumer.receive(answer), Status::ok);
  CHECK_STATUS(answer.status(), Status::ok);
  CHECK_EQ(answer.greeting().uuid()[0], first[0]);
  CHECK(answer.greeting().on_pci());
  CHECK_EQ(answer.greeting().bus(), 3u);
  CHECK(consumer.send(av::ExportRequest(av::ExportOperation::export_surface, 0x51u, 2u)));
  CHECK_STATUS(consumer.receive(answer), Status::ok);
  CHECK_STATUS(answer.status(), Status::ok);
  CHECK_EQ(executor.last_domain_gpu(), first[0]);
  CHECK(executor.last_domain() != 0u);
  CHECK(consumer.descriptor() >= 0);
  GpuRequestBuilder release(0x02F1u);
  release.u64(0x51u);
  MockRequest release_request(release.bytes(), release.size());
  CHECK(device.control(&release_request, 0x02F1u));
  CHECK_STATUS(consumer.receive(answer), Status::ok);
  CHECK(answer.operation() == av::ExportOperation::revoked);
  CHECK_EQ(answer.surface(), 0x51u);
  device.reset();
  CHECK_STATUS(consumer.receive(answer), Status::ok);
  CHECK(answer.operation() == av::ExportOperation::revoked);
  CHECK_EQ(answer.surface(), 0u);
  CHECK_EQ(answer.reason(), 2u);
  CHECK(device.invariants_hold()); }


namespace {

std::uint32_t compose_two(void *, std::uint32_t, const AvScanoutPlane *planes, std::uint32_t count,
                          std::uint32_t width, std::uint32_t height, AvExportedImage *image)
{
    if (count != 2u || planes[1].key != 0x77u || width != 1280u || height != 800u) return AV_UNKNOWN;
    *image = AvExportedImage{13, 0u, width, height, width * 4u, 0u, 0x34325241u, 0u, 0u};
    return AV_EXECUTED;
}

}  // namespace

/* VirtioGpuAv::export_head: a head that shows only its base plane filling it
 * is the image under the key, as before planes; one with an overlay plane
 * (SCANOUT_PLANE) is its planes composed by the executor. */
TEST(virtio_gpu_av_export_head, composes_a_head_only_when_it_shows_more_than_its_base)
{ ClockedDevice rig;
  rig.executor.editable().compose_planes = compose_two;
  AvExportedImage image = {};
  CHECK(rig.device.export_head(0u, 0x51u, image));
  CHECK_EQ(image.fd, 12);   /* export_image's */
  const auto send = [&](std::uint32_t layer, std::uint64_t key, std::uint32_t right) {
      GpuRequestBuilder request(av::ScanoutPlaneRequest::type);
      request.u32(0u).u32(layer).u32(av::ScanoutPlaneRequest::flag_shown).u32(0u).u64(key);
      request.u32(0u).u32(0u).u32(right).u32(800u);
      request.u32(0u).u32(0u).u32(right).u32(800u);
      request.u32(1280u).u32(800u);
      MockRequest mock(request.bytes(), request.size());
      return rig.device.control(&mock, av::ScanoutPlaneRequest::type) ? mock.response_type() : 0u;
  };
  CHECK_EQ(send(0u, 0x51u, 1280u), 0x1100u);
  CHECK(rig.device.export_head(0u, 0x51u, image));
  CHECK_EQ(image.fd, 12);   /* a base filling the head is still the key's image */
  CHECK_EQ(send(1u, 0x77u, 640u), 0x1100u);
  CHECK(rig.device.export_head(0u, 0x51u, image));
  CHECK_EQ(image.fd, 13);   /* composed */
  rig.device.reset();
  CHECK(rig.device.export_head(0u, 0x51u, image)); }
