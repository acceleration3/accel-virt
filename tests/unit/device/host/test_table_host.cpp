/* TableHost forwards every Host call through the glue's C table; run here over
 * a table that reaches the MockHost, so the forwarding and the big-lock checks
 * are proven with real threads. */
#include "core/support/status.hpp"
#include "core/trace/function_scope.hpp"
#include "core/trace/scope_binding.hpp"
#include "core/trace/trace_snapshot.hpp"
#include "core/trace/trace_table.hpp"
#include "core/support/text_buffer.hpp"
#include "device/host/table_host.hpp"
#include "device/gpu/pci_address.hpp"
#include "device/display/scanout_format.hpp"
#include "device/display/scanout_framebuffer.hpp"
#include "device/fence/fence_interrupts.hpp"
#include "device/fence/fence_reporter.hpp"
#include "device/recording/wire_recorder.hpp"
#include "device/context/ring_consumer.hpp"
#include "harness/executor/mock_context_execution.hpp"
#include "device/host/host.hpp"
#include "device/threading/work_item.hpp"
#include "device/threading/worker.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/host/host_violation.hpp"
#include "proto/context/gpu_identity.hpp"
#include "proto/virtio_gpu/set_scanout_blob_request.hpp"
#include "proto/wire/wire_reader.hpp"
#include "harness/host/mock_host.hpp"
#include "harness/host/mock_host_ops.hpp"
#include "harness/host/mock_request.hpp"
#include "harness/host/renderer_peer.hpp"
#include "harness/trace/mock_trace_host.hpp"

#include <cstdint>
#include <cstring>
#include <sys/mman.h>
#include <sys/socket.h>
#include <unistd.h>

using av::GpuIdentity;
using av::Host;
using av::MockHost;
using av::MockHostOps;
using av::Status;

using av::TableHost;
using av::WorkItem;
using av::Worker;

TEST(table_host, a_worker_runs_over_the_c_table)
{ MockHost mock; MockHostOps ops(mock); TableHost host(ops.table());
  av::FenceInterrupts interrupts(host);
  av::WireRecorder recorder(host);
  av::FenceReporter fences(host, interrupts, recorder, 1u);
  av::MockContextExecution execution(host, fences, 12u);
  av::RingConsumer ring(host, fences, execution.batches());
  Worker worker(host, ring);
  CHECK_STATUS(worker.start(), Status::ok);
  std::uint32_t ran = 0u;
  CHECK_STATUS(worker.post(WorkItem([](void *count) { (*static_cast<std::uint32_t *>(count))++; },
                                    [](void *count) { (*static_cast<std::uint32_t *>(count)) += 10u; }, &ran)),
               Status::ok);
  CHECK(mock.wait_for_main_loop_work(1u, 5000u));
  CHECK_EQ(mock.run_main_loop(), 1u);
  CHECK_EQ(ran, 11u);
  worker.stop();
  CHECK_EQ(mock.live_threads(), 0u);
  CHECK_EQ(mock.logged(), 0u); }

TEST(table_host, forwards_the_catalog_and_the_log)
{ MockHost mock; MockHostOps ops(mock); TableHost host(ops.table());
  const std::uint8_t uuid[GpuIdentity::uuid_bytes] = {9, 8, 7};
  mock.add_gpu(GpuIdentity(0x8086u, 0x46A6u, GpuIdentity::integrated_gpu, uuid, 3u, 4u,
                           "a name long enough to fill every one of the forty-eight bytes"));
  CHECK_EQ(host.gpu_count(), 1u);
  const GpuIdentity identity = host.gpu_identity(0u);
  CHECK(std::memcmp(identity.uuid(), uuid, sizeof uuid) == 0);
  CHECK_EQ(identity.vendor_id(), 0x8086u);
  CHECK_EQ(identity.device_id(), 0x46A6u);
  CHECK_EQ(identity.device_type(), GpuIdentity::integrated_gpu);
  CHECK_EQ(identity.local_memory(), 3u);
  CHECK_EQ(identity.shared_memory(), 4u);
  CHECK_EQ(std::strlen(identity.name()), GpuIdentity::name_bytes - 1u);
  host.log("hello");
  CHECK_EQ(mock.logged(), 1u); }

TEST(table_host, forwards_where_a_gpu_sits_on_pci_and_the_files_the_host_reads)
{ MockHost mock; MockHostOps ops(mock); TableHost host(ops.table());
  const std::uint8_t uuid[GpuIdentity::uuid_bytes] = {5};
  mock.add_gpu(uuid);
  mock.add_gpu(uuid);
  mock.place_gpu_on_pci(1u, MockHost::PciBus{0x1u, 0x7Bu, 0x2u, 0x3u});
  av::PciAddress address;
  CHECK_STATUS(host.gpu_pci_address(0u, address), Status::not_found);
  CHECK_STATUS(host.gpu_pci_address(1u, address), Status::ok);
  char           spelled[16] = {};
  av::TextBuffer text(spelled, sizeof spelled);
  address.append_to(text);
  CHECK(std::strcmp(text.text(), "0001:7b:02.3") == 0);
  mock.add_file("/sys/x", "0x1002\n", 7u);
  char        bytes[4] = {};
  std::size_t length = 99u;
  CHECK_STATUS(host.read_file("/sys/x", bytes, sizeof bytes, length), Status::ok);
  CHECK_EQ(length, 4u);   /* cut at the buffer */
  CHECK(std::memcmp(bytes, "0x10", 4u) == 0);
  CHECK_STATUS(host.read_file("/sys/absent", bytes, sizeof bytes, length), Status::not_found);
  CHECK_EQ(length, 0u); }

