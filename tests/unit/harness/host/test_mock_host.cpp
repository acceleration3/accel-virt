/* The harness's host, proven before any device code relies on it: real
 * threads, events that wake once per signal, the main loop's ordering, and the
 * violations and failures it must catch. */
#include "device/host/host.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/context/gpu_identity.hpp"
#include "harness/host/host_violation.hpp"
#include "harness/host/mock_host.hpp"
#include "harness/host/mock_request.hpp"

#include <atomic>
#include <cstdint>
#include <cstring>

using av::GpuIdentity;
using av::Host;
using av::HostViolation;
using av::MockHost;
using av::Status;

namespace {

struct Counter {
    std::atomic<std::uint32_t> value{0u};
};

void bump(void *counter) { static_cast<Counter *>(counter)->value++; }

/* A worker body: waits for one signal, then reports on the main loop. */
struct Rendezvous {
    Host         *host = nullptr;
    void         *event = nullptr;
    Counter       ran;
    std::uint32_t order[4] = {};
    std::uint32_t recorded = 0u;
};

void wait_then_report(void *context)
{
    auto *const rendezvous = static_cast<Rendezvous *>(context);
    rendezvous->host->require_worker("wait_then_report");
    rendezvous->host->wait_event(rendezvous->event);
    rendezvous->ran.value++;
    rendezvous->host->run_on_main_loop(&bump, &rendezvous->ran);
}

void record_first(void *context) { auto *r = static_cast<Rendezvous *>(context); r->order[r->recorded++] = 1u; }
void record_second(void *context) { auto *r = static_cast<Rendezvous *>(context); r->order[r->recorded++] = 2u; }

void touch_main_loop_state(void *host) { static_cast<Host *>(host)->require_main_loop("touch_main_loop_state"); }

}  // namespace

TEST(mock_host_start_thread, runs_the_entry_on_another_thread_that_hops_back_to_the_main_loop)
{ MockHost mock; Host &host = mock;
  Rendezvous rendezvous;
  rendezvous.host = &host;
  CHECK_STATUS(host.create_event(rendezvous.event), Status::ok);
  void *thread = nullptr;
  CHECK_STATUS(host.start_thread(&wait_then_report, &rendezvous, thread), Status::ok);
  CHECK_EQ(mock.live_threads(), 1u);
  host.signal_event(rendezvous.event);
  CHECK(mock.wait_for_main_loop_work(1u, 5000u));
  CHECK_EQ(mock.run_main_loop(), 1u);
  CHECK_EQ(rendezvous.ran.value.load(), 2u);
  host.join_thread(thread);
  host.destroy_event(rendezvous.event);
  CHECK_EQ(mock.live_threads(), 0u);
  CHECK_EQ(mock.live_events(), 0u); }

TEST(mock_host_run_on_main_loop, runs_in_call_order)
{ MockHost mock;
  Rendezvous rendezvous;
  mock.run_on_main_loop(&record_first, &rendezvous);
  mock.run_on_main_loop(&record_second, &rendezvous);
  CHECK(mock.wait_for_main_loop_work(2u, 0u));
  CHECK(!mock.wait_for_main_loop_work(3u, 10u));
  CHECK_EQ(mock.run_main_loop(), 2u);
  CHECK_EQ(rendezvous.order[0], 1u);
  CHECK_EQ(rendezvous.order[1], 2u);
  CHECK_EQ(mock.run_main_loop(), 0u); }

TEST(mock_host_events, a_signal_before_the_wait_is_kept_and_many_signals_wake_once)
{ MockHost mock;
  void *event = nullptr;
  CHECK_STATUS(mock.create_event(event), Status::ok);
  mock.signal_event(event);
  mock.signal_event(event);
  mock.wait_event(event);   /* returns at once */
  mock.destroy_event(event); }

TEST(mock_host_mutexes, lock_and_unlock_and_catch_misuse)
{ MockHost mock;
  void *mutex = nullptr;
  CHECK_STATUS(mock.create_mutex(mutex), Status::ok);
  mock.lock_mutex(mutex);
  mock.destroy_mutex(mutex);
  CHECK(mock.claim(HostViolation::held_mutex));
  CHECK_STATUS(mock.create_mutex(mutex), Status::ok);
  mock.unlock_mutex(mutex);
  CHECK(mock.claim(HostViolation::held_mutex));
  mock.lock_mutex(mutex);
  mock.unlock_mutex(mutex);
  mock.destroy_mutex(mutex);
  CHECK_EQ(mock.live_mutexes(), 0u); }

