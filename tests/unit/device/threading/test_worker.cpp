#include "core/support/status.hpp"
#include "device/host/host.hpp"
#include "device/threading/work_item.hpp"
#include "device/threading/worker.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/host/host_violation.hpp"
#include "harness/host/mock_host.hpp"

#include <atomic>
#include <cstdint>
#include <sched.h>

using av::Host;
using av::HostViolation;
using av::MockHost;
using av::Status;
using av::WorkItem;
using av::Worker;

namespace av {

struct WorkerProbe {
    static void overfill(Worker &worker) { worker.count_ = Worker::queue_capacity + 1u; }
    static void misplace_head(Worker &worker) { worker.head_ = Worker::queue_capacity; }
};

}  // namespace av

namespace {

constexpr std::uint32_t patience_ms = 5000u;

/* A journal both sides write: which items ran, in what order, and which
 * completions reached the main loop. */
struct Journal {
    Host                      *host = nullptr;
    std::atomic<std::uint32_t> ran{0u};
    std::uint32_t              run_order[256] = {};
    std::uint32_t              completed = 0u;
    std::uint32_t              completion_order[256] = {};
};

struct Entry {
    Journal      *journal;
    std::uint32_t number;
    void         *gate;   /* an event the item waits on, or null */
};

void work(void *context)
{
    auto *const entry = static_cast<Entry *>(context);
    entry->journal->host->require_worker("test work");
    if (entry->gate != nullptr) entry->journal->host->wait_event(entry->gate);
    entry->journal->run_order[entry->journal->ran.load()] = entry->number;
    entry->journal->ran++;
}

void completion(void *context)
{
    auto *const entry = static_cast<Entry *>(context);
    entry->journal->host->require_main_loop("test completion");
    entry->journal->completion_order[entry->journal->completed++] = entry->number;
}

class Fixture {
public:
    Fixture() : worker_(host_) { journal_.host = &host_; }
    ~Fixture() { worker_.stop(); }

    MockHost &host() { return host_; }
    Worker   &worker() { return worker_; }
    Journal  &journal() { return journal_; }
    Entry     entry(std::uint32_t number, void *gate = nullptr) { return Entry{&journal_, number, gate}; }

private:
    MockHost host_;
    Worker   worker_;
    Journal  journal_;
};

}  // namespace

TEST(worker_post, runs_work_off_the_main_loop_and_completes_on_it_in_order)
{ Fixture fixture;
  CHECK_STATUS(fixture.worker().start(), Status::ok);
  CHECK(fixture.worker().running());
  CHECK(fixture.worker().invariants_hold());
  Entry entries[3] = {fixture.entry(1u), fixture.entry(2u), fixture.entry(3u)};
  for (Entry &entry : entries) {
      CHECK_STATUS(fixture.worker().post(WorkItem(&work, &completion, &entry)), Status::ok);
      CHECK(fixture.worker().invariants_hold());
  }
  CHECK(fixture.host().wait_for_main_loop_work(3u, patience_ms));
  CHECK_EQ(fixture.host().run_main_loop(), 3u);
  for (std::uint32_t index = 0u; index < 3u; index++) {
      CHECK_EQ(fixture.journal().run_order[index], index + 1u);
      CHECK_EQ(fixture.journal().completion_order[index], index + 1u);
  }
  CHECK_EQ(fixture.worker().pending(), 0u);
  fixture.worker().stop();
  CHECK(!fixture.worker().running());
  CHECK(fixture.worker().invariants_hold()); }

TEST(worker_post, a_full_queue_refuses_the_next_item)
{ Fixture fixture;
  void *gate = nullptr;
  CHECK_STATUS(fixture.host().create_event(gate), Status::ok);
  CHECK_STATUS(fixture.worker().start(), Status::ok);
  Entry blocker = fixture.entry(0u, gate);
  CHECK_STATUS(fixture.worker().post(WorkItem(&work, &completion, &blocker)), Status::ok);
  while (fixture.worker().pending() != 0u) sched_yield();   /* the blocker is running */
  Entry entries[Worker::queue_capacity + 1u] = {};
  for (std::uint32_t index = 0u; index < Worker::queue_capacity; index++) {
      entries[index] = fixture.entry(index + 1u);
      CHECK_STATUS(fixture.worker().post(WorkItem(&work, &completion, &entries[index])), Status::ok);
  }
  entries[Worker::queue_capacity] = fixture.entry(999u);
  CHECK_STATUS(fixture.worker().post(WorkItem(&work, &completion, &entries[Worker::queue_capacity])),
               Status::no_space);
  CHECK_EQ(fixture.worker().pending(), Worker::queue_capacity);
  fixture.host().signal_event(gate);
  CHECK(fixture.host().wait_for_main_loop_work(Worker::queue_capacity + 1u, patience_ms));
  CHECK_EQ(fixture.host().run_main_loop(), Worker::queue_capacity + 1u);
  CHECK_EQ(fixture.journal().completion_order[Worker::queue_capacity], Worker::queue_capacity);
  fixture.worker().stop();
  fixture.host().destroy_event(gate); }