/* A glue with no file reader, and one claiming more than it was given room for,
 * are both a host that read nothing. */
TEST(table_host, a_missing_or_overlong_file_reader_reads_nothing)
{ MockHost mock; MockHostOps ops(mock);
  AvHostOps table = ops.table();
  table.read_file = nullptr;
  TableHost without(table);
  char        bytes[8] = {};
  std::size_t length = 99u;
  CHECK_STATUS(without.read_file("/sys/x", bytes, sizeof bytes, length), Status::not_found);
  CHECK_EQ(length, 0u);
  table.read_file = [](void *, const char *, char *, std::size_t size, std::size_t *read) {
      *read = size + 1u;
      return 0;
  };
  TableHost overlong(table);
  length = 99u;
  CHECK_STATUS(overlong.read_file("/sys/x", bytes, sizeof bytes, length), Status::not_found);
  CHECK_EQ(length, 0u); }

TEST(table_host, a_null_answer_from_the_table_is_no_memory)
{ MockHost mock; MockHostOps ops(mock); TableHost host(ops.table());
  void *handle = &mock;
  mock.fail_thread_start_at(1u);
  CHECK_STATUS(host.start_thread([](void *) {}, nullptr, handle), Status::no_memory);
  CHECK(handle == nullptr);
  mock.fail_event_at(1u);
  CHECK_STATUS(host.create_event(handle), Status::no_memory);
  mock.fail_mutex_at(1u);
  CHECK_STATUS(host.create_mutex(handle), Status::no_memory); }

TEST(table_host, forwards_a_try_lock_and_its_answer)
{ MockHost mock; MockHostOps ops(mock); TableHost host(ops.table());
  void *mutex = nullptr;
  CHECK_STATUS(host.create_mutex(mutex), Status::ok);
  mock.hold_mutex_elsewhere(mutex, true);
  CHECK(!host.try_lock_mutex(mutex));
  mock.hold_mutex_elsewhere(mutex, false);
  CHECK(host.try_lock_mutex(mutex));
  host.unlock_mutex(mutex);
  host.destroy_mutex(mutex);
  CHECK_EQ(mock.live_mutexes(), 0u); }

/* Every device lock goes through the table, so the table names it in the
 * trace: waited on, then held, then given back; a refused try holds nothing. */
TEST(table_host, marks_each_mutex_in_the_bound_trace)
{ MockHost mock; MockHostOps ops(mock); TableHost host(ops.table());
  av::MockTraceHost clock;
  alignas(8) std::uint8_t bytes[96u + 824u] = {};
  av::TraceTable table(clock);
  CHECK_STATUS(table.initialize(bytes, sizeof bytes, "device", 1u), Status::ok);
  av::scope_binding.bind(table);
  void *mutex = nullptr;
  CHECK_STATUS(host.create_mutex(mutex), Status::ok);
  const auto held = [&]() {
    av::TraceSnapshot snapshot;
    CHECK_STATUS(snapshot.read(bytes, sizeof bytes), Status::ok);
    return snapshot.slot(0u).holding();
  };
  const std::uint64_t address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(mutex));
  {
    const av::FunctionScope scope(av::scope_binding, av::ScopeName("test::lock"));
    host.lock_mutex(mutex);
    CHECK_EQ(held(), address);
    host.unlock_mutex(mutex);
    CHECK_EQ(held(), 0u);
    mock.hold_mutex_elsewhere(mutex, true);
    CHECK(!host.try_lock_mutex(mutex));
    CHECK_EQ(held(), 0u);
    mock.hold_mutex_elsewhere(mutex, false);
    CHECK(host.try_lock_mutex(mutex));
    CHECK_EQ(held(), address);
    host.unlock_mutex(mutex);
    CHECK_EQ(held(), 0u);
  }
  av::scope_binding.unbind();
  host.destroy_mutex(mutex);
  CHECK(table.invariants_hold()); }

TEST(table_host, checks_the_threading_contract_against_the_big_lock)
{ MockHost mock; MockHostOps ops(mock); TableHost host(ops.table());
  host.require_main_loop("on the main loop");
  CHECK_EQ(mock.logged(), 0u);
  host.require_worker("worker work on the main loop");
  CHECK_EQ(mock.logged(), 2u);
  void *thread = nullptr;
  CHECK_STATUS(host.start_thread(
                   [](void *context) {
                       Host &worker_side = *static_cast<Host *>(context);
                       worker_side.require_worker("on a worker");
                       worker_side.require_main_loop("main loop work on a worker");
                   },
                   &host, thread),
               Status::ok);
  host.join_thread(thread);
  CHECK_EQ(mock.logged(), 4u); }

TEST(table_host, forwards_memory_hostmem_and_requests)
{ MockHost mock; MockHostOps ops(mock); TableHost host(ops.table());
  void *const block = host.allocate(8u, av::Pool::paged);
  CHECK(block != nullptr);
  host.release(block);
  mock.add_guest_ram(0x1000u, 0x1000u);
  mock.add_bounce_region(0x8000u, 0x1000u);
  std::uint64_t length = 0x1000u;
  bool direct = false;
  void *const ram = host.map_guest(0x1000u, length, direct);
  CHECK(direct);
  host.unmap_guest(ram, length);
  void *const bounce = host.map_guest(0x8000u, length, direct);
  CHECK(!direct);
  host.unmap_guest(bounce, length);
  mock.set_hostmem_size(0x4000u);
  CHECK_EQ(host.hostmem_size(), 0x4000u);
  const std::uint8_t bytes[] = {1, 2, 3};
  av::MockRequest request(bytes, sizeof bytes);
  std::uint8_t read[3] = {};
  CHECK_EQ(host.read_request(&request, 1u, read, sizeof read), 2u);
  const std::uint8_t response[] = {0x05, 0x12, 0, 0};
  host.respond(&request, response, sizeof response);
  CHECK_EQ(request.response_type(), 0x1205u);
  mock.add_resource(9u);
  CHECK(host.resource_exists(9u));
  CHECK(!host.resource_exists(10u)); }