TEST(mock_host, unknown_handles_are_violations)
{ MockHost mock;
  int stranger = 0;
  mock.join_thread(&stranger);
  mock.destroy_event(&stranger);
  mock.signal_event(&stranger);
  mock.wait_event(&stranger);
  mock.destroy_mutex(&stranger);
  mock.lock_mutex(&stranger);
  mock.unlock_mutex(&stranger);
  static_cast<void>(mock.gpu_identity(0u));
  for (int count = 0; count < 8; count++) CHECK(mock.claim(HostViolation::unknown_handle));
  CHECK(!mock.claim(HostViolation::unknown_handle));
  CHECK_EQ(mock.unclaimed_violations(), 0u); }

TEST(mock_host, the_contract_is_checked_on_both_sides)
{ MockHost mock;
  mock.require_worker("main loop running worker work");
  CHECK(mock.claim(HostViolation::worker_required));
  void *thread = nullptr;
  CHECK_STATUS(mock.start_thread(&touch_main_loop_state, &mock, thread), Status::ok);
  mock.join_thread(thread);
  CHECK(mock.claim(HostViolation::main_loop_required));
  mock.log("once");
  CHECK_EQ(mock.logged(), 1u); }

TEST(mock_host, every_create_can_be_failed_and_the_catalog_answers_what_was_added)
{ MockHost mock;
  void *handle = &mock;
  mock.fail_thread_start_at(1u);
  CHECK_STATUS(mock.start_thread(&bump, nullptr, handle), Status::no_memory);
  CHECK(handle == nullptr);
  mock.fail_event_at(2u);
  void *first = nullptr;
  CHECK_STATUS(mock.create_event(first), Status::ok);
  CHECK_STATUS(mock.create_event(handle), Status::no_memory);
  mock.destroy_event(first);
  mock.fail_mutex_at(1u);
  CHECK_STATUS(mock.create_mutex(handle), Status::no_memory);
  const std::uint8_t uuid[GpuIdentity::uuid_bytes] = {0xAB, 1, 2, 3};
  mock.add_gpu(uuid);
  mock.add_gpu(GpuIdentity(0x1002u, 0x744Cu, GpuIdentity::integrated_gpu, uuid, 1u, 2u, "second"));
  CHECK_EQ(mock.gpu_count(), 2u);
  CHECK(std::memcmp(mock.gpu_identity(0u).uuid(), uuid, sizeof uuid) == 0);
  CHECK(std::strcmp(mock.gpu_identity(0u).name(), "mock gpu") == 0);
  CHECK_EQ(mock.gpu_identity(1u).vendor_id(), 0x1002u); }

TEST(mock_host_memory, allocations_are_tracked_and_can_fail)
{ MockHost mock;
  void *const block = mock.allocate(16u);
  CHECK(block != nullptr);
  CHECK_EQ(mock.live_allocations(), 1u);
  mock.release(block);
  mock.release(block);
  CHECK(mock.claim(HostViolation::unknown_handle));
  mock.fail_allocation_at(1u);
  CHECK(mock.allocate(16u) == nullptr); }

TEST(mock_host_map_guest, maps_ram_up_to_a_boundary_and_bounces_the_rest)
{ MockHost mock;
  mock.add_guest_ram(0x1000u, 0x4000u);
  mock.split_guest_memory_at(0x3000u);
  mock.add_bounce_region(0x10000u, 0x1000u);
  mock.guest_ram(0x2000u)[0] = 0x5A;
  std::uint64_t length = 0x3000u;
  bool direct = false;
  void *const ram = mock.map_guest(0x2000u, length, direct);
  CHECK(direct);
  CHECK_EQ(length, 0x1000u);
  CHECK_EQ(static_cast<std::uint8_t *>(ram)[0], 0x5Au);
  length = 0x100u;
  void *const bounce = mock.map_guest(0x10000u, length, direct);
  CHECK(!direct);
  CHECK(bounce != nullptr);
  CHECK(mock.guest_ram(0x10000u) == nullptr);
  CHECK(mock.guest_ram(0x90000u) == nullptr);
  length = 0x1000u;
  CHECK(mock.map_guest(0x90000u, length, direct) == nullptr);
  CHECK_EQ(length, 0u);
  CHECK_EQ(mock.live_guest_mappings(), 2u);
  mock.unmap_guest(ram, 0x1000u);
  mock.unmap_guest(bounce, 0x100u);
  mock.unmap_guest(ram, 0x1000u);
  CHECK(mock.claim(HostViolation::unknown_handle));
  CHECK_EQ(mock.live_guest_mappings(), 0u); }

