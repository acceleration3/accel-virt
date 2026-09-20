#include "core/support/status.hpp"
#include "device/context/device_context.hpp"
#include "device/context/device_context_table.hpp"
#include "device/recording/wire_recorder.hpp"
#include "device/execution/executor_library.hpp"
#include "device/fence/fence_interrupts.hpp"
#include "device/memory/blob_table.hpp"
#include "harness/ring/ring_blob_writer.hpp"
#include "proto/context/capset_info.hpp"
#include "proto/context/context_setup.hpp"
#include "proto/context/gpu_identity.hpp"
#include "proto/context/protocol_version.hpp"
#include "proto/context/sub_api.hpp"
#include "core/trace/state_writer.hpp"
#include "device/threading/work_item.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/host/host_violation.hpp"
#include "device/memory/host_shared_objects.hpp"
#include "harness/host/mock_host.hpp"

#include <atomic>
#include <cstdint>
#include <cstring>

using av::BlobTable;
using av::DeviceContext;
using av::DeviceContextTable;
using av::ExecutorLibrary;
using av::FenceInterrupts;
using av::HostViolation;
using av::MockHost;
using av::Status;
using av::WorkItem;

namespace av {

struct DeviceContextTableProbe {
    /* A context placed in a free slot behind the table's back. */
    static void place(DeviceContextTable &table, std::uint32_t id, bool started)
    {
        for (ObjectStorage<DeviceContext> &slot : table.slots_)
            if (!slot.constructed()) {
                static_cast<void>(slot.construct(table.host_, table.blobs_, table.shared_, table.interrupts_, table.library_, table.shares_,
                                                table.recorder_, id));
                if (started) static_cast<void>(slot.object().start());
                return;
            }
    }
};

}  // namespace av

TEST(device_context_table_create, each_context_gets_its_own_running_worker)
{ MockHost host; BlobTable blobs(host); av::HostSharedObjects shared(host); FenceInterrupts interrupts(host); ExecutorLibrary library; av::WireRecorder recorder(host);
  DeviceContextTable table(host, blobs, shared, interrupts, library, recorder);
  CHECK_STATUS(table.create(1u), Status::ok);
  CHECK_STATUS(table.create(7u), Status::ok);
  CHECK(table.invariants_hold());
  CHECK_EQ(table.live(), 2u);
  CHECK_EQ(host.live_threads(), 2u);
  DeviceContext *const context = table.find(7u);
  CHECK(context != nullptr);
  CHECK_EQ(context->id(), 7u);
  CHECK(context->running());
  std::atomic<std::uint32_t> ran{0u};
  CHECK_STATUS(context->post(WorkItem([](void *count) { (*static_cast<std::atomic<std::uint32_t> *>(count))++; },
                                      [](void *) {}, &ran)),
               Status::ok);
  CHECK(host.wait_for_main_loop_work(1u, 5000u));
  CHECK_EQ(host.run_main_loop(), 1u);
  CHECK_EQ(ran.load(), 1u);
  CHECK(table.find(2u) == nullptr); }

/* The table's line, then each context in slot order with its worker; an
 * unbound context says only that it is unbound, its ring's progress and
 * whether it has an executor. */
TEST(device_context_table_report, writes_each_context_and_its_worker)
{ MockHost host; BlobTable blobs(host); av::HostSharedObjects shared(host); FenceInterrupts interrupts(host); ExecutorLibrary library; av::WireRecorder recorder(host);
  DeviceContextTable table(host, blobs, shared, interrupts, library, recorder);
  char bytes[512];
  av::StateWriter empty(bytes, sizeof bytes);
  empty.open("device", 0u);
  table.report(empty);
  CHECK(std::strcmp(empty.dump(), "device/0\ndevice/0/contexts/0 live=0") == 0);
  CHECK_STATUS(table.create(1u), Status::ok);
  CHECK_STATUS(table.create(7u), Status::ok);
  av::StateWriter two(bytes, sizeof bytes);
  table.report(two);
  const char *const expected =
      "contexts/0 live=2\n"
      "contexts/0/context/1 bound=false attached=false faulted=false head=0 last_fence=0 batches=0 executor=false\n"
      "contexts/0/context/1/worker/0 running=true pending=0 stopping=false\n"
      "contexts/0/context/7 bound=false attached=false faulted=false head=0 last_fence=0 batches=0 executor=false\n"
      "contexts/0/context/7/worker/0 running=true pending=0 stopping=false";
  CHECK(std::strcmp(two.dump(), expected) == 0);
  CHECK(two.invariants_hold());
  table.destroy_all(); }

TEST(device_context_table_create, refuses_id_zero_a_live_id_and_a_full_table)
{ MockHost host; BlobTable blobs(host); av::HostSharedObjects shared(host); FenceInterrupts interrupts(host); ExecutorLibrary library; av::WireRecorder recorder(host);
  DeviceContextTable table(host, blobs, shared, interrupts, library, recorder);
  CHECK_STATUS(table.create(0u), Status::invalid_parameter);
  CHECK_STATUS(table.create(3u), Status::ok);
  CHECK_STATUS(table.create(3u), Status::conflict);
  for (std::uint32_t id = 100u; table.live() < DeviceContextTable::max_contexts; id++)
      CHECK_STATUS(table.create(id), Status::ok);
  CHECK_STATUS(table.create(9999u), Status::no_space);
  CHECK(table.invariants_hold());
  table.destroy_all();
  CHECK_EQ(table.live(), 0u);
  CHECK_EQ(host.live_threads(), 0u); }