/* The executor's dma-buf placed where the guest reads it, through the C
 * table: the descriptor, its size and the offset reach QEMU unchanged, what
 * the guest then reads is the descriptor's memory, and a refusal is no
 * memory. */
TEST(table_host, places_a_dma_buf_in_the_hostmem_region_and_takes_it_out)
{ MockHost mock; MockHostOps ops(mock); TableHost host(ops.table());
  mock.set_hostmem_size(0x4000u);
  const int memory = memfd_create("table host memory", MFD_CLOEXEC);
  CHECK_EQ(ftruncate(memory, 0x2000), 0);
  auto *const library = static_cast<std::uint8_t *>(mmap(nullptr, 0x2000u, PROT_READ | PROT_WRITE, MAP_SHARED, memory, 0));
  library[0x1800] = 0x39u;
  CHECK_STATUS(host.place_in_hostmem(memory, 0x2000u, 0x1000u), Status::ok);
  CHECK_EQ(mock.last_placement().fd, memory);
  CHECK_EQ(mock.last_placement().size, 0x2000u);
  CHECK_EQ(mock.last_placement().offset, 0x1000u);
  CHECK_EQ(static_cast<std::uint8_t *>(mock.placed_at(0x1000u))[0x1800], 0x39u);
  CHECK(mock.descriptor_open(memory));   /* the descriptor stays the caller's */
  host.remove_from_hostmem(0x1000u, 0x2000u);
  CHECK_EQ(mock.live_placements(), 0u);
  mock.fail_placement_at(1u);
  CHECK_STATUS(host.place_in_hostmem(memory, 0x2000u, 0u), Status::no_memory);
  CHECK_EQ(mock.live_placements(), 0u);
  munmap(library, 0x2000u);
  close(memory); }

TEST(table_host, forwards_the_clock_and_the_gpu_binding)
{ MockHost mock; MockHostOps ops(mock); TableHost host(ops.table());
  mock.advance_clock_per_read(7u);
  CHECK_EQ(host.monotonic_nanoseconds(), 0u);
  CHECK_EQ(host.monotonic_nanoseconds(), 7u);
  CHECK_STATUS(host.bind_gpu(1u), Status::ok);
  CHECK_EQ(mock.bound_gpu(), 1u);
  mock.fail_bind_gpu_at(1u);
  CHECK_STATUS(host.bind_gpu(0u), Status::not_supported);
  CHECK_EQ(mock.bound_gpu(), 1u); }

/* The fence orders the calling thread's own stores, so it is the instruction
 * on whichever thread asks, never a call into the glue: the mock behind the
 * table sees none, from the main loop or a worker. */
TEST(table_host, fences_stores_itself_on_any_thread_without_the_glue)
{ MockHost mock; MockHostOps ops(mock); TableHost host(ops.table());
  host.fence_stores();
  void *thread = nullptr;
  CHECK_STATUS(mock.start_thread([](void *table_host) { static_cast<TableHost *>(table_host)->fence_stores(); }, &host,
                                 thread),
               Status::ok);
  mock.join_thread(thread);
  CHECK_EQ(mock.store_fences(), 0u);
  CHECK_EQ(mock.logged(), 0u); }

TEST(table_host, forwards_doorbell_connections)
{ MockHost mock; MockHostOps ops(mock); TableHost host(ops.table());
  void *event = nullptr;
  CHECK_STATUS(host.create_event(event), Status::ok);
  host.connect_doorbell(event, 63u);
  CHECK_EQ(mock.live_doorbells(), 1u);
  CHECK(mock.ring_doorbell(63u));
  host.wait_event(event);
  host.disconnect_doorbell(event, 63u);
  CHECK_EQ(mock.live_doorbells(), 0u);
  host.destroy_event(event); }

TEST(table_host, forwards_fence_interrupts)
{ MockHost mock; MockHostOps ops(mock); TableHost host(ops.table());
  CHECK_STATUS(host.connect_fence_interrupts(), Status::ok);
  host.raise_fence_interrupt(9u);
  CHECK_EQ(mock.raised(9u), 1u);
  host.disconnect_fence_interrupts();
  CHECK(!mock.fence_interrupts_connected());
  mock.fail_fence_connect_at(1u);
  CHECK_STATUS(host.connect_fence_interrupts(), Status::no_memory); }

TEST(table_host, forwards_ranges_and_aliases_and_reads_their_refusals)
{ MockHost mock; MockHostOps ops(mock); TableHost host(ops.table());
  mock.add_guest_ram(0x1000u, 0x1000u);
  std::uint64_t length = 0x1000u;
  bool          direct = false;
  void *const   run = host.map_guest(0x1000u, length, direct);
  void *base = nullptr;
  CHECK_STATUS(host.reserve_range(0x2000u, base), Status::ok);
  CHECK(base != nullptr);
  CHECK_STATUS(host.alias_guest_run(run, 0x1000u, static_cast<std::uint8_t *>(base) + 0x1000u), Status::ok);
  static_cast<std::uint8_t *>(run)[0x10] = 0x44u;
  CHECK_EQ(static_cast<std::uint8_t *>(base)[0x1010], 0x44u);
  mock.fail_alias_at(1u);
  CHECK_STATUS(host.alias_guest_run(run, 0x1000u, base), Status::not_supported);
  host.release_range(base, 0x2000u);
  CHECK_EQ(mock.live_ranges(), 0u);
  mock.fail_reserve_at(1u);
  void *refused = &mock;
  CHECK_STATUS(host.reserve_range(0x1000u, refused), Status::no_memory);
  CHECK(refused == nullptr);
  host.unmap_guest(run, length); }

