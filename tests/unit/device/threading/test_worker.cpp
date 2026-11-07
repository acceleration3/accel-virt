#include "core/support/status.hpp"
#include "core/trace/state_writer.hpp"
#include "device/fence/fence_interrupts.hpp"
#include "device/fence/fence_reporter.hpp"
#include "device/host/host.hpp"
#include "device/memory/blob_window.hpp"
#include "device/context/ring_consumer.hpp"
#include "device/context/ring_window.hpp"
#include "device/context/submission_watchdog.hpp"
#include "device/threading/work_item.hpp"
#include "device/threading/doorbell.hpp"
#include "device/threading/worker.hpp"
#include "device/recording/wire_recorder.hpp"
#include "device/abi/av_executor.h"
#include "harness/executor/mock_executor_ops.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/host/host_violation.hpp"
#include "harness/executor/mock_context_execution.hpp"
#include "harness/host/mock_host.hpp"
#include "harness/context/guest_ring.hpp"

#include <atomic>
#include <cstdint>
#include <cstring>
#include <sched.h>

using av::BlobWindow;
using av::GuestRing;
using av::Host;
using av::HostViolation;
using av::MockHost;
using av::RingConsumer;
using av::RingWindow;
using av::Status;
using av::WorkItem;
using av::Worker;

namespace av {

struct DoorbellProbe {
    static void forge(Doorbell &doorbell, void *event) { doorbell.event_ = event; doorbell.connected_ = true; }
    static void forget_event(Doorbell &doorbell) { doorbell.event_ = nullptr; }
    static void repair(Doorbell &doorbell) { doorbell.event_ = nullptr; doorbell.connected_ = false; }
};

struct WorkerProbe {
    static void overfill(Worker &worker) { worker.count_ = Worker::queue_capacity + 1u; }
    static void misplace_head(Worker &worker) { worker.head_ = Worker::queue_capacity; }
    static Doorbell &doorbell(Worker &worker) { return worker.doorbell_; }
    static void     *lock(Worker &worker) { return worker.lock_; }
    static bool      spin(Worker &worker) { return worker.work_within_spin(); }
};

}

namespace {

constexpr std::uint32_t patience_ms = 5000u;

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
    void         *gate;
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

class RingMemory {
public:
    RingMemory() : bytes_{}, pointer_(bytes_), length_(sizeof bytes_), guest_(bytes_, 12u) {}
    RingWindow window() const { return RingWindow(BlobWindow(&pointer_, &length_, 1u, length_), 12u); }
    GuestRing &guest() { return guest_; }

private:
    std::uint8_t  bytes_[32u + 4096u];
    void         *pointer_;
    std::uint64_t length_;
    GuestRing     guest_;
};

void attach(void *consumer) { static_cast<RingConsumer *>(consumer)->attach(); }

class Fixture {
public:
    Fixture()
        : interrupts_(host_), recorder_(host_), fences_(host_, interrupts_, recorder_, 1u),
          execution_(host_, fences_, 12u),
          ring_(host_, fences_, execution_.batches()), worker_(host_, ring_)
    {
        journal_.host = &host_;
    }
    ~Fixture() { worker_.stop(); }

    MockHost     &host() { return host_; }
    Worker       &worker() { return worker_; }
    Journal      &journal() { return journal_; }
    RingConsumer &ring() { return ring_; }
    GuestRing    &guest() { return memory_.guest(); }
    av::MockExecutorOps &ops() { return execution_.ops(); }
    RingWindow    window() const { return memory_.window(); }
    Entry         entry(std::uint32_t number, void *gate = nullptr) { return Entry{&journal_, number, gate}; }

    bool published(std::uint64_t head)
    {
        for (std::uint32_t spins = 0u; spins < 50000000u && memory_.guest().head() != head; spins++) sched_yield();
        return memory_.guest().head() == head;
    }

private:
    MockHost             host_;
    av::FenceInterrupts  interrupts_;
    av::WireRecorder     recorder_;
    av::FenceReporter    fences_;
    av::MockContextExecution execution_;
    RingConsumer         ring_;
    Worker               worker_;
    Journal      journal_;
    RingMemory   memory_;
};

}

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

