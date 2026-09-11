/* TableHost forwards every Host call through the glue's C table; run here over
 * a table that reaches the MockHost, so the forwarding and the big-lock checks
 * are proven with real threads. */
#include "core/support/status.hpp"
#include "device/abi/table_host.hpp"
#include "device/host/host.hpp"
#include "device/threading/work_item.hpp"
#include "device/threading/worker.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/context/gpu_identity.hpp"
#include "harness/host/mock_host.hpp"
#include "harness/host/mock_host_ops.hpp"
#include "harness/host/mock_request.hpp"

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
  Worker worker(host);
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
