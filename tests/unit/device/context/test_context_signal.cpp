/* The landing pad for a completion that arrives on a thread the device never
 * gave the host: everything that thread is allowed to do, and what the context's
 * worker does with it afterwards. */
#include "core/support/status.hpp"
#include "device/context/context_signal.hpp"
#include "device/fence/fence_interrupts.hpp"
#include "device/fence/fence_reporter.hpp"
#include "device/recording/wire_recorder.hpp"
#include "device/fence/fence_window.hpp"
#include "device/host/host.hpp"
#include "device/memory/blob_window.hpp"
#include "device/context/ring_consumer.hpp"
#include "device/context/ring_window.hpp"
#include "device/threading/work_item.hpp"
#include "device/threading/worker.hpp"
#include "harness/executor/mock_context_execution.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/host/host_violation.hpp"
#include "harness/host/mock_host.hpp"
#include "harness/context/guest_ring.hpp"
#include "proto/submission/fence_page.hpp"

#include <cstdint>
#include <cstring>
#include <sched.h>

using av::BlobWindow;
using av::ContextSignal;
using av::FenceInterrupts;
using av::FencePage;
using av::FenceReporter;
using av::FenceWindow;
using av::GuestRing;
using av::HostViolation;
using av::MockHost;
using av::RingConsumer;
using av::RingWindow;
using av::Status;
using av::WorkItem;
using av::Worker;

namespace av {

struct ContextSignalProbe {
    static void deliver_what_was_never_raised(ContextSignal &signal) { signal.delivered_ = signal.raised_ + 1u; }
    static void pend_without_raising(ContextSignal &signal) { signal.pending_ = true; }
};

}  // namespace av

namespace {

constexpr std::uint32_t line = 5u;
constexpr std::uint8_t  ring_order = 12u;
constexpr std::uint32_t patience = 50000000u;

void attach(void *consumer) { static_cast<RingConsumer *>(consumer)->attach(); }
void raise_from_host(void *signal) { static_cast<ContextSignal *>(signal)->raise(); }
void drain_as_worker(void *signal) { static_cast<ContextSignal *>(signal)->drain(); }

class Fixture {
public:
    Fixture()
        : page_{}, page_pointer_(page_), page_length_(sizeof page_), ring_bytes_{}, ring_pointer_(ring_bytes_),
          ring_length_(sizeof ring_bytes_), guest_(ring_bytes_, ring_order), interrupts_(host_),
          recorder_(host_), fences_(host_, interrupts_, recorder_, 1u),
          execution_(host_, fences_, ring_order),
          ring_(host_, fences_, execution_.batches()), worker_(host_, ring_), signal_(host_, worker_, fences_)
    {
        std::memset(page_, 0xEE, sizeof page_);
        static_cast<void>(interrupts_.initialize());
        static_cast<void>(interrupts_.connect());
    }
    ~Fixture() { worker_.stop(); }

    MockHost      &host() { return host_; }
    ContextSignal &signal() { return signal_; }
    Worker        &worker() { return worker_; }
    RingConsumer  &ring() { return ring_; }

    void   prepare_page()
    {
        CHECK_STATUS(fences_.prepare(FenceWindow(BlobWindow(&page_pointer_, &page_length_, 1u, page_length_)), line),
                     Status::ok);
    }
    Status prepare_ring()
    {
        return ring_.prepare(RingWindow(BlobWindow(&ring_pointer_, &ring_length_, 1u, ring_length_), ring_order));
    }

    FencePage page()
    {
        FencePage decoded;
        static_cast<void>(decoded.decode(page_, FencePage::encoded_size));
        return decoded;
    }

    /* Whether the page reached `progress` before patience ran out. */
    bool progressed(std::uint64_t progress)
    {
        for (std::uint32_t spins = 0u; spins < patience && page().progress() != progress; spins++) sched_yield();
        return page().progress() == progress;
    }
    bool interrupted(std::uint32_t interrupts)
    {
        for (std::uint32_t spins = 0u; spins < patience && host_.raised(line) != interrupts; spins++) sched_yield();
        return host_.raised(line) == interrupts;
    }

    void on_another_thread(av::HostRoutine routine, void *context)
    {
        void *thread = nullptr;
        CHECK_STATUS(host_.start_thread(routine, context, thread), Status::ok);
        host_.join_thread(thread);
    }

private:
    std::uint8_t             page_[64];
    void                    *page_pointer_;
    std::uint64_t            page_length_;
    std::uint8_t             ring_bytes_[32u + 4096u];
    void                    *ring_pointer_;
    std::uint64_t            ring_length_;
    MockHost                 host_;
    GuestRing                guest_;
    FenceInterrupts          interrupts_;
    av::WireRecorder         recorder_;
    FenceReporter            fences_;
    av::MockContextExecution execution_;
    RingConsumer             ring_;
    Worker                   worker_;
    ContextSignal            signal_;
};

}  // namespace

/* The whole budget of a thread the device does not own is a flag and a wake.
 * The fence page, the handle registry and the decode arena each open with
 * require_worker, so if raise() reached any of them this call - made from the
 * main loop, which is not the worker - would record a violation. */