/* A run laid under a dma-buf is the dma-buf's memory, holding what the RAM
 * held; restored, the RAM holds what the memory held; each refusal reads back
 * as not_supported. */
TEST(table_host, forwards_overlays_and_restores_and_reads_their_refusals)
{ MockHost mock; MockHostOps ops(mock); TableHost host(ops.table());
  mock.add_guest_ram(0x1000u, 0x1000u);
  std::uint64_t length = 0x1000u;
  bool          direct = false;
  auto *const   run = static_cast<std::uint8_t *>(host.map_guest(0x1000u, length, direct));
  run[3] = 0x21u;
  const int memory = memfd_create("overlay", MFD_CLOEXEC);
  CHECK(memory >= 0 && ftruncate(memory, 0x2000) == 0);
  CHECK_STATUS(host.overlay_guest_run(run, 0x1000u, memory, 0x1000u), Status::ok);
  CHECK_EQ(run[3], 0x21u);
  auto *const library = static_cast<std::uint8_t *>(mmap(nullptr, 0x2000u, PROT_READ | PROT_WRITE, MAP_SHARED, memory, 0));
  CHECK_EQ(library[0x1003], 0x21u);
  library[0x1004] = 0x31u;
  CHECK_EQ(run[4], 0x31u);
  CHECK_STATUS(host.restore_guest_run(run, 0x1000u), Status::ok);
  library[0x1004] = 0x41u;
  CHECK_EQ(run[4], 0x31u);
  mock.fail_overlay_at(1u);
  CHECK_STATUS(host.overlay_guest_run(run, 0x1000u, memory, 0u), Status::not_supported);
  CHECK_STATUS(host.overlay_guest_run(run, 0x1000u, memory, 0u), Status::ok);
  mock.fail_restore_at(1u);
  CHECK_STATUS(host.restore_guest_run(run, 0x1000u), Status::not_supported);
  CHECK_STATUS(host.restore_guest_run(run, 0x1000u), Status::ok);
  munmap(library, 0x2000u);
  close(memory);
  host.unmap_guest(run, length); }

TEST(table_host, forwards_the_recording_file_and_reads_each_refusal)
{ MockHost mock; MockHostOps ops(mock); TableHost host(ops.table());
  void *file = nullptr;
  CHECK_STATUS(host.create_recording("run.av", file), Status::ok);
  CHECK(file != nullptr);
  const std::uint8_t bytes[] = {1u, 2u, 3u, 4u};
  CHECK_STATUS(host.append_recording(file, bytes, sizeof bytes), Status::ok);
  CHECK_EQ(mock.recording_length("run.av"), sizeof bytes);
  CHECK_EQ(mock.recording_bytes("run.av")[2], 3u);
  CHECK_STATUS(host.truncate_recording(file), Status::ok);
  CHECK_EQ(mock.recording_length("run.av"), 0u);
  mock.fail_recording_append_at(1u);
  CHECK_STATUS(host.append_recording(file, bytes, sizeof bytes), Status::no_space);
  mock.fail_recording_truncate_at(1u);
  CHECK_STATUS(host.truncate_recording(file), Status::no_space);
  host.close_recording(file);
  CHECK_EQ(mock.live_recordings(), 0u);
  /* The bytes outlive the close, as a file does. */
  CHECK(mock.recording_bytes("run.av") != nullptr);
  mock.fail_recording_create_at(1u);
  void *refused = &mock;
  CHECK_STATUS(host.create_recording("again.av", refused), Status::no_memory);
  CHECK(refused == nullptr); }
/* The four display operations, over the same C table. What the guest sends is
 * geometry; what the glue is handed is one plane with a DRM fourcc and a
 * modifier, and nothing left for it to decide. */
