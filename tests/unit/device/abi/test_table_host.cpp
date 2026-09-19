/* TableHost forwards every Host call through the glue's C table; run here over
 * a table that reaches the MockHost, so the forwarding and the big-lock checks
 * are proven with real threads. */
#include "core/support/status.hpp"
#include "core/trace/function_scope.hpp"
#include "core/trace/scope_binding.hpp"
#include "core/trace/trace_snapshot.hpp"
#include "core/trace/trace_table.hpp"
#include "device/abi/table_host.hpp"
#include "device/fence/fence_interrupts.hpp"
#include "device/fence/fence_reporter.hpp"
#include "device/ring/ring_consumer.hpp"
#include "harness/executor/mock_context_execution.hpp"
#include "device/host/host.hpp"
#include "device/threading/work_item.hpp"
#include "device/threading/worker.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/context/gpu_identity.hpp"
#include "harness/host/mock_host.hpp"
#include "harness/host/mock_host_ops.hpp"
#include "harness/host/mock_request.hpp"
#include "harness/trace/mock_trace_host.hpp"

#include <cstdint>
#include <cstring>

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
  av::FenceReporter fences(host, interrupts);
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
  void *const block = host.allocate(8u);
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
  void *view = &mock;
  void *const memory = host.create_host_memory(0x1000u, view);
  CHECK(memory != nullptr && view != &mock);
  CHECK_STATUS(host.place_in_hostmem(memory, 0x1000u), Status::ok);
  mock.fail_placement_at(1u);
  CHECK_STATUS(host.place_in_hostmem(memory, 0x2000u), Status::no_memory);
  host.remove_from_hostmem(0x1000u, 0x1000u);
  host.destroy_host_memory(memory);
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

/* The probes' calls: an export is a block the hostmem placement accepts, an
 * import is a window the host GPU reads back, and every refusal carries the
 * host's own number (feature 039). */
TEST(table_host, forwards_exported_gpu_memory_and_guest_imports)
{ MockHost mock; MockHostOps ops(mock); TableHost host(ops.table());
  mock.set_hostmem_size(0x4000u);
  mock.advance_clock_per_read(7u);
  CHECK_EQ(host.monotonic_nanoseconds(), 0u);
  CHECK_EQ(host.monotonic_nanoseconds(), 7u);
  CHECK_STATUS(host.bind_gpu(1u), Status::ok);
  CHECK_EQ(mock.bound_gpu(), 1u);
  mock.fail_bind_gpu_at(1u);
  CHECK_STATUS(host.bind_gpu(0u), Status::not_supported);
  void         *view = &mock;
  std::int32_t  host_error = 7;
  void *const exported = host.export_gpu_memory(0x1000u, view, host_error);
  CHECK(exported != nullptr && view != &mock);
  CHECK_EQ(host_error, 0);
  CHECK_EQ(mock.live_gpu_memory(), 1u);
  CHECK_STATUS(host.place_in_hostmem(exported, 0u), Status::ok);
  static_cast<std::uint8_t *>(view)[0] = 0x39u;
  std::uint8_t region[1] = {};
  CHECK_STATUS(host.read_hostmem(0u, region, sizeof region), Status::ok);
  CHECK_EQ(region[0], 0x39u);
  host.remove_from_hostmem(0u, 0x1000u);
  CHECK_STATUS(host.read_hostmem(0u, region, sizeof region), Status::not_supported);
  host.release_gpu_memory(exported);
  CHECK_EQ(mock.live_gpu_memory(), 0u);
  mock.refuse_export_at(1u, -12);
  view = &mock;
  CHECK(host.export_gpu_memory(0x1000u, view, host_error) == nullptr);
  CHECK_EQ(host_error, -12);
  CHECK(view == nullptr);

  mock.add_guest_ram(0x1000u, 0x2000u);
  std::uint64_t length = 0x1000u;
  bool          direct = false;
  void *const   window = host.map_guest(0x1000u, length, direct);
  CHECK(window != nullptr && direct);
  std::uint8_t *const bytes = static_cast<std::uint8_t *>(window);
  bytes[0] = 0xA5u;
  bytes[0xFFFu] = 0x5Au;
  void *const imported = host.import_guest_memory(window, length, host_error);
  CHECK(imported != nullptr);
  CHECK_EQ(host_error, 0);
  CHECK_EQ(mock.live_imports(), 1u);
  std::uint8_t seen[0x1000] = {};
  CHECK_STATUS(host.read_imported_memory(imported, 0u, seen, sizeof seen), Status::ok);
  CHECK_EQ(seen[0], 0xA5u);
  CHECK_EQ(seen[0xFFFu], 0x5Au);
  std::uint8_t tail[1] = {};
  CHECK_STATUS(host.read_imported_memory(imported, 0xFFFu, tail, sizeof tail), Status::ok);
  CHECK_EQ(tail[0], 0x5Au);
  mock.refuse_import_read_at(1u);
  CHECK_STATUS(host.read_imported_memory(imported, 0u, seen, sizeof seen), Status::not_supported);
  host.release_guest_memory(imported);
  CHECK_EQ(mock.live_imports(), 0u);
  /* A host with no VK_EXT_external_memory_host refuses with no number of its own. */
  mock.refuse_import_at(1u, 0);
  CHECK(host.import_guest_memory(window, length, host_error) == nullptr);
  CHECK_EQ(host_error, 0);
  host.unmap_guest(window, length); }

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

/* The dma-buf over guest pages, and what a table from before it answers.
 *
 * A host that cannot make one - no udmabuf, or RAM with no file behind it -
 * is not a failure of the blob: the caller imports a host pointer instead,
 * which is what every host did before this op existed. So an older table,
 * with nothing in the slot, has to answer the same refusal rather than call
 * through a null pointer. */
TEST(table_host, forwards_the_dma_buf_over_guest_pages_and_its_refusals)
{ MockHost mock; MockHostOps ops(mock); TableHost host(ops.table());
  mock.add_guest_ram(0x1000u, 0x1000u);
  std::uint64_t length = 0x1000u;
  bool          direct = false;
  void *const   run = host.map_guest(0x1000u, length, direct);
  void *const   runs[] = {run};
  const std::uint64_t sizes[] = {0x1000u};

  int fd = -1;
  const Status made = host.dmabuf_over_guest(runs, sizes, 1u, fd);
  /* A kernel with no udmabuf refuses, which is a legal answer here. */
  CHECK(made == Status::ok || made == Status::not_supported);
  if (made == Status::ok) {
      CHECK(fd >= 0);
      CHECK_EQ(mock.live_dmabufs(), 1u);
      host.close_dmabuf(fd);
      CHECK_EQ(mock.live_dmabufs(), 0u);
  }
  /* Closing what was never made is not a close of anything. */
  host.close_dmabuf(-1);
  CHECK_EQ(mock.live_dmabufs(), 0u);

  mock.fail_dmabuf_at(1u);
  int refused = -1;
  CHECK_STATUS(host.dmabuf_over_guest(runs, sizes, 1u, refused), Status::not_supported);
  CHECK_EQ(refused, -1);

  /* A table with the slot empty, as one built before the op has it. */
  RvkHostOps older = ops.table();
  older.dmabuf_over_guest = nullptr;
  older.close_dmabuf = nullptr;
  TableHost before(older);
  int        none = -1;
  CHECK_STATUS(before.dmabuf_over_guest(runs, sizes, 1u, none), Status::not_supported);
  before.close_dmabuf(7);
  CHECK_EQ(none, -1);
  host.unmap_guest(run, length); }