TEST(context_signal_raise, a_thread_that_is_not_the_worker_touches_nothing_the_worker_owns)
{ Fixture fixture;
  fixture.prepare_page();
  fixture.signal().raise();
  fixture.signal().raise();
  CHECK_EQ(fixture.host().unclaimed_violations(), 0u);
  CHECK(fixture.signal().pending());
  CHECK_EQ(fixture.signal().raised(), 2u);
  CHECK_EQ(fixture.signal().delivered(), 0u);
  /* Nothing the guest reads has moved: the page is as prepare left it. */
  CHECK_EQ(fixture.page().progress(), 0u);
  CHECK_EQ(fixture.page().completed_fence(), 0u);
  CHECK(!fixture.page().faulted());
  CHECK_EQ(fixture.host().raised(line), 0u);
  CHECK(fixture.signal().invariants_hold()); }

TEST(context_signal_drain, the_worker_nudges_once_for_everything_raised_since)
{ Fixture fixture;
  fixture.prepare_page();
  fixture.signal().raise();
  fixture.signal().raise();
  fixture.signal().raise();
  fixture.on_another_thread(&drain_as_worker, &fixture.signal());
  CHECK_EQ(fixture.page().progress(), 1u);
  CHECK_EQ(fixture.page().completed_fence(), 0u);
  CHECK_EQ(fixture.signal().delivered(), 1u);
  CHECK_EQ(fixture.signal().raised(), 3u);
  CHECK(!fixture.signal().pending());
  /* A drain with nothing raised since writes nothing at all. */
  fixture.on_another_thread(&drain_as_worker, &fixture.signal());
  CHECK_EQ(fixture.page().progress(), 1u);
  CHECK_EQ(fixture.signal().delivered(), 1u);
  fixture.signal().raise();
  fixture.on_another_thread(&drain_as_worker, &fixture.signal());
  CHECK_EQ(fixture.page().progress(), 2u);
  CHECK_EQ(fixture.page().completed_fence(), 0u);
  CHECK_EQ(fixture.signal().delivered(), 2u);
  CHECK(fixture.signal().invariants_hold()); }

/* End to end, on the worker the device really runs: a host thread raises, the
 * worker wakes, the page moves with nothing claimed completed, and the guest's
 * line is raised once. */
TEST(context_signal, a_raise_from_another_thread_wakes_the_worker_and_interrupts_the_guest)
{ Fixture fixture;
  fixture.prepare_page();
  CHECK_STATUS(fixture.prepare_ring(), Status::ok);
  fixture.worker().connect_signal(fixture.signal());
  CHECK_STATUS(fixture.worker().start(), Status::ok);
  CHECK_STATUS(fixture.worker().post(WorkItem(&attach, nullptr, &fixture.ring())), Status::ok);
  fixture.on_another_thread(&raise_from_host, &fixture.signal());
  CHECK(fixture.progressed(1u));
  CHECK(fixture.interrupted(1u));
  CHECK_EQ(fixture.page().completed_fence(), 0u);
  fixture.on_another_thread(&raise_from_host, &fixture.signal());
  CHECK(fixture.progressed(2u));
  CHECK(fixture.interrupted(2u));
  fixture.worker().stop();
  CHECK_EQ(fixture.signal().delivered(), 2u);
  CHECK_EQ(fixture.host().unclaimed_violations(), 0u);
  CHECK(fixture.signal().invariants_hold()); }

/* A wake has nowhere to go until the worker runs, and the flag is what keeps
 * the completion from being lost. */
TEST(context_signal, a_raise_a_stopped_worker_never_saw_is_delivered_when_it_next_wakes)
{ Fixture fixture;
  fixture.prepare_page();
  CHECK_STATUS(fixture.prepare_ring(), Status::ok);
  fixture.worker().connect_signal(fixture.signal());
  fixture.worker().wake();
  fixture.signal().raise();
  CHECK(fixture.signal().pending());
  CHECK_EQ(fixture.page().progress(), 0u);
  CHECK_STATUS(fixture.worker().start(), Status::ok);
  CHECK_STATUS(fixture.worker().post(WorkItem(&attach, nullptr, &fixture.ring())), Status::ok);
  CHECK(fixture.progressed(1u));
  CHECK(fixture.interrupted(1u));
  fixture.worker().stop();
  CHECK_EQ(fixture.signal().delivered(), 1u);
  CHECK_EQ(fixture.signal().raised(), 1u); }

TEST(context_signal, the_drain_is_the_workers_and_the_landing_pad_is_attached_on_the_main_loop)
{ Fixture fixture;
  fixture.prepare_page();
  fixture.signal().raise();
  fixture.signal().drain();
  CHECK(fixture.host().claim(HostViolation::worker_required));   /* ContextSignal::drain */
  CHECK(fixture.host().claim(HostViolation::worker_required));   /* FenceReporter::nudge */
  struct Attachment {
      Worker        *worker;
      ContextSignal *signal;
  } attachment{&fixture.worker(), &fixture.signal()};
  fixture.on_another_thread(
      [](void *context) {
          auto *const pair = static_cast<Attachment *>(context);
          pair->worker->connect_signal(*pair->signal);
      },
      &attachment);
  CHECK(fixture.host().claim(HostViolation::main_loop_required));
  CHECK_EQ(fixture.host().unclaimed_violations(), 0u); }

TEST(context_signal_invariants, a_delivery_nothing_raised_or_a_flag_nothing_set_is_detected)
{ Fixture delivered;
  av::ContextSignalProbe::deliver_what_was_never_raised(delivered.signal());
  CHECK(!delivered.signal().invariants_hold());
  Fixture pending;
  av::ContextSignalProbe::pend_without_raising(pending.signal());
  CHECK(!pending.signal().invariants_hold()); }