TEST(table_host_scanout, a_head_is_asked_shown_updated_and_hidden_through_the_table)
{ MockHost mock; MockHostOps ops(mock); TableHost host(ops.table());
  constexpr std::uint64_t ram = 0x40000000u;
  constexpr std::uint32_t picture_width = 64u, picture_height = 16u;
  constexpr std::uint64_t picture_bytes = 4096u;
  mock.add_guest_ram(ram, 0x10000u);
  mock.add_display_heads(1u);

  CHECK(host.display_takes_dmabuf(0u));
  CHECK(!host.display_takes_dmabuf(1u));

  std::uint8_t bytes[av::SetScanoutBlobRequest::encoded_size] = {};
  const auto put32 = [&bytes](std::size_t at, std::uint32_t value) { std::memcpy(bytes + at, &value, 4); };
  put32(24u, 8u);                             /* the picture's origin in the framebuffer */
  put32(28u, 4u);
  put32(32u, 16u);
  put32(36u, 8u);
  put32(48u, picture_width);
  put32(52u, picture_height);
  put32(56u, av::ScanoutFormat::b8g8r8a8);
  put32(64u, picture_width * 4u);
  av::WireReader            reader(bytes, sizeof bytes);
  av::SetScanoutBlobRequest request;
  CHECK_STATUS(request.decode(reader), Status::ok);
  av::ScanoutFramebuffer framebuffer;
  CHECK_STATUS(framebuffer.adopt(request, picture_bytes), Status::ok);

  const std::uint8_t *const blob = mock.guest_ram(ram);
  {
      CHECK_STATUS(host.show_scanout(0u, blob, framebuffer), Status::ok);
      const MockHost::Scanout shown = mock.head(0u);
      CHECK(shown.shown);
      CHECK(shown.blob == blob);
      CHECK_EQ(shown.plane.x, 8u);
      CHECK_EQ(shown.plane.y, 4u);
      CHECK_EQ(shown.plane.width, 16u);
      CHECK_EQ(shown.plane.height, 8u);
      CHECK_EQ(shown.plane.backing_width, picture_width);
      CHECK_EQ(shown.plane.backing_height, picture_height);
      CHECK_EQ(shown.plane.stride, picture_width * 4u);
      CHECK_EQ(shown.plane.offset, 0u);
      CHECK_EQ(shown.plane.fourcc, av::ScanoutFormat(av::ScanoutFormat::b8g8r8a8).fourcc());
      host.update_scanout(0u, 1u, 2u, 3u, 4u);
      std::uint32_t x = 0u, y = 0u, width = 0u, height = 0u;
      mock.last_update(0u, x, y, width, height);
      CHECK_EQ(x, 1u); CHECK_EQ(y, 2u); CHECK_EQ(width, 3u); CHECK_EQ(height, 4u);
      host.hide_scanout(0u);
      CHECK(!mock.head(0u).shown);
  }

  /* A table from before these four: the display shows nothing, which is the
   * same answer a host with no GL display gives. */
  AvHostOps older = ops.table();
  older.display_takes_dmabuf = nullptr;
  older.show_scanout = nullptr;
  older.update_scanout = nullptr;
  older.hide_scanout = nullptr;
  TableHost before(older);
  CHECK(!before.display_takes_dmabuf(0u));
  CHECK_STATUS(before.show_scanout(0u, blob, framebuffer), Status::not_supported);
  before.update_scanout(0u, 0u, 0u, 1u, 1u);
  before.hide_scanout(0u);
  CHECK_EQ(mock.head(0u).updates, mock.head(0u).updates); }

/* A display that will not take the surface: not the guest's fault, and the
 * core is told so rather than being left to believe the head is showing. */
TEST(table_host_scanout, a_display_that_refuses_the_surface_answers_not_supported)
{ MockHost mock; MockHostOps ops(mock); TableHost host(ops.table());
  mock.add_display_heads(1u);
  av::ScanoutFramebuffer framebuffer;
  const std::uint8_t blob[16] = {};
  mock.refuse_scanout_at(1u);
  CHECK_STATUS(host.show_scanout(0u, blob, framebuffer), Status::not_supported);
  CHECK(!mock.head(0u).shown);
  /* A head the host does not have is refused; no mapping to read from, and
   * an update of a head showing nothing, are recorded contract breaks. */
  CHECK_STATUS(host.show_scanout(1u, blob, framebuffer), Status::not_supported);
  CHECK_STATUS(host.show_scanout(0u, nullptr, framebuffer), Status::not_supported);
  host.update_scanout(0u, 0u, 0u, 1u, 1u);
  CHECK(mock.claim(av::HostViolation::scanout_misuse));
  CHECK(mock.claim(av::HostViolation::scanout_misuse));
  CHECK(!mock.claim(av::HostViolation::scanout_misuse)); }

namespace {

/* The renderer's end of the socket, scripted: it keeps what it was sent and
 * answers what the test queued, including answers no glue should give. */
class ScriptedPeer final : public av::RendererPeer {
public:
    void deliver(const std::uint8_t *bytes, std::size_t length, const int *descriptors,
                 std::uint32_t count) override
    {
        delivered_++;
        last_length_ = length;
        last_byte_ = length != 0u ? bytes[0] : 0u;
        last_descriptor_ = count != 0u ? descriptors[0] : -1;
    }
    bool answer(std::uint8_t *buffer, std::size_t, std::size_t &length, int *descriptors, std::uint32_t,
                std::uint32_t &count) override
    {
        if (!answering_) return false;
        answering_ = false;
        buffer[0] = 0x5Au;
        length = answer_length_;
        count = answer_descriptor_ >= 0 ? answer_count_ : 0u;
        if (answer_descriptor_ >= 0) descriptors[0] = answer_descriptor_;
        return true;
    }
    void script(std::size_t length, int descriptor, std::uint32_t count)
    {
        answering_ = true;
        answer_length_ = length;
        answer_descriptor_ = descriptor;
        answer_count_ = count;
    }

    std::uint32_t delivered_ = 0u;
    std::size_t   last_length_ = 0u;
    std::uint8_t  last_byte_ = 0u;
    int           last_descriptor_ = -1;

private:
    bool          answering_ = false;
    std::size_t   answer_length_ = 0u;
    int           answer_descriptor_ = -1;
    std::uint32_t answer_count_ = 0u;
};

}  // namespace

/* What a renderer is handed of QEMU's: an event's own eventfd, a fence line's
 * notifier while the lines are connected, each guest RAM block's memfd and
 * where a pointer falls in one, and a host blob's memfd. Everything but the
 * event's is a duplicate the caller closes through the table. */