namespace {

struct Releaser {
    Worker *worker;
    Host   *host;
    void   *gate;
};

/* Waits until stop has been requested, then lets the running item finish. */
void release_once_stopping(void *context)
{
    auto *const releaser = static_cast<Releaser *>(context);
    while (!releaser->worker->stopping()) sched_yield();
    releaser->host->signal_event(releaser->gate);
}

}  // namespace

TEST(worker_stop, finishes_the_running_item_and_drops_the_rest)
{ Fixture fixture;
  void *gate = nullptr;
  CHECK_STATUS(fixture.host().create_event(gate), Status::ok);
  CHECK_STATUS(fixture.worker().start(), Status::ok);
  Entry blocker = fixture.entry(1u, gate);
  CHECK_STATUS(fixture.worker().post(WorkItem(&work, &completion, &blocker)), Status::ok);
  while (fixture.worker().pending() != 0u) sched_yield();
  Entry dropped[3] = {fixture.entry(2u), fixture.entry(3u), fixture.entry(4u)};
  for (Entry &entry : dropped) CHECK_STATUS(fixture.worker().post(WorkItem(&work, &completion, &entry)), Status::ok);
  Releaser releaser = {&fixture.worker(), &fixture.host(), gate};
  void *helper = nullptr;
  CHECK_STATUS(fixture.host().start_thread(&release_once_stopping, &releaser, helper), Status::ok);
  fixture.worker().stop();
  fixture.host().join_thread(helper);
  CHECK_EQ(fixture.journal().ran.load(), 1u);
  CHECK_EQ(fixture.host().run_main_loop(), 1u);
  CHECK(!fixture.worker().stopping());
  CHECK_EQ(fixture.worker().pending(), 0u);
  CHECK_STATUS(fixture.worker().post(WorkItem(&work, &completion, &dropped[0])), Status::invalid_state);
  CHECK_STATUS(fixture.worker().start(), Status::ok);   /* restartable */
  fixture.host().destroy_event(gate); }

TEST(worker_start, a_second_start_is_refused_and_each_host_failure_releases_everything)
{ Fixture fixture;
  CHECK_STATUS(fixture.worker().start(), Status::ok);
  CHECK_STATUS(fixture.worker().start(), Status::invalid_state);
  fixture.worker().stop();
  fixture.worker().stop();   /* stopping a stopped worker does nothing */
  for (int failure = 0; failure < 3; failure++) {
      if (failure == 0) fixture.host().fail_event_at(1u);
      if (failure == 1) fixture.host().fail_mutex_at(1u);
      if (failure == 2) fixture.host().fail_thread_start_at(1u);
      CHECK_STATUS(fixture.worker().start(), Status::no_memory);
      CHECK(!fixture.worker().running());
      CHECK_EQ(fixture.host().live_events(), 0u);
      CHECK_EQ(fixture.host().live_mutexes(), 0u);
  } }

TEST(worker, lifecycle_is_main_loop_only_and_work_is_worker_only)
{ Fixture fixture;
  struct Call {
      Worker *worker;
      Status  status;
  } call = {&fixture.worker(), Status::ok};
  void *thread = nullptr;
  CHECK_STATUS(fixture.host().start_thread(
                   [](void *context) {
                       auto *const c = static_cast<Call *>(context);
                       c->status = c->worker->post(WorkItem());
                       static_cast<void>(c->worker->pending());
                   },
                   &call, thread),
               Status::ok);
  fixture.host().join_thread(thread);
  CHECK(fixture.host().claim(HostViolation::main_loop_required));
  CHECK(fixture.host().claim(HostViolation::main_loop_required));
  CHECK_STATUS(call.status, Status::invalid_state);
  CHECK_STATUS(fixture.worker().start(), Status::ok);
  fixture.worker().stop();
  Entry entry = fixture.entry(1u);
  work(&entry);   /* worker work run on the main loop */
  CHECK(fixture.host().claim(HostViolation::worker_required)); }

TEST(worker_invariants, a_queue_past_capacity_or_a_head_outside_it_is_detected)
{ MockHost host;
  Worker overfilled(host);
  av::WorkerProbe::overfill(overfilled);
  CHECK(!overfilled.invariants_hold());
  Worker misplaced(host);
  av::WorkerProbe::misplace_head(misplaced);
  CHECK(!misplaced.invariants_hold()); }