TEST(worker_report, writes_its_queue_or_busy_without_waiting)
{ Fixture fixture;
  char bytes[256];
  av::StateWriter unstarted(bytes, sizeof bytes);
  fixture.worker().report(unstarted);
  CHECK(std::strcmp(unstarted.dump(), "worker/0 running=false pending=0 stopping=false") == 0);
  CHECK_STATUS(fixture.worker().start(), Status::ok);
  av::StateWriter started(bytes, sizeof bytes);
  fixture.worker().report(started);
  CHECK(std::strcmp(started.dump(), "worker/0 running=true pending=0 stopping=false") == 0);
  void *gate = nullptr;
  CHECK_STATUS(fixture.host().create_event(gate), Status::ok);
  Entry blocker = fixture.entry(0u, gate);
  CHECK_STATUS(fixture.worker().post(WorkItem(&work, nullptr, &blocker)), Status::ok);
  while (fixture.worker().pending() != 0u) sched_yield();
  Entry entries[Worker::queue_capacity] = {};
  for (std::uint32_t index = 0u; index < Worker::queue_capacity; index++) {
      entries[index] = fixture.entry(index + 1u);
      CHECK_STATUS(fixture.worker().post(WorkItem(&work, nullptr, &entries[index])), Status::ok);
  }
  fixture.host().hold_mutex_elsewhere(av::WorkerProbe::lock(fixture.worker()), true);
  av::StateWriter busy(bytes, sizeof bytes);
  fixture.worker().report(busy);
  CHECK(std::strcmp(busy.dump(), "worker/0 running=true busy=true") == 0);
  fixture.host().hold_mutex_elsewhere(av::WorkerProbe::lock(fixture.worker()), false);
  av::StateWriter full(bytes, sizeof bytes);
  fixture.worker().report(full);
  CHECK(std::strcmp(full.dump(), "worker/0 running=true pending=64 stopping=false") == 0);
  fixture.host().signal_event(gate);
  while (fixture.journal().ran.load() != Worker::queue_capacity + 1u) sched_yield();
  CHECK(fixture.worker().invariants_hold());
  fixture.worker().stop();
  fixture.host().destroy_event(gate); }

TEST(worker_post, a_full_queue_refuses_the_next_item)
{ Fixture fixture;
  void *gate = nullptr;
  CHECK_STATUS(fixture.host().create_event(gate), Status::ok);
  CHECK_STATUS(fixture.worker().start(), Status::ok);
  Entry blocker = fixture.entry(0u, gate);
  CHECK_STATUS(fixture.worker().post(WorkItem(&work, &completion, &blocker)), Status::ok);
  while (fixture.worker().pending() != 0u) sched_yield();
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

void release_once_stopping(void *context)
{
    auto *const releaser = static_cast<Releaser *>(context);
    while (!releaser->worker->stopping()) sched_yield();
    releaser->host->signal_event(releaser->gate);
}

}

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
  CHECK_STATUS(fixture.worker().start(), Status::ok);
  fixture.host().destroy_event(gate); }

TEST(worker_start, a_second_start_is_refused_and_each_host_failure_releases_everything)
{ Fixture fixture;
  CHECK_STATUS(fixture.worker().start(), Status::ok);
  CHECK_STATUS(fixture.worker().start(), Status::invalid_state);
  fixture.worker().stop();
  fixture.worker().stop();
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
  work(&entry);
  CHECK(fixture.host().claim(HostViolation::worker_required)); }