TEST(device_context_table_create, a_worker_that_cannot_start_leaves_no_context)
{ MockHost host; BlobTable blobs(host); av::HostSharedObjects shared(host); FenceInterrupts interrupts(host); ExecutorLibrary library; av::WireRecorder recorder(host);
  DeviceContextTable table(host, blobs, shared, interrupts, library, recorder);
  host.fail_thread_start_at(1u);
  CHECK_STATUS(table.create(5u), Status::no_memory);
  CHECK_EQ(table.live(), 0u);
  CHECK(table.invariants_hold());
  CHECK_STATUS(table.create(5u), Status::ok); }

TEST(device_context_table_destroy, stops_the_worker_once)
{ MockHost host; BlobTable blobs(host); av::HostSharedObjects shared(host); FenceInterrupts interrupts(host); ExecutorLibrary library; av::WireRecorder recorder(host);
  DeviceContextTable table(host, blobs, shared, interrupts, library, recorder);
  CHECK_STATUS(table.create(4u), Status::ok);
  CHECK_STATUS(table.destroy(4u), Status::ok);
  CHECK(table.invariants_hold());
  CHECK_EQ(host.live_threads(), 0u);
  CHECK_STATUS(table.destroy(4u), Status::not_found); }

TEST(device_context_table_holds_doorbell, only_a_bound_context_holds_its_doorbell)
{ MockHost host; BlobTable blobs(host); av::HostSharedObjects shared(host); FenceInterrupts interrupts(host); ExecutorLibrary library; av::WireRecorder recorder(host);
  DeviceContextTable table(host, blobs, shared, interrupts, library, recorder);
  host.set_hostmem_size(0x10000u);
  CHECK_STATUS(blobs.initialize(), Status::ok);
  CHECK_STATUS(blobs.create_host(1u, 0x2000u), Status::ok);
  CHECK_STATUS(blobs.create_host(2u, 0x1000u), Status::ok);
  av::RingBlobWriter(blobs).write_header(1u, 12u);
  CHECK_STATUS(table.create(1u), Status::ok);
  CHECK_STATUS(table.create(2u), Status::ok);
  CHECK(!table.holds_doorbell(5u));
  const std::uint8_t uuid[av::GpuIdentity::uuid_bytes] = {};
  const av::CapsetInfo capset(av::ProtocolVersion(1u, 0u), 1u, 64u, 12u, 24u, 0u,
                              av::GpuIdentity(0u, 0u, 0u, uuid, 0u, 0u, ""));
  const av::ContextSetup setup(av::ProtocolVersion(1u, 0u), av::SubApi::vulkan, 1u, 12u, 2u, 5u, 0u);
  CHECK_STATUS(table.find(2u)->bind(setup, capset), Status::ok);
  CHECK(table.holds_doorbell(5u));
  CHECK(!table.holds_doorbell(0u));
  CHECK_STATUS(table.destroy(2u), Status::ok);
  CHECK(!table.holds_doorbell(5u)); }

TEST(device_context_table, is_main_loop_only)
{ MockHost host; BlobTable blobs(host); av::HostSharedObjects shared(host); FenceInterrupts interrupts(host); ExecutorLibrary library; av::WireRecorder recorder(host);
  DeviceContextTable table(host, blobs, shared, interrupts, library, recorder);
  void *thread = nullptr;
  CHECK_STATUS(host.start_thread(
                   [](void *context) {
                       auto *const t = static_cast<DeviceContextTable *>(context);
                       static_cast<void>(t->create(0u));
                       static_cast<void>(t->destroy(1u));
                       static_cast<void>(t->find(1u));
                       static_cast<void>(t->holds_doorbell(1u));
                   },
                   &table, thread),
               Status::ok);
  host.join_thread(thread);
  for (int call = 0; call < 4; call++) CHECK(host.claim(HostViolation::main_loop_required)); }

TEST(device_context_table_invariants, a_duplicate_id_a_zero_id_or_a_stopped_worker_is_detected)
{ MockHost host;
  BlobTable blobs(host);
  av::HostSharedObjects shared(host);
  FenceInterrupts interrupts(host);
  ExecutorLibrary library;
  av::WireRecorder recorder(host);
  DeviceContextTable duplicate(host, blobs, shared, interrupts, library, recorder);
  CHECK_STATUS(duplicate.create(6u), Status::ok);
  CHECK_STATUS(duplicate.create(8u), Status::ok);
  av::DeviceContextTableProbe::place(duplicate, 6u, true);
  CHECK(!duplicate.invariants_hold());
  DeviceContextTable zero(host, blobs, shared, interrupts, library, recorder);
  av::DeviceContextTableProbe::place(zero, 0u, true);
  CHECK(!zero.invariants_hold());
  DeviceContextTable stopped(host, blobs, shared, interrupts, library, recorder);
  av::DeviceContextTableProbe::place(stopped, 9u, false);
  CHECK(!stopped.invariants_hold()); }