TEST(table_host_renderer, forwards_the_descriptors_and_regions_a_renderer_is_handed)
{ MockHost mock; MockHostOps ops(mock); TableHost host(ops.table());
  void *event = nullptr;
  CHECK_STATUS(host.create_event(event), Status::ok);
  const int eventfd = host.event_descriptor(event);
  CHECK(mock.descriptor_open(eventfd));
  CHECK_EQ(host.event_descriptor(event), eventfd);   /* the event's own, not a copy */
  CHECK_EQ(mock.live_descriptors(), 0u);
  host.destroy_event(event);
  CHECK(!mock.descriptor_open(eventfd));

  CHECK_EQ(host.fence_interrupt_descriptor(0u), -1);  /* nothing connected yet */
  CHECK_STATUS(host.connect_fence_interrupts(), Status::ok);
  const int line = host.fence_interrupt_descriptor(63u);
  CHECK(mock.descriptor_open(line));
  CHECK_EQ(mock.live_descriptors(), 1u);
  CHECK_EQ(host.fence_interrupt_descriptor(66u), -1);  /* past the fence queues, the display line and the present line */
  host.close_descriptor(line);
  CHECK(!mock.descriptor_open(line));
  CHECK_EQ(mock.live_descriptors(), 0u);
  host.disconnect_fence_interrupts();

  std::uint8_t ram[0x2000] = {};
  CHECK_EQ(host.guest_region_count(), 0u);
  const int owned = mock.add_guest_region(4u, ram, sizeof ram);
  mock.add_guest_region(5u, nullptr, 0x1000u);
  CHECK_EQ(host.guest_region_count(), 2u);
  std::uint32_t region = 0u;
  int           descriptor = -1;
  std::uint64_t size = 0u;
  CHECK_STATUS(host.guest_region(0u, region, descriptor, size), Status::ok);
  CHECK_EQ(region, 4u);
  CHECK_EQ(size, sizeof ram);
  CHECK(descriptor != owned && mock.descriptor_open(descriptor));
  CHECK(mock.guest_region_mapping(descriptor, sizeof ram) == ram);
  CHECK(mock.guest_region_mapping(descriptor, sizeof ram + 1u) == nullptr);
  CHECK_EQ(mock.live_descriptors(), 1u);
  host.close_descriptor(descriptor);
  CHECK_EQ(mock.live_descriptors(), 0u);
  CHECK(mock.descriptor_open(owned));
  int refused = 7;
  CHECK_STATUS(host.guest_region(1u, region, refused, size), Status::not_supported);  /* no shared file */
  CHECK_EQ(refused, -1);
  CHECK_STATUS(host.guest_region(2u, region, refused, size), Status::not_supported);
  std::uint64_t offset = 0u;
  region = 0u;
  CHECK_STATUS(host.locate_guest(ram + 0x10u, 0x20u, region, offset), Status::ok);
  CHECK_EQ(region, 4u);
  CHECK_EQ(offset, 0x10u);
  CHECK_STATUS(host.locate_guest(ram + 0x1FF0u, 0x20u, region, offset), Status::not_found);
  CHECK_STATUS(host.locate_guest(&mock, 1u, region, offset), Status::not_found);

  const int counted = mock.open_descriptor();
  CHECK_EQ(mock.live_descriptors(), 1u);
  host.close_descriptor(counted);
  CHECK(!mock.descriptor_open(counted));
  host.close_descriptor(-1);   /* nothing to close is not a close */
  CHECK_EQ(mock.live_descriptors(), 0u);
  CHECK_EQ(mock.unclaimed_violations(), 0u); }

/* The renderer's socket through the table: a start that fails and one that
 * does, a datagram whose descriptor the peer gets its own copy of, a receive
 * with nothing to receive, an answer with a descriptor that becomes the
 * device's, and answers no glue may give, refused whole. */
TEST(table_host_renderer, starts_sends_receives_and_kills_the_renderer_through_the_table)
{ MockHost mock; MockHostOps ops(mock); TableHost host(ops.table());
  ScriptedPeer peer;
  mock.connect_renderer(peer);
  const std::uint8_t datagram[] = {0x11u, 0x22u};
  CHECK_STATUS(host.send_renderer(datagram, sizeof datagram, nullptr, 0u), Status::not_found);  /* none started */
  mock.fail_renderer_start_at(1u);
  CHECK_STATUS(host.start_renderer("/opt/av-renderer"), Status::not_found);
  CHECK(!mock.renderer_running());
  CHECK_STATUS(host.start_renderer("/opt/av-renderer"), Status::ok);
  CHECK(std::strcmp(mock.started_renderer(), "/opt/av-renderer") == 0);
  CHECK_EQ(mock.renderer_starts(), 1u);

  const int sent = mock.open_descriptor();
  CHECK_STATUS(host.send_renderer(datagram, sizeof datagram, &sent, 1u), Status::ok);
  CHECK_EQ(peer.delivered_, 1u);
  CHECK_EQ(peer.last_length_, sizeof datagram);
  CHECK_EQ(peer.last_byte_, 0x11u);
  CHECK(peer.last_descriptor_ != sent && mock.descriptor_open(peer.last_descriptor_));
  CHECK(mock.descriptor_open(sent));   /* the caller's stays the caller's */
  ::close(peer.last_descriptor_);
  host.close_descriptor(sent);

  std::uint8_t  buffer[16] = {};
  int           descriptors[1] = {-1};
  std::size_t   length = 99u;
  std::uint32_t count = 99u;
  CHECK_STATUS(host.receive_renderer(buffer, sizeof buffer, length, descriptors, 1u, count, 10u),
               Status::invalid_state);
  CHECK_EQ(length, 0u);
  CHECK_EQ(count, 0u);
  peer.script(3u, mock.open_descriptor(), 1u);
  CHECK_EQ(mock.live_descriptors(), 1u);
  CHECK_STATUS(host.receive_renderer(buffer, sizeof buffer, length, descriptors, 1u, count, 10u), Status::ok);
  CHECK_EQ(length, 3u);
  CHECK_EQ(buffer[0], 0x5Au);
  CHECK_EQ(count, 1u);
  CHECK(mock.descriptor_open(descriptors[0]));
  host.close_descriptor(descriptors[0]);

  /* Longer than the buffer: the descriptor that came with it is closed. */
  const int cut = mock.open_descriptor();
  peer.script(sizeof buffer + 1u, cut, 1u);
  CHECK_STATUS(host.receive_renderer(buffer, sizeof buffer, length, descriptors, 1u, count, 10u),
               Status::too_large);
  CHECK_EQ(length, 0u);
  CHECK_EQ(count, 0u);
  CHECK(!mock.descriptor_open(cut));
  /* More descriptors than there was room for: those that fit are closed. */
  const int excess = mock.open_descriptor();
  peer.script(1u, excess, 2u);
  CHECK_STATUS(host.receive_renderer(buffer, sizeof buffer, length, descriptors, 1u, count, 10u),
               Status::too_large);
  CHECK(!mock.descriptor_open(excess));

  host.kill_renderer();
  CHECK_EQ(mock.renderer_kills(), 1u);
  CHECK_STATUS(host.send_renderer(datagram, sizeof datagram, nullptr, 0u), Status::not_found);
  CHECK_STATUS(host.receive_renderer(buffer, sizeof buffer, length, descriptors, 1u, count, 10u), Status::not_found);
  CHECK_STATUS(host.start_renderer("/opt/av-renderer"), Status::ok);
  mock.renderer_gone();
  CHECK_STATUS(host.receive_renderer(buffer, sizeof buffer, length, descriptors, 1u, count, 10u), Status::not_found);
  CHECK_EQ(mock.renderer_datagrams(), 1u); }