TEST(worker_connect_doorbell, a_store_wakes_the_worker_to_serve_its_ring)
{ Fixture fixture;
  CHECK_STATUS(fixture.worker().connect_doorbell(3u), Status::invalid_state);
  CHECK_STATUS(fixture.worker().start(), Status::ok);
  CHECK_STATUS(fixture.ring().prepare(fixture.window()), Status::ok);
  CHECK_STATUS(fixture.worker().connect_doorbell(3u), Status::ok);
  CHECK_STATUS(fixture.worker().connect_doorbell(4u), Status::invalid_state);
  CHECK_STATUS(fixture.worker().post(WorkItem(&attach, nullptr, &fixture.ring())), Status::ok);
  for (std::uint64_t fence = 1u; fence <= 3u; fence++) {
      fixture.guest().write_batch(fence, 20u);
      fixture.guest().publish();
      CHECK(fixture.host().ring_doorbell(3u));
      CHECK(fixture.published(fixture.guest().tail()));
  }
  CHECK(!fixture.host().ring_doorbell(4u));
  fixture.worker().stop();
  CHECK_EQ(fixture.host().live_doorbells(), 0u);
  CHECK_EQ(fixture.ring().batches(), 3u);
  CHECK(fixture.worker().invariants_hold()); }

TEST(worker_run, a_doorbell_that_raced_the_attachment_is_not_lost)
{ Fixture fixture;
  CHECK_STATUS(fixture.worker().start(), Status::ok);
  CHECK_STATUS(fixture.ring().prepare(fixture.window()), Status::ok);
  CHECK_STATUS(fixture.worker().connect_doorbell(0u), Status::ok);
  fixture.guest().write_batch(1u, 0u);
  fixture.guest().publish();
  CHECK(fixture.host().ring_doorbell(0u));
  CHECK_STATUS(fixture.worker().post(WorkItem(&attach, nullptr, &fixture.ring())), Status::ok);
  CHECK(fixture.published(fixture.guest().tail()));
  CHECK_EQ(fixture.guest().tail(), 32u);
  CHECK_EQ(fixture.host().run_main_loop(), 0u); }

TEST(worker_disconnect_doorbell, a_disconnected_page_rings_nothing_and_can_be_connected_again)
{ Fixture fixture;
  fixture.worker().disconnect_doorbell();
  CHECK_STATUS(fixture.worker().start(), Status::ok);
  CHECK_STATUS(fixture.worker().connect_doorbell(2u), Status::ok);
  fixture.worker().disconnect_doorbell();
  CHECK(!fixture.host().ring_doorbell(2u));
  CHECK(fixture.worker().invariants_hold());
  CHECK_STATUS(fixture.worker().connect_doorbell(5u), Status::ok);
  CHECK(fixture.host().ring_doorbell(5u)); }

TEST(worker_connect_doorbell, a_page_past_the_region_is_refused)
{ Fixture fixture;
  CHECK_STATUS(fixture.worker().start(), Status::ok);
  CHECK_STATUS(fixture.worker().connect_doorbell(av::Doorbell::pages), Status::invalid_parameter);
  CHECK_EQ(fixture.host().live_doorbells(), 0u);
  fixture.worker().stop();
  CHECK_STATUS(fixture.worker().start(), Status::ok);
  CHECK_STATUS(fixture.worker().connect_doorbell(1u), Status::ok);
  fixture.worker().stop(); }

TEST(worker_invariants, a_queue_past_capacity_or_a_head_outside_it_is_detected)
{ MockHost host;
  av::FenceInterrupts interrupts(host);
  av::WireRecorder recorder(host);
  av::FenceReporter fences(host, interrupts, recorder, 1u);
  av::MockContextExecution execution(host, fences, 12u);
  RingConsumer ring(host, fences, execution.batches());
  Worker overfilled(host, ring);
  av::WorkerProbe::overfill(overfilled);
  CHECK(!overfilled.invariants_hold());
  Worker misplaced(host, ring);
  av::WorkerProbe::misplace_head(misplaced);
  CHECK(!misplaced.invariants_hold());
  int event = 0;
  Worker stopped(host, ring);
  av::DoorbellProbe::forge(av::WorkerProbe::doorbell(stopped), &event);
  CHECK(!stopped.invariants_hold());
  av::DoorbellProbe::forget_event(av::WorkerProbe::doorbell(stopped));
  CHECK(!stopped.invariants_hold());
  av::DoorbellProbe::repair(av::WorkerProbe::doorbell(stopped)); }