TEST(mock_host_hostmem, placements_are_tracked_and_misuse_is_caught)
{ MockHost mock;
  mock.set_hostmem_size(0x10000u);
  CHECK_EQ(mock.hostmem_size(), 0x10000u);
  void *view = nullptr;
  void *const first = mock.create_host_memory(0x2000u, view);
  CHECK(first != nullptr && view != nullptr);
  void *const second = mock.create_host_memory(0x2000u, view);
  CHECK_STATUS(mock.place_in_hostmem(first, 0u), Status::ok);
  CHECK(mock.placed_at(0u) != nullptr);
  CHECK(mock.placed_at(0x1000u) == nullptr);
  CHECK_STATUS(mock.place_in_hostmem(second, 0x1000u), Status::ok);
  CHECK(mock.claim(HostViolation::overlapping_placement));
  mock.remove_from_hostmem(0x1000u, 0x2000u);
  mock.fail_placement_at(1u);
  CHECK_STATUS(mock.place_in_hostmem(second, 0x4000u), Status::no_memory);
  CHECK_STATUS(mock.place_in_hostmem(&view, 0x4000u), Status::invalid_parameter);
  CHECK(mock.claim(HostViolation::unknown_handle));
  mock.destroy_host_memory(first);
  CHECK(mock.claim(HostViolation::unknown_handle));
  mock.remove_from_hostmem(0u, 0x1000u);
  CHECK(mock.claim(HostViolation::unknown_handle));
  CHECK_EQ(mock.live_placements(), 1u);
  mock.remove_from_hostmem(0u, 0x2000u);
  mock.destroy_host_memory(first);
  mock.destroy_host_memory(second);
  CHECK_EQ(mock.live_host_memory(), 0u);
  mock.fail_host_memory_at(1u);
  CHECK(mock.create_host_memory(0x1000u, view) == nullptr);
  CHECK(view == nullptr); }

TEST(mock_host_requests, answers_once_and_knows_the_base_class_resources)
{ MockHost mock;
  const std::uint8_t bytes[] = {7, 8};
  av::MockRequest request(bytes, sizeof bytes);
  std::uint8_t read[4] = {};
  CHECK_EQ(mock.read_request(&request, 0u, read, sizeof read), 2u);
  CHECK_EQ(read[1], 8u);
  const std::uint8_t response[] = {0x00, 0x11, 0x00, 0x00};
  mock.respond(&request, response, sizeof response);
  CHECK(request.answered());
  CHECK_EQ(request.response_type(), 0x1100u);
  CHECK_EQ(request.response_size(), 4u);
  CHECK_EQ(request.response_u32(4u), 0u);
  mock.respond(&request, response, sizeof response);
  CHECK(mock.claim(HostViolation::second_response));
  mock.add_resource(3u);
  CHECK(mock.resource_exists(3u));
  CHECK(!mock.resource_exists(4u)); }

TEST(mock_host_doorbells, a_connected_doorbell_wakes_its_event_and_misuse_is_caught)
{ MockHost mock;
  void *event = nullptr;
  void *other = nullptr;
  CHECK_STATUS(mock.create_event(event), Status::ok);
  CHECK_STATUS(mock.create_event(other), Status::ok);
  CHECK(!mock.ring_doorbell(3u));
  mock.connect_doorbell(event, 3u);
  CHECK_EQ(mock.live_doorbells(), 1u);
  CHECK(mock.ring_doorbell(3u));
  mock.wait_event(event);   /* the ring was kept */
  CHECK(!mock.ring_doorbell(4u));
  CHECK(!mock.ring_doorbell(MockHost::max_doorbells));
  mock.connect_doorbell(other, 3u);          /* the page is taken */
  mock.connect_doorbell(event, 5u);          /* the event already has one */
  mock.connect_doorbell(other, MockHost::max_doorbells);
  int stranger = 0;
  mock.connect_doorbell(&stranger, 6u);
  mock.disconnect_doorbell(other, 3u);       /* not its page */
  mock.destroy_event(event);                 /* still connected */
  for (int count = 0; count < 6; count++) CHECK(mock.claim(HostViolation::doorbell_misuse));
  CHECK_EQ(mock.live_events(), 2u);
  mock.disconnect_doorbell(event, 3u);
  CHECK_EQ(mock.live_doorbells(), 0u);
  mock.destroy_event(event);
  mock.destroy_event(other);
  void *thread = nullptr;
  CHECK_STATUS(mock.start_thread([](void *host) {
                   static_cast<MockHost *>(host)->disconnect_doorbell(nullptr, 1u);
               },
                                 &mock, thread),
               Status::ok);
  mock.join_thread(thread);
  CHECK(mock.claim(HostViolation::main_loop_required));
  CHECK(mock.claim(HostViolation::doorbell_misuse)); }