/* A table from before renderer mode: a host that runs every context in QEMU,
 * with nothing to hand a renderer and no renderer to start. */
TEST(table_host_renderer, a_table_from_before_renderer_mode_offers_none_of_it)
{ MockHost mock; MockHostOps ops(mock);
  AvHostOps older = ops.table();
  older.event_descriptor = nullptr;
  older.fence_interrupt_descriptor = nullptr;
  older.guest_region_count = nullptr;
  older.guest_region = nullptr;
  older.locate_guest = nullptr;
  older.close_descriptor = nullptr;
  older.start_renderer = nullptr;
  older.send_renderer = nullptr;
  older.receive_renderer = nullptr;
  older.kill_renderer = nullptr;
  TableHost before(older);
  std::uint8_t ram[0x1000] = {};
  mock.add_guest_region(0u, ram, sizeof ram);
  CHECK_STATUS(mock.connect_fence_interrupts(), Status::ok);
  CHECK_EQ(before.event_descriptor(&mock), -1);
  CHECK_EQ(before.fence_interrupt_descriptor(0u), -1);
  CHECK_EQ(before.guest_region_count(), 0u);
  std::uint32_t region = 0u;
  int           descriptor = 5;
  std::uint64_t size = 0u;
  CHECK_STATUS(before.guest_region(0u, region, descriptor, size), Status::not_supported);
  CHECK_EQ(descriptor, -1);
  std::uint64_t offset = 0u;
  CHECK_STATUS(before.locate_guest(ram, 1u, region, offset), Status::not_found);
  const int kept = mock.open_descriptor();
  before.close_descriptor(kept);
  CHECK(mock.descriptor_open(kept));
  mock.close_descriptor(kept);
  CHECK_STATUS(before.start_renderer("/opt/av-renderer"), Status::not_supported);
  CHECK_STATUS(before.send_renderer(ram, 1u, nullptr, 0u), Status::not_supported);
  std::size_t   length = 9u;
  std::uint32_t count = 9u;
  CHECK_STATUS(before.receive_renderer(ram, sizeof ram, length, nullptr, 0u, count, 0u), Status::not_supported);
  CHECK_EQ(length, 0u);
  CHECK_EQ(count, 0u);
  before.kill_renderer();
  CHECK_EQ(mock.renderer_kills(), 0u);
  mock.disconnect_fence_interrupts(); }

/* The glue's directory listing reaches the core as it gave it; none, or one
 * claiming more than it was given room for, is a host that lists nothing. */
TEST(table_host, a_directory_is_listed_through_the_glue_or_not_at_all)
{ MockHost mock; MockHostOps ops(mock); TableHost host(ops.table());
  mock.add_file("/sys/class/drm/card1-DP-2/status", "connected\n", 10u);
  mock.add_file("/sys/class/drm/card1-DP-2/modes", "2560x1440\n", 10u);
  mock.add_file("/sys/class/drm/card1/dev", "226:1\n", 6u);
  char        names[64] = {};
  std::size_t length = 0u;
  CHECK_STATUS(host.list_directory("/sys/class/drm", names, sizeof names, length), Status::ok);
  CHECK_EQ(length, 17u);
  CHECK(std::memcmp(names, "card1-DP-2\ncard1\n", 17u) == 0);
  CHECK_STATUS(host.list_directory("/sys/absent", names, sizeof names, length), Status::not_found);
  CHECK_EQ(length, 0u);
  AvHostOps table = ops.table();
  table.list_directory = nullptr;
  TableHost without(table);
  CHECK_STATUS(without.list_directory("/sys/class/drm", names, sizeof names, length), Status::not_found);
  table.list_directory = [](void *, const char *, char *, std::size_t size, std::size_t *listed) {
      *listed = size + 1u;
      return 0;
  };
  TableHost overlong(table);
  CHECK_STATUS(overlong.list_directory("/sys/class/drm", names, sizeof names, length), Status::not_found);
  CHECK_EQ(length, 0u); }

/* The display clock's wait: a signal wakes it with true, the deadline with
 * false, a deadline already past answers at once - and takes a signal that was
 * already there. */