namespace {

std::atomic<std::uint32_t> polls{0u};

std::uint32_t pending_for_a_while(void *, AvExecutor *) { return polls++ < 5u ? AV_POLL_PENDING : AV_POLL_IDLE; }

struct Watching {
    Fixture *fixture;
    bool     kept_working;
};

}

TEST(worker_run, a_worker_watches_a_gpu_that_still_has_work_before_it_gives_up_spinning)
{ Fixture fixture;
  fixture.ops().editable().poll = pending_for_a_while;
  polls = 0u;
  CHECK_STATUS(fixture.ring().prepare(fixture.window()), Status::ok);
  fixture.host().advance_clock_per_read(1000u);
  Watching watching{&fixture, true};
  void    *thread = nullptr;
  CHECK_STATUS(fixture.host().start_thread([](void *context) {
                   auto *const w = static_cast<Watching *>(context);
                   w->fixture->ring().attach();
                   w->kept_working = av::WorkerProbe::spin(w->fixture->worker());
               },
                                           &watching, thread),
               Status::ok);
  fixture.host().join_thread(thread);
  CHECK(!watching.kept_working);
  CHECK(polls.load() > 5u); }

namespace {

struct Racing {
    Fixture *fixture;
};

void publish_a_batch(void *context)
{
    Fixture &fixture = *static_cast<Racing *>(context)->fixture;
    fixture.guest().write_batch(1u, 0u);
    fixture.guest().publish();
}

void attach_and_expect_the_race(void *context)
{
    Racing &racing = *static_cast<Racing *>(context);
    racing.fixture->ring().attach();
    racing.fixture->host().on_clock_reaching(racing.fixture->host().peek_clock() + 100000u, &publish_a_batch, &racing);
}

}

TEST(worker_run, a_batch_published_as_the_worker_gives_up_spinning_is_served_without_a_sleep)
{ Fixture fixture;
  fixture.host().advance_clock_per_read(1000u);
  CHECK_STATUS(fixture.worker().start(), Status::ok);
  CHECK_STATUS(fixture.ring().prepare(fixture.window()), Status::ok);
  Racing racing{&fixture};
  CHECK_STATUS(fixture.worker().post(WorkItem(&attach_and_expect_the_race, nullptr, &racing)), Status::ok);
  for (std::uint32_t spins = 0u; spins < 50000000u && fixture.guest().tail() != 32u; spins++) sched_yield();
  CHECK_EQ(fixture.guest().tail(), 32u);
  CHECK(fixture.published(32u));
  fixture.worker().stop();
  CHECK(fixture.worker().invariants_hold()); }

namespace {

void hung_progress(void *, AvExecutor *, std::uint64_t *marked, std::uint64_t *reached)
{
    *marked = 4u;
    *reached = 0u;
}

}

TEST(worker_run, an_idle_worker_with_a_hung_submission_wakes_at_the_watchdog_deadline_and_loses_the_context)
{ Fixture fixture;
  fixture.ops().editable().gpu_progress = hung_progress;
  fixture.host().set_clock(1000u);
  CHECK_STATUS(fixture.ring().prepare(fixture.window()), Status::ok);
  CHECK_STATUS(fixture.worker().start(), Status::ok);
  CHECK_STATUS(fixture.worker().post(WorkItem(&attach, nullptr, &fixture.ring())), Status::ok);
  fixture.guest().write_batch(1u, 8u);
  fixture.guest().publish();
  for (std::uint32_t spins = 0u; spins < 50000000u && fixture.ring().watchdog_deadline() == 0u; spins++) sched_yield();
  CHECK_EQ(fixture.ring().watchdog_deadline(), 1000u + av::SubmissionWatchdog::default_limit_ns);
  CHECK(!fixture.ring().faulted());
  fixture.host().set_clock(1000u + av::SubmissionWatchdog::default_limit_ns + 1u);
  for (std::uint32_t spins = 0u; spins < 50000000u && !fixture.ring().faulted(); spins++) sched_yield();
  CHECK(fixture.ring().faulted());
  fixture.worker().stop(); }
