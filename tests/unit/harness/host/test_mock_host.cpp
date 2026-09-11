/* The harness's host, proven before any device code relies on it: real
 * threads, events that wake once per signal, the main loop's ordering, and the
 * violations and failures it must catch. */
#include "device/host/host.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/host/host_violation.hpp"
#include "harness/host/mock_host.hpp"

#include <atomic>
#include <cstdint>
#include <cstring>

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
  std::uint8_t uuid[Host::uuid_bytes] = {};
  mock.gpu_uuid(0u, uuid);
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
  const std::uint8_t uuid[Host::uuid_bytes] = {0xAB, 1, 2, 3};
  mock.add_gpu(uuid);
  CHECK_EQ(mock.gpu_count(), 1u);
  std::uint8_t read[Host::uuid_bytes] = {};
  mock.gpu_uuid(0u, read);
  CHECK(std::memcmp(read, uuid, sizeof uuid) == 0); }