TEST(table_host, wait_event_until_answers_whether_the_signal_or_the_deadline_came_first)
{ MockHost mock; MockHostOps ops(mock); TableHost host(ops.table());
  void *event = nullptr;
  CHECK_STATUS(host.create_event(event), Status::ok);
  mock.set_clock(100u);
  CHECK(!host.wait_event_until(event, 50u));   /* already past */
  host.signal_event(event);
  CHECK(host.wait_event_until(event, 50u));    /* past, and signalled */
  host.signal_event(event);
  CHECK(host.wait_event_until(event, 1000u));
  void *thread = nullptr;
  CHECK_STATUS(host.start_thread([](void *context) { static_cast<MockHost *>(context)->set_clock(2000u); }, &mock,
                                 thread),
               Status::ok);
  CHECK(!host.wait_event_until(event, 2000u));  /* the clock reached it */
  host.join_thread(thread);
  host.destroy_event(event); }

/* An image a head is to show goes through the glue, which says whether the
 * head's console could; a table from before the display clock has no glue
 * for it, and no head can. */
TEST(table_host_scanout, show_image_goes_through_the_table_or_is_not_supported)
{ MockHost mock; MockHostOps ops(mock); TableHost host(ops.table());
  mock.add_display_heads(1u);
  AvExportedImage image = {};
  image.fd = 7;
  image.width = 320u;
  CHECK_STATUS(host.show_image(0u, image), Status::ok);
  CHECK_EQ(mock.images_shown(0u), 1u);
  CHECK_EQ(mock.last_image(0u).fd, 7);
  CHECK_EQ(mock.last_image(0u).width, 320u);
  CHECK_STATUS(host.show_image(1u, image), Status::not_supported);
  AvHostOps older = ops.table();
  older.show_image = nullptr;
  TableHost old_host(older);
  CHECK_STATUS(old_host.show_image(0u, image), Status::not_supported);
  CHECK_EQ(mock.images_shown(0u), 1u); }

/* An export connection's datagrams cross the table as the glue gives them: one
 * datagram each way with the descriptor beside it, a datagram longer than the
 * buffer known by its whole length, a shut-down connection answering not_found,
 * and the close counted once. A table with no export calls offers none. */
TEST(table_host_export, a_connection_s_datagrams_cross_the_table_or_none_do)
{ MockHost mock; MockHostOps ops(mock); TableHost host(ops.table());
  int ends[2] = {-1, -1};
  CHECK(::socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0, ends) == 0);
  const std::uint8_t sent[3] = {1u, 2u, 3u};
  CHECK(::send(ends[1], sent, sizeof sent, 0) == static_cast<ssize_t>(sizeof sent));
  std::uint8_t got[2] = {};
  std::size_t  length = 0u;
  CHECK_STATUS(host.receive_export(ends[0], got, sizeof got, length), Status::ok);
  CHECK_EQ(length, 3u);   /* longer than the buffer, known by its length */
  CHECK_EQ(got[1], 2u);
  const int passed = mock.open_descriptor();
  CHECK_STATUS(host.send_export(ends[0], sent, sizeof sent, passed), Status::ok);
  CHECK_EQ(mock.export_sends(), 1u);
  std::uint8_t          back[4] = {};
  alignas(cmsghdr) char control[CMSG_SPACE(sizeof(int))] = {};
  iovec                 vector = {back, sizeof back};
  msghdr                message = {};
  message.msg_iov = &vector;
  message.msg_iovlen = 1u;
  message.msg_control = control;
  message.msg_controllen = sizeof control;
  CHECK(::recvmsg(ends[1], &message, 0) == static_cast<ssize_t>(sizeof sent));
  const cmsghdr *const header = CMSG_FIRSTHDR(&message);
  CHECK(header != nullptr && header->cmsg_type == SCM_RIGHTS);
  int arrived = -1;
  std::memcpy(&arrived, CMSG_DATA(header), sizeof arrived);
  CHECK(arrived >= 0 && arrived != passed);
  ::close(arrived);
  CHECK(mock.descriptor_open(passed));   /* still the sender's */
  mock.close_descriptor(passed);
  CHECK_STATUS(host.send_export(ends[0], sent, sizeof sent, -1), Status::ok);
  mock.fail_export_send_at(1u);
  CHECK_STATUS(host.send_export(ends[0], sent, sizeof sent, -1), Status::not_found);
  host.shutdown_export(ends[0]);
  CHECK_STATUS(host.receive_export(ends[0], got, sizeof got, length), Status::not_found);
  CHECK_EQ(length, 0u);
  /* Host::duplicate_export: a descriptor of the export path's own, which a
   * send may close while the executor keeps the one it duplicated. */
  const int copied = host.duplicate_export(ends[1]);
  CHECK(copied >= 0 && copied != ends[1]);
  ::close(copied);
  CHECK_EQ(host.duplicate_export(-1), -1);
  host.close_export(ends[0]);
  CHECK_EQ(mock.exports_closed(), 1u);
  CHECK(!mock.descriptor_open(ends[0]));

  AvHostOps older = ops.table();
  older.receive_export = nullptr;
  older.send_export = nullptr;
  older.shutdown_export = nullptr;
  older.close_export = nullptr;
  older.duplicate_export = nullptr;
  TableHost before(older);
  CHECK_EQ(before.duplicate_export(ends[1]), -1);   /* a QEMU with no duplicate makes none */
  length = 5u;
  CHECK_STATUS(before.receive_export(ends[1], got, sizeof got, length), Status::not_found);
  CHECK_EQ(length, 0u);
  CHECK_STATUS(before.send_export(ends[1], sent, sizeof sent, -1), Status::not_found);
  before.shutdown_export(ends[1]);
  before.close_export(ends[1]);
  CHECK_EQ(mock.exports_closed(), 1u);
  CHECK(mock.descriptor_open(ends[1]));
  ::close(ends[1]); }

