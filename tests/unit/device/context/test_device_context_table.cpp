#include "core/support/status.hpp"
#include "device/context/device_context.hpp"
#include "device/context/device_context_table.hpp"
#include "device/threading/work_item.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/host/host_violation.hpp"
#include "harness/host/mock_host.hpp"

#include <atomic>
#include <cstdint>

using av::DeviceContext;
using av::DeviceContextTable;
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
                static_cast<void>(slot.construct(table.host_, id));
                if (started) static_cast<void>(slot.object().start());
                return;
            }
    }
};

}  // namespace av

TEST(device_context_table_create, each_context_gets_its_own_running_worker)
{ MockHost host; DeviceContextTable table(host);
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

TEST(device_context_table_create, refuses_id_zero_a_live_id_and_a_full_table)
{ MockHost host; DeviceContextTable table(host);
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
{ MockHost host; DeviceContextTable table(host);
  host.fail_thread_start_at(1u);
  CHECK_STATUS(table.create(5u), Status::no_memory);
  CHECK_EQ(table.live(), 0u);
  CHECK(table.invariants_hold());
  CHECK_STATUS(table.create(5u), Status::ok); }

TEST(device_context_table_destroy, stops_the_worker_once)
{ MockHost host; DeviceContextTable table(host);
  CHECK_STATUS(table.create(4u), Status::ok);
  CHECK_STATUS(table.destroy(4u), Status::ok);
  CHECK(table.invariants_hold());
  CHECK_EQ(host.live_threads(), 0u);
  CHECK_STATUS(table.destroy(4u), Status::not_found); }

TEST(device_context_table, is_main_loop_only)
{ MockHost host; DeviceContextTable table(host);
  void *thread = nullptr;
  CHECK_STATUS(host.start_thread(
                   [](void *context) {
                       auto *const t = static_cast<DeviceContextTable *>(context);
                       static_cast<void>(t->create(0u));
                       static_cast<void>(t->destroy(1u));
                       static_cast<void>(t->find(1u));
                   },
                   &table, thread),
               Status::ok);
  host.join_thread(thread);
  for (int call = 0; call < 3; call++) CHECK(host.claim(HostViolation::main_loop_required)); }

TEST(device_context_table_invariants, a_duplicate_id_a_zero_id_or_a_stopped_worker_is_detected)
{ MockHost host;
  DeviceContextTable duplicate(host);
  CHECK_STATUS(duplicate.create(6u), Status::ok);
  CHECK_STATUS(duplicate.create(8u), Status::ok);
  av::DeviceContextTableProbe::place(duplicate, 6u, true);
  CHECK(!duplicate.invariants_hold());
  DeviceContextTable zero(host);
  av::DeviceContextTableProbe::place(zero, 0u, true);
  CHECK(!zero.invariants_hold());
  DeviceContextTable stopped(host);
  av::DeviceContextTableProbe::place(stopped, 9u, false);
  CHECK(!stopped.invariants_hold()); }
