#include "core/support/status.hpp"
#include "device/context/d3d_ordering.hpp"
#include "device/fence/device_pulse.hpp"
#include "device/fence/fence_interrupts.hpp"
#include "device/fence/fence_reporter.hpp"
#include "device/fence/fence_window.hpp"
#include "device/memory/blob_window.hpp"
#include "device/context/held_completions.hpp"
#include "device/context/ring_consumer.hpp"
#include "device/recording/wire_recorder.hpp"
#include "device/context/ring_window.hpp"
#include "device/threading/worker.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/host/host_violation.hpp"
#include "harness/executor/mock_context_execution.hpp"
#include "harness/host/mock_host.hpp"
#include "harness/context/guest_ring.hpp"
#include "proto/submission/fault_code.hpp"
#include "proto/submission/fence_page.hpp"
#include "proto/submission/ring_header.hpp"
#include "proto/submission/ring_progress.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>
#include <thread>
#include <unistd.h>

#include <sched.h>
#include <unistd.h>

using av::BlobWindow;
using av::FenceInterrupts;
using av::FaultCode;
using av::FencePage;
using av::FenceReporter;
using av::FenceWindow;
using av::GuestRing;
using av::HostViolation;
using av::MockHost;
using av::RingConsumer;
using av::RingHeader;
using av::RingProgress;
using av::RingWindow;
using av::Status;

namespace av {

struct RingProgressProbe {
    static void forget_order(RingProgress &progress) { progress.order_ = 0u; }
};

struct RingConsumerProbe {
    static void replace_ring(RingConsumer &consumer, const RingWindow &ring) { consumer.ring_ = ring; }
    static void overcount(RingConsumer &consumer) { consumer.batches_ = consumer.head_ + 1u; }
    static void pass_tail(RingConsumer &consumer) { consumer.head_ = consumer.progress_.tail() + 16u; }
    static void attach_unprepared(RingConsumer &consumer) { consumer.attached_ = true; consumer.prepared_ = false; }
    static void move_unprepared(RingConsumer &consumer) { consumer.head_ = 8u; }
    static void break_progress(RingConsumer &consumer) { RingProgressProbe::forget_order(consumer.progress_); }
    static void fault_again(RingConsumer &consumer) { consumer.fault(FaultCode::device_lost, "again", 1u); }
    static bool settle(RingConsumer &consumer) { return consumer.settle(); }
    static void lose_fence(RingConsumer &consumer) { consumer.batches_ = 1u; consumer.head_ = 32u; consumer.last_fence_ = 0u; }
};



}

namespace {

constexpr std::uint64_t capacity = 4096u;
constexpr std::uint64_t head_offset = GuestRing::head_offset;

class RingMemory {
public:
    RingMemory() : bytes_{}, pointer_(bytes_), length_(sizeof bytes_), guest_(bytes_, 12u) {}
    RingWindow window() const { return RingWindow(BlobWindow(&pointer_, &length_, 1u, length_), 12u); }
    GuestRing &guest() { return guest_; }

private:
    std::uint8_t  bytes_[32u + capacity];
    void         *pointer_;
    std::uint64_t length_;
    GuestRing     guest_;
};

class Fixture {
public:
    Fixture()
        : page_{}, page_pointer_(page_), page_length_(sizeof page_), interrupts_(host_), recorder_(host_), fences_(host_, interrupts_, recorder_, 1u),
          execution_(host_, fences_, 12u), consumer_(host_, fences_, execution_.batches()), worker_(host_, consumer_)
    {
        static_cast<void>(interrupts_.initialize());
        static_cast<void>(interrupts_.connect());
        fences_.beat_by(pulse_);
    }

    av::DevicePulse &pulse() { return pulse_; }

    MockHost     &host() { return host_; }
    RingConsumer &consumer() { return consumer_; }
    FenceReporter &fences() { return fences_; }
    av::Worker   &worker() { return worker_; }
    RingWindow    window() const { return memory_.window(); }
    GuestRing    &ring() { return memory_.guest(); }

    ~Fixture()
    {
        worker_.stop();
        on_worker([](RingConsumer &consumer) { consumer.detach(); });
    }

    av::MockExecutorOps &ops() { return execution_.ops(); }

    FencePage page() const
    {
        FencePage decoded;
        static_cast<void>(decoded.decode(page_, sizeof page_));
        return decoded;
    }

    void bind()
    {
        CHECK_STATUS(fences_.prepare(FenceWindow(BlobWindow(&page_pointer_, &page_length_, 1u, page_length_)), 0u),
                     Status::ok);
        CHECK_STATUS(consumer_.prepare(memory_.window()), Status::ok);
        CHECK(consumer_.invariants_hold());
        on_worker([](RingConsumer &consumer) { consumer.attach(); });
        CHECK(consumer_.attached());
    }
    bool poll()
    {
        bool publishing = true;
        on_worker([&publishing](RingConsumer &consumer) { publishing = consumer.poll(); });
        CHECK(consumer_.invariants_hold());
        return publishing;
    }
    void serve()
    {
        on_worker([](RingConsumer &consumer) { consumer.service(); });
        CHECK(consumer_.invariants_hold());
    }
    void detach()
    {
        on_worker([](RingConsumer &consumer) { consumer.detach(); });
    }

private:
    template <typename Body>
    void on_worker(Body body)
    {
        struct Call {
            RingConsumer *consumer;
            Body         *body;
        } call{&consumer_, &body};
        void *thread = nullptr;
        CHECK_STATUS(host_.start_thread([](void *context) {
                         auto *const c = static_cast<Call *>(context);
                         (*c->body)(*c->consumer);
                     },
                                        &call, thread),
                     Status::ok);
        host_.join_thread(thread);
    }

    std::uint8_t    page_[FencePage::encoded_size];
    void           *page_pointer_;
    std::uint64_t   page_length_;
    MockHost        host_;
    FenceInterrupts interrupts_;
    av::WireRecorder recorder_;
    av::DevicePulse pulse_{host_};
    FenceReporter   fences_;
    av::MockContextExecution execution_;
    RingConsumer    consumer_;
    av::Worker      worker_;
    RingMemory      memory_;
};

}

TEST(ring_consumer_service, consumes_every_complete_batch_and_publishes_head)
{ Fixture fixture;
  fixture.bind();
  fixture.ring().write_batch(1u, 0u);
  fixture.ring().write_batch(2u, 13u);
  fixture.ring().write_batch(9u, 64u);
  fixture.ring().publish();
  fixture.serve();
  CHECK(!fixture.consumer().faulted());
  CHECK_EQ(fixture.consumer().batches(), 3u);
  CHECK_EQ(fixture.consumer().last_fence(), 9u);
  CHECK_EQ(fixture.consumer().head(), 32u + 48u + 96u);
  CHECK_EQ(fixture.ring().head(), fixture.ring().tail());
  CHECK_EQ(fixture.page().completed_fence(), 9u);
  CHECK_EQ(fixture.page().progress(), 4u);
  CHECK_EQ(fixture.host().raised(0u), 1u);
  fixture.serve();
  CHECK_EQ(fixture.consumer().batches(), 3u);
  CHECK_EQ(fixture.host().raised(0u), 1u);
  CHECK_EQ(fixture.host().logged(), 0u); }

TEST(ring_consumer_service, stops_at_a_batch_the_guest_has_not_finished)
{ Fixture fixture;
  fixture.bind();
  fixture.ring().write_batch(1u, 8u);
  fixture.ring().write_batch(2u, 100u);
  fixture.ring().publish(40u + 15u);
  fixture.serve();
  CHECK_EQ(fixture.consumer().batches(), 1u);
  CHECK_EQ(fixture.ring().head(), 40u);
  fixture.ring().publish(fixture.ring().tail() - 1u);
  fixture.serve();
  CHECK_EQ(fixture.consumer().head(), 40u);
  fixture.ring().publish();
  fixture.serve();
  CHECK_EQ(fixture.consumer().batches(), 2u);
  CHECK_EQ(fixture.ring().head(), fixture.ring().tail());
  CHECK(!fixture.consumer().faulted());
  CHECK_EQ(fixture.page().completed_fence(), 2u);
  CHECK_EQ(fixture.host().raised(0u), 2u); }

TEST(ring_consumer_service, reads_batches_across_the_end_of_the_data_area)
{ Fixture fixture;
  fixture.bind();
  std::uint64_t fence = 1u;
  for (std::uint32_t round = 0u; round < 5u; round++) {
      for (std::uint32_t i = 0u; i < 7u; i++) fixture.ring().write_batch(fence++, 8u + round * 37u + i * 11u);
      fixture.ring().publish();
      fixture.serve();
      CHECK_EQ(fixture.ring().head(), fixture.ring().tail());
  }
  CHECK(fixture.ring().tail() > capacity);
  CHECK_EQ(fixture.consumer().batches(), 35u);
  CHECK(!fixture.consumer().faulted()); }

TEST(ring_consumer_service, a_full_ring_is_consumed_whole)
{ Fixture fixture;
  fixture.bind();
  fixture.ring().write_batch(1u, 2000u);
  fixture.ring().write_batch(2u, capacity - 2032u - 32u);
  CHECK_EQ(fixture.ring().tail(), capacity);
  fixture.ring().publish();
  fixture.serve();
  CHECK_EQ(fixture.consumer().head(), capacity);
  fixture.ring().write_batch(3u, capacity - 32u);
  fixture.ring().publish();
  fixture.serve();
  CHECK_EQ(fixture.consumer().head(), 2u * capacity);
  CHECK(!fixture.consumer().faulted()); }

TEST(ring_consumer_service, executes_each_batch_before_it_completes)
{ Fixture fixture;
  fixture.bind();
  fixture.ring().write_batch(1u, 40u);
  fixture.ring().write_batch(2u, 0u);
  fixture.ring().publish();
  fixture.serve();
  CHECK_EQ(fixture.ops().created(), 1u);
  CHECK_EQ(fixture.ops().executed(), 1u);
  CHECK_EQ(fixture.ops().id_at(0u), GuestRing::filler_command);
  CHECK_EQ(fixture.ops().size_at(0u), 32u);
  CHECK_EQ(fixture.page().completed_fence(), 2u); }

TEST(ring_consumer_service, a_batch_that_cannot_run_faults_the_context_before_it_completes)
{ struct Case {
      const char   *name;
      std::uint32_t length;
      std::uint32_t command;
      FaultCode     code;
  };
  const Case cases[] = {
      {"framing no producer writes", 5u, 0u, FaultCode::command_framing},
      {"undecodable", 8u, av::MockExecutorOps::undecodable_id, FaultCode::undecodable},
      {"device lost", 8u, av::MockExecutorOps::lost_id, FaultCode::device_lost},
  };
  for (const Case &c : cases) {
      Fixture fixture;
      fixture.bind();
      fixture.ring().write_batch(1u, 8u);
      fixture.ring().publish();
      fixture.serve();
      fixture.ring().write_batch(2u, c.length);
      if (c.command != 0u) std::memcpy(&fixture.ring().data(40u + 32u), &c.command, 4u);
      fixture.ring().publish();
      fixture.serve();
      if (fixture.page().fault_code() != static_cast<std::uint32_t>(c.code))
          av::TestRegistry::instance().record_failure(__FILE__, __LINE__, c.name);
      CHECK(fixture.consumer().faulted());
      CHECK(!fixture.fences().working());
      CHECK_EQ(fixture.page().completed_fence(), 1u);
      CHECK_EQ(fixture.ops().live(), 0u);
      CHECK_EQ(fixture.consumer().head(), 40u);
      CHECK_EQ(fixture.host().logged(), 1u);
  } }

TEST(ring_consumer_attach, a_context_whose_executor_cannot_be_created_faults)
{ Fixture fixture;
  fixture.ops().fail_create_at(1u);
  fixture.ring().write_batch(1u, 8u);
  fixture.ring().publish();
  fixture.bind();
  CHECK(fixture.consumer().faulted());
  CHECK(fixture.page().faulted());
  CHECK_EQ(fixture.page().fault_code(), static_cast<std::uint32_t>(FaultCode::no_executor));
  CHECK_EQ(fixture.host().raised(0u), 1u);
  fixture.serve();
  CHECK_EQ(fixture.consumer().head(), 0u);
  CHECK_EQ(fixture.host().logged(), 1u); }

TEST(ring_consumer_service, the_head_the_guest_writes_is_ignored)
{ Fixture fixture;
  fixture.bind();
  fixture.ring().write_batch(1u, 0u);
  fixture.ring().publish();
  fixture.ring().store(head_offset, 0xDEADu);
  fixture.serve();
  CHECK_EQ(fixture.ring().head(), 32u);
  fixture.ring().store(head_offset, 0u);
  fixture.ring().write_batch(2u, 0u);
  fixture.ring().publish();
  fixture.serve();
  CHECK_EQ(fixture.ring().head(), 64u);
  CHECK(!fixture.consumer().faulted()); }

TEST(ring_consumer_service, a_hostile_ring_faults_the_context_logs_once_and_ignores_later_doorbells)
{ struct Case {
      const char *name;
      void      (*spoil)(GuestRing &ring);
      FaultCode   code;
  };
  const Case cases[] = {
      {"header magic", [](GuestRing &ring) { ring.bytes()[0] = 0u; }, FaultCode::ring_header},
      {"order changed", [](GuestRing &ring) { ring.bytes()[4] = 13u; }, FaultCode::ring_header},
      {"tail backwards", [](GuestRing &ring) { ring.publish(8u); }, FaultCode::ring_tail},
      {"tail past a whole ring", [](GuestRing &ring) { ring.publish(32u + capacity + 1u); }, FaultCode::ring_tail},
      {"tail far away", [](GuestRing &ring) { ring.publish(~std::uint64_t{0}); }, FaultCode::ring_tail},
      {"batch magic", [](GuestRing &ring) { ring.write_batch(6u, 0u); ring.data(32u) = 0u; ring.publish(); },
       FaultCode::batch_header},
      {"fence zero", [](GuestRing &ring) { ring.write_batch(6u, 0u); ring.data(32u + 8u) = 0u; ring.publish(); },
       FaultCode::batch_header},
      {"fence repeated", [](GuestRing &ring) { ring.write_batch(5u, 0u); ring.publish(); }, FaultCode::batch_order},
      {"fence lower", [](GuestRing &ring) { ring.write_batch(4u, 0u); ring.publish(); }, FaultCode::batch_order},
      {"batch longer than the ring", [](GuestRing &ring) { ring.write_batch(6u, 0u); ring.data(32u + 6u) = 1u;
                                                           ring.publish(32u + capacity); }, FaultCode::batch_header},
  };
  for (const Case &c : cases) {
      Fixture fixture;
      fixture.bind();
      fixture.ring().write_batch(5u, 0u);
      fixture.ring().publish();
      fixture.serve();
      CHECK_EQ(fixture.consumer().head(), 32u);
      c.spoil(fixture.ring());
      fixture.serve();
      if (!fixture.consumer().faulted() || fixture.page().fault_code() != static_cast<std::uint32_t>(c.code))
          av::TestRegistry::instance().record_failure(__FILE__, __LINE__, c.name);
      CHECK(fixture.page().faulted());
      CHECK_EQ(fixture.host().logged(), 1u);
      CHECK_EQ(fixture.consumer().head(), 32u);
      CHECK_EQ(fixture.page().completed_fence(), 5u);
      CHECK_EQ(fixture.page().progress(), 1u);
      CHECK_EQ(fixture.host().raised(0u), 2u);
      CHECK_EQ(fixture.ops().live(), 0u);
      static_cast<void>(RingHeader(12u, 0u, 0u).encode(fixture.ring().bytes(), 32u));
      fixture.ring().store(head_offset, 32u);
      fixture.serve();
      CHECK_EQ(fixture.host().logged(), 1u);
      CHECK_EQ(fixture.consumer().head(), 32u);
      CHECK_EQ(fixture.host().raised(0u), 2u);
  } }

TEST(ring_consumer_service, a_ring_the_blob_cannot_hold_faults_without_reading_outside_it)
{ Fixture unreadable_header;
  unreadable_header.bind();
  std::uint8_t  short_bytes[8] = {};
  void         *short_pointer = short_bytes;
  std::uint64_t short_length = sizeof short_bytes;
  av::RingConsumerProbe::replace_ring(unreadable_header.consumer(),
                                      RingWindow(BlobWindow(&short_pointer, &short_length, 1u, 8u), 12u));
  unreadable_header.serve();
  CHECK(unreadable_header.consumer().faulted());
  CHECK_EQ(unreadable_header.page().fault_code(), static_cast<std::uint32_t>(FaultCode::ring_header));
  Fixture unreadable_data;
  unreadable_data.bind();
  unreadable_data.ring().write_batch(1u, 0u);
  unreadable_data.ring().write_batch(2u, 0u);
  unreadable_data.ring().publish();
  void         *pointer = unreadable_data.ring().bytes();
  std::uint64_t length = 32u + 32u + 20u;
  av::RingConsumerProbe::replace_ring(unreadable_data.consumer(), RingWindow(BlobWindow(&pointer, &length, 1u, length), 12u));
  unreadable_data.serve();
  CHECK(unreadable_data.consumer().faulted());
  CHECK_EQ(unreadable_data.page().fault_code(), static_cast<std::uint32_t>(FaultCode::batch_header));
  CHECK_EQ(unreadable_data.consumer().batches(), 1u);
  CHECK_EQ(unreadable_data.host().logged(), 1u); }

TEST(ring_consumer_prepare, accepts_a_fresh_header_of_the_setup_order_only)
{ struct Case {
      const char   *name;
      std::uint8_t  order;
      std::uint64_t tail;
      std::uint64_t head;
      Status        status;
  };
  const Case cases[] = {
      {"fresh", 12u, 0u, 0u, Status::ok},
      {"written before the setup", 12u, 64u, 0u, Status::ok},
      {"a whole ring written", 12u, capacity, 0u, Status::ok},
      {"more than a ring", 12u, capacity + 8u, 0u, Status::invalid_parameter},
      {"head moved", 12u, 64u, 16u, Status::invalid_parameter},
      {"another order", 13u, 0u, 0u, Status::invalid_parameter},
  };
  for (const Case &c : cases) {
      Fixture fixture;
      static_cast<void>(RingHeader(c.order, c.tail, c.head).encode(fixture.ring().bytes(), 32u));
      if (fixture.consumer().prepare(fixture.window()) != c.status)
          av::TestRegistry::instance().record_failure(__FILE__, __LINE__, c.name);
      CHECK(fixture.consumer().invariants_hold());
  }
  Fixture malformed;
  malformed.ring().bytes()[1] = 0u;
  CHECK_STATUS(malformed.consumer().prepare(malformed.window()), Status::invalid_parameter);
  Fixture again;
  CHECK_STATUS(again.consumer().prepare(again.window()), Status::ok);
  CHECK_STATUS(again.consumer().prepare(again.window()), Status::invalid_state); }

TEST(ring_consumer_prepare, a_blob_too_small_for_the_order_is_refused)
{ Fixture fixture;
  void         *pointer = fixture.ring().bytes();
  std::uint64_t length = 32u + capacity - 1u;
  CHECK_STATUS(fixture.consumer().prepare(RingWindow(BlobWindow(&pointer, &length, 1u, length), 12u)),
               Status::invalid_parameter);
  CHECK(!fixture.consumer().attached()); }

TEST(ring_consumer_withdraw, forgets_a_ring_whose_attachment_was_never_posted)
{ Fixture fixture;
  CHECK_STATUS(fixture.consumer().prepare(fixture.window()), Status::ok);
  fixture.consumer().withdraw();
  CHECK(fixture.consumer().invariants_hold());
  fixture.ring().write_batch(1u, 0u);
  fixture.ring().publish();
  fixture.serve();
  CHECK(!fixture.consumer().attached());
  CHECK_EQ(fixture.ring().head(), 0u);
  fixture.ring().publish(0u);
  CHECK_STATUS(fixture.consumer().prepare(fixture.window()), Status::ok); }

TEST(ring_consumer_attach, an_unprepared_consumer_serves_nothing)
{ Fixture fixture;
  fixture.ring().write_batch(1u, 0u);
  fixture.ring().publish();
  fixture.serve();
  CHECK(!fixture.consumer().attached());
  CHECK_EQ(fixture.ring().head(), 0u);
  CHECK_EQ(fixture.consumer().head(), 0u);
  CHECK_EQ(fixture.consumer().last_fence(), 0u); }

TEST(ring_consumer, prepares_on_the_main_loop_and_serves_on_a_worker)
{ Fixture fixture;
  fixture.consumer().attach();
  fixture.consumer().service();
  fixture.consumer().detach();
  CHECK(fixture.host().claim(HostViolation::worker_required));
  CHECK(fixture.host().claim(HostViolation::worker_required));
  CHECK(fixture.host().claim(HostViolation::worker_required));
  CHECK(fixture.host().claim(HostViolation::worker_required));
  CHECK(fixture.host().claim(HostViolation::worker_required));
  void *thread = nullptr;
  CHECK_STATUS(fixture.host().start_thread(
                   [](void *context) {
                       auto *const f = static_cast<Fixture *>(context);
                       static_cast<void>(f->consumer().prepare(f->window()));
                       f->consumer().withdraw();
                   },
                   &fixture, thread),
               Status::ok);
  fixture.host().join_thread(thread);
  CHECK(fixture.host().claim(HostViolation::main_loop_required));
  CHECK(fixture.host().claim(HostViolation::main_loop_required)); }

TEST(ring_consumer_invariants, more_batches_than_bytes_or_a_head_past_the_tail_is_detected)
{ Fixture overcounted;
  overcounted.bind();
  av::RingConsumerProbe::overcount(overcounted.consumer());
  CHECK(!overcounted.consumer().invariants_hold());
  Fixture passed;
  passed.bind();
  av::RingConsumerProbe::pass_tail(passed.consumer());
  CHECK(!passed.consumer().invariants_hold());
  Fixture unprepared;
  av::RingConsumerProbe::attach_unprepared(unprepared.consumer());
  CHECK(!unprepared.consumer().invariants_hold());
  Fixture moved;
  av::RingConsumerProbe::move_unprepared(moved.consumer());
  CHECK(!moved.consumer().invariants_hold());
  Fixture broken;
  broken.bind();
  av::RingConsumerProbe::break_progress(broken.consumer());
  CHECK(!broken.consumer().invariants_hold());
  Fixture fenceless;
  fenceless.ring().write_batch(1u, 0u);
  fenceless.ring().publish();
  fenceless.bind();
  av::RingConsumerProbe::lose_fence(fenceless.consumer());
  CHECK(!fenceless.consumer().invariants_hold()); }

namespace {
void stamp(GuestRing &ring, std::uint64_t counter, std::uint64_t published_at)
{
    for (std::uint64_t i = 0u; i < 8u; i++)
        ring.data(counter + 16u + i) = static_cast<std::uint8_t>(published_at >> (8u * i));
}

std::uint64_t gpu_marked = 0u;
std::uint64_t gpu_reached = 0u;

void scripted_progress(void *, AvExecutor *, std::uint64_t *marked, std::uint64_t *reached)
{
    *marked = gpu_marked;
    *reached = gpu_reached;
}

std::uint64_t now_ns()
{
    timespec now = {};
    clock_gettime(CLOCK_MONOTONIC, &now);
    return static_cast<std::uint64_t>(now.tv_sec) * 1000000000u + static_cast<std::uint64_t>(now.tv_nsec);
}

}

TEST(ring_consumer_has_work, only_an_attached_healthy_ring_with_a_whole_header_published_has_work)
{ Fixture fixture;
  fixture.ring().write_batch(1u, 0u);
  fixture.ring().publish();
  CHECK(!fixture.consumer().has_work());
  fixture.bind();
  CHECK(fixture.consumer().has_work());
  fixture.serve();
  CHECK(!fixture.consumer().has_work());
  fixture.ring().write_batch(2u, 0u);
  fixture.ring().publish(fixture.ring().tail() - 1u);
  CHECK(!fixture.consumer().has_work());
  fixture.ring().publish();
  CHECK(fixture.consumer().has_work());
  fixture.ring().bytes()[0] = 0u;
  CHECK(!fixture.consumer().has_work());
  fixture.serve();
  CHECK(fixture.consumer().faulted());
  static_cast<void>(RingHeader(12u, fixture.ring().tail(), 0u).encode(fixture.ring().bytes(), 32u));
  CHECK(!fixture.consumer().has_work()); }

TEST(ring_consumer_earliest_pending, is_the_stamp_of_the_oldest_published_batch_not_run)
{ Fixture fixture;
  fixture.ring().write_batch(1u, 0u);
  stamp(fixture.ring(), 0u, 700u);
  fixture.ring().write_batch(2u, 0u);
  stamp(fixture.ring(), 32u, 900u);
  fixture.ring().publish();
  CHECK(fixture.consumer().earliest_pending() == UINT64_MAX);
  fixture.bind();
  CHECK(fixture.consumer().earliest_pending() == 700u);
  fixture.serve();
  CHECK(fixture.consumer().earliest_pending() == UINT64_MAX);
  fixture.ring().write_batch(3u, 0u);
  fixture.ring().publish(fixture.ring().tail() - 1u);
  CHECK(fixture.consumer().earliest_pending() == UINT64_MAX);
  fixture.ring().publish();
  fixture.ring().data(64u) = 0u;
  CHECK(fixture.consumer().earliest_pending() == 0u);
  fixture.serve();
  CHECK(fixture.consumer().faulted());
  CHECK(fixture.consumer().earliest_pending() == UINT64_MAX); }

TEST(ring_consumer_earliest_pending, a_ring_whose_executor_was_never_made_holds_nothing_up)
{ Fixture fixture;
  fixture.ops().fail_create_at(1u);
  fixture.ring().write_batch(1u, 0u);
  stamp(fixture.ring(), 0u, 5u);
  fixture.ring().publish();
  fixture.bind();
  CHECK(fixture.consumer().faulted());
  CHECK(fixture.consumer().earliest_pending() == UINT64_MAX); }

TEST(ring_consumer_published_end, is_the_tail_of_an_attached_ring_and_nothing_once_it_cannot_be_served)
{ Fixture fixture;
  fixture.ring().write_batch(1u, 0u);
  stamp(fixture.ring(), 0u, 700u);
  fixture.ring().publish();
  CHECK_EQ(fixture.consumer().published_end(), 0u);
  CHECK(fixture.consumer().consumed_to(fixture.ring().tail()));
  fixture.bind();
  CHECK_EQ(fixture.consumer().published_end(), fixture.ring().tail());
  CHECK(!fixture.consumer().consumed_to(fixture.consumer().published_end()));
  CHECK(fixture.consumer().consumed_to(0u));
  fixture.serve();
  CHECK(fixture.consumer().consumed_to(fixture.consumer().published_end()));
  CHECK(!fixture.consumer().consumed_to(fixture.consumer().published_end() + 1u));
  fixture.ring().write_batch(2u, 0u);
  fixture.ring().publish();
  const std::uint64_t cut = fixture.consumer().published_end();
  fixture.ring().write_batch(3u, 0u);
  fixture.ring().publish();
  fixture.serve();
  CHECK(fixture.consumer().consumed_to(cut));
  fixture.ring().write_batch(4u, 0u);
  fixture.ring().publish();
  fixture.ring().data(fixture.ring().head()) = 0u;
  fixture.serve();
  CHECK(fixture.consumer().faulted());
  CHECK_EQ(fixture.consumer().published_end(), 0u);
  CHECK(fixture.consumer().consumed_to(UINT64_MAX)); }

TEST(d3d_ordering, settling_waits_only_for_what_was_published_before_it_began)
{ Fixture drained;
  Fixture published;
  av::D3dOrdering ordering(drained.host());
  CHECK_STATUS(ordering.initialize(), Status::ok);
  published.ring().write_batch(1u, 0u);
  stamp(published.ring(), 0u, 1u);
  published.ring().publish();
  published.bind();
  drained.bind();
  ordering.add(drained.consumer(), drained.worker());
  std::uint64_t started = now_ns();
  ordering.settle();
  CHECK(now_ns() - started < 1000000u);
  published.serve();
  ordering.add(published.consumer(), published.worker());
  started = now_ns();
  ordering.settle();
  CHECK(now_ns() - started < 1000000u);
  published.ring().write_batch(2u, 0u);
  stamp(published.ring(), 32u, 2u);
  published.ring().publish();
  published.consumer().order_with(ordering);
  std::uint64_t waited = 0u;
  started = now_ns();
  std::thread settling([&] { ordering.settle(); waited = now_ns() - started; });
  usleep(5000);
  published.serve();
  settling.join();
  CHECK(waited >= 4000000u);
  CHECK(published.consumer().consumed_to(published.consumer().published_end()));
  ordering.remove(published.consumer());
  ordering.remove(drained.consumer()); }

TEST(ring_consumer_announce_awake, writes_the_flag_the_process_reads_once_attached)
{ Fixture fixture;
  constexpr std::size_t awake_at = 5u;
  fixture.consumer().announce_awake(true);
  CHECK_EQ(fixture.ring().bytes()[awake_at], 0u);
  fixture.bind();
  fixture.consumer().announce_awake(true);
  CHECK_EQ(fixture.ring().bytes()[awake_at], 1u);
  fixture.consumer().announce_awake(false);
  CHECK_EQ(fixture.ring().bytes()[awake_at], 0u); }

TEST(ring_window_announce_awake, writes_one_byte_and_is_refused_by_a_blob_too_short)
{ std::uint8_t  bytes[32u + capacity] = {};
  GuestRing     guest(bytes, 12u);
  void         *pointer = bytes;
  std::uint64_t length = sizeof bytes;
  const RingWindow window(BlobWindow(&pointer, &length, 1u, length), 12u);
  CHECK_STATUS(window.announce_awake(true), Status::ok);
  CHECK_EQ(bytes[5], 1u);
  CHECK_EQ(bytes[4], 12u);
  CHECK_EQ(bytes[6], 0u);
  CHECK_STATUS(window.announce_awake(false), Status::ok);
  CHECK_EQ(bytes[5], 0u);
  std::uint8_t  short_bytes[4] = {};
  void         *short_pointer = short_bytes;
  std::uint64_t short_length = sizeof short_bytes;
  const RingWindow cut(BlobWindow(&short_pointer, &short_length, 1u, 4u), 12u);
  CHECK(cut.announce_awake(true) != Status::ok);
  CHECK_EQ(short_bytes[0], 0u); }

TEST(ring_consumer_poll, completes_held_batches_as_the_gpu_reaches_their_marks)
{ Fixture fixture;
  fixture.ops().editable().gpu_progress = scripted_progress;
  gpu_marked = 4u;
  gpu_reached = 1u;
  fixture.bind();
  fixture.ring().write_batch(1u, 8u);
  fixture.ring().publish();
  fixture.serve();
  CHECK_EQ(fixture.consumer().batches(), 1u);
  CHECK_EQ(fixture.page().completed_fence(), 0u);
  CHECK(!fixture.poll());
  CHECK_EQ(fixture.page().completed_fence(), 0u);
  gpu_marked = 6u;
  fixture.ring().write_batch(2u, 8u);
  fixture.ring().publish();
  fixture.serve();
  CHECK_EQ(fixture.page().completed_fence(), 0u);
  gpu_reached = 4u;
  const std::uint32_t raised = fixture.host().raised(0u);
  CHECK(!fixture.poll());
  CHECK_EQ(fixture.page().completed_fence(), 1u);
  CHECK(fixture.host().raised(0u) > raised);
  gpu_reached = 6u;
  CHECK(!fixture.poll());
  CHECK_EQ(fixture.page().completed_fence(), 2u);
  gpu_marked = 7u;
  gpu_reached = 7u;
  fixture.ring().write_batch(3u, 8u);
  fixture.ring().publish();
  fixture.serve();
  CHECK_EQ(fixture.page().completed_fence(), 3u);
  CHECK(fixture.consumer().invariants_hold()); }

namespace {

std::uint32_t publishing_answer = 0u;

std::uint32_t scripted_poll(void *, AvExecutor *) { return publishing_answer; }

}

TEST(ring_consumer_service, the_worker_is_at_work_for_the_whole_of_a_batch_and_no_longer)
{ Fixture fixture;
  fixture.bind();
  CHECK(!fixture.fences().working());
  fixture.ops().hold();
  fixture.ring().write_batch(1u, 8u);
  fixture.ring().publish();
  void *thread = nullptr;
  CHECK_STATUS(fixture.host().start_thread([](void *consumer) { static_cast<RingConsumer *>(consumer)->service(); },
                                           &fixture.consumer(), thread),
               Status::ok);
  while (fixture.ops().waiting() == 0u) sched_yield();
  CHECK(fixture.fences().working());
  CHECK_EQ(fixture.page().completed_fence(), 0u);
  fixture.ops().release();
  fixture.host().join_thread(thread);
  CHECK_EQ(fixture.page().completed_fence(), 1u);
  CHECK(!fixture.fences().working()); }

TEST(ring_consumer_poll, keeping_watch_on_the_gpu_is_work_and_sleeping_is_not)
{ Fixture fixture;
  fixture.ops().editable().poll = scripted_poll;
  fixture.bind();
  publishing_answer = 1u;
  CHECK(fixture.poll());
  CHECK(fixture.fences().working());
  fixture.consumer().announce_awake(true);
  CHECK(fixture.fences().working());
  fixture.consumer().announce_awake(false);
  CHECK(!fixture.fences().working());
  CHECK(fixture.poll());
  publishing_answer = 0u;
  CHECK(!fixture.poll());
  CHECK(!fixture.fences().working()); }

TEST(ring_consumer_poll, a_ring_not_attached_or_faulted_polls_nothing)
{ Fixture fixture;
  CHECK(!fixture.poll());
  fixture.bind();
  fixture.ring().bytes()[0] = 0u;
  fixture.ring().publish(32u);
  fixture.serve();
  CHECK(fixture.consumer().faulted());
  CHECK(!fixture.poll()); }

TEST(ring_consumer_poll, a_device_lost_while_idle_faults_the_context_at_the_poll)
{ Fixture fixture;
  fixture.bind();
  fixture.ring().write_batch(1u, 8u);
  fixture.ring().publish();
  fixture.serve();
  CHECK_EQ(fixture.page().completed_fence(), 1u);
  CHECK(!fixture.poll());
  CHECK(!fixture.consumer().faulted());
  const std::uint32_t raised = fixture.host().raised(0u);
  fixture.ops().lose_host_device();
  CHECK(!fixture.poll());
  CHECK(fixture.consumer().faulted());
  CHECK(fixture.page().faulted());
  CHECK_EQ(fixture.page().fault_code(), static_cast<std::uint32_t>(FaultCode::device_lost));
  CHECK_EQ(fixture.page().completed_fence(), 1u);
  CHECK(fixture.host().raised(0u) > raised);
  CHECK(!fixture.consumer().has_work());
  CHECK(!fixture.fences().working());
  CHECK_EQ(fixture.ops().live(), 0u);
  const std::uint32_t after = fixture.host().raised(0u);
  CHECK(!fixture.poll());
  fixture.ring().write_batch(2u, 8u);
  fixture.ring().publish();
  fixture.serve();
  CHECK_EQ(fixture.page().completed_fence(), 1u);
  CHECK_EQ(fixture.host().raised(0u), after);
  CHECK(fixture.consumer().invariants_hold()); }

TEST(ring_consumer_poll, an_answer_no_library_may_give_is_idle)
{ Fixture fixture;
  fixture.ops().editable().poll = scripted_poll;
  fixture.bind();
  publishing_answer = 9u;
  CHECK(!fixture.poll());
  CHECK(!fixture.consumer().faulted());
  CHECK(!fixture.fences().working());
  publishing_answer = 0u; }

TEST(ring_consumer_order_with, a_batch_waits_for_an_earlier_one_on_another_stream_and_not_a_later)
{ Fixture waiting;
  Fixture other;
  av::D3dOrdering ordering(waiting.host());
  CHECK_STATUS(ordering.initialize(), Status::ok);
  CHECK_STATUS(ordering.initialize(), Status::ok);
  other.ring().write_batch(1u, 0u);
  stamp(other.ring(), 0u, 500u);
  other.ring().publish();
  other.bind();
  waiting.bind();
  ordering.add(waiting.consumer(), waiting.worker());
  ordering.add(other.consumer(), other.worker());
  waiting.consumer().order_with(ordering);

  waiting.ring().write_batch(1u, 0u);
  stamp(waiting.ring(), 0u, 400u);
  waiting.ring().publish();
  std::uint64_t started = now_ns();
  waiting.serve();
  const std::uint64_t unhindered = now_ns() - started;
  CHECK(unhindered < 20000000u);
  CHECK_EQ(waiting.consumer().batches(), 1u);

  waiting.ring().write_batch(2u, 0u);
  stamp(waiting.ring(), 32u, 600u);
  waiting.ring().publish();
  started = now_ns();
  waiting.serve();
  CHECK(now_ns() - started >= 20000000u);
  CHECK_EQ(waiting.consumer().batches(), 2u);

  ordering.remove(other.consumer());
  waiting.ring().write_batch(3u, 0u);
  stamp(waiting.ring(), 64u, 700u);
  waiting.ring().publish();
  started = now_ns();
  waiting.serve();
  CHECK(now_ns() - started < 20000000u);
  CHECK_EQ(waiting.consumer().batches(), 3u);
  ordering.remove(waiting.consumer()); }

TEST(d3d_ordering, a_stream_waited_on_is_woken_to_run_its_batch)
{ Fixture waiting;
  Fixture other;
  av::D3dOrdering ordering(waiting.host());
  CHECK_STATUS(ordering.initialize(), Status::ok);
  other.bind();
  waiting.bind();
  ordering.add(waiting.consumer(), waiting.worker());
  ordering.add(other.consumer(), other.worker());
  waiting.consumer().order_with(ordering);
  other.host().advance_clock_per_read(1000u);
  CHECK_STATUS(other.worker().start(), Status::ok);
  CHECK(other.host().wait_for_sleepers(1u, 2000u));

  other.ring().write_batch(1u, 0u);
  stamp(other.ring(), 0u, 500u);
  other.ring().publish();
  waiting.ring().write_batch(1u, 0u);
  stamp(waiting.ring(), 0u, 600u);
  waiting.ring().publish();
  const std::uint64_t started = now_ns();
  waiting.serve();
  CHECK(now_ns() - started < 20000000u);
  CHECK_EQ(waiting.consumer().batches(), 1u);
  other.worker().stop();
  CHECK_EQ(other.consumer().batches(), 1u);
  CHECK_EQ(other.ring().head(), other.ring().tail());
  ordering.remove(other.consumer());
  ordering.remove(waiting.consumer()); }

TEST(d3d_ordering, an_ordering_never_initialized_or_an_unstamped_batch_waits_for_nothing)
{ Fixture waiting;
  Fixture other;
  av::D3dOrdering ordering(waiting.host());
  other.ring().write_batch(1u, 0u);
  stamp(other.ring(), 0u, 1u);
  other.ring().publish();
  other.bind();
  waiting.bind();
  ordering.add(other.consumer(), other.worker());
  ordering.add(waiting.consumer(), waiting.worker());
  std::uint64_t started = now_ns();
  ordering.wait_before(waiting.consumer(), 900u);
  CHECK(now_ns() - started < 20000000u);
  CHECK_STATUS(ordering.initialize(), Status::ok);
  ordering.add(other.consumer(), other.worker());
  started = now_ns();
  ordering.wait_before(waiting.consumer(), 0u);
  CHECK(now_ns() - started < 20000000u);
  started = now_ns();
  ordering.wait_before(waiting.consumer(), 900u);
  CHECK(now_ns() - started >= 20000000u);
  std::uint64_t waited = 0u;
  started = now_ns();
  std::thread settling([&] { ordering.settle(); waited = now_ns() - started; });
  usleep(5000);
  waiting.host().set_clock(waiting.host().monotonic_nanoseconds() + 1000000000u);
  settling.join();
  CHECK(waited >= 4000000u);
  ordering.remove(other.consumer());
  ordering.remove(other.consumer());
  started = now_ns();
  ordering.wait_before(waiting.consumer(), 900u);
  CHECK(now_ns() - started < 20000000u); }

namespace {

void two_commands(std::uint8_t (&payload)[16])
{
    std::memset(payload, 0, sizeof payload);
    const std::uint32_t id = GuestRing::filler_command;
    const std::uint32_t size = 8u;
    for (std::uint32_t command = 0u; command < 2u; command++) {
        std::memcpy(payload + command * 8u, &id, sizeof id);
        std::memcpy(payload + command * 8u + 4u, &size, sizeof size);
    }
}

class CapturedStderr {
public:
    CapturedStderr() : saved_(dup(2)), file_(tmpfile())
    {
        std::fflush(stderr);
        dup2(fileno(file_), 2);
    }
    ~CapturedStderr()
    {
        restore();
        std::fclose(file_);
    }
    CapturedStderr(const CapturedStderr &) = delete;
    CapturedStderr &operator=(const CapturedStderr &) = delete;

    std::string text()
    {
        restore();
        std::string captured;
        std::rewind(file_);
        char buffer[256];
        while (std::fgets(buffer, sizeof buffer, file_) != nullptr) captured += buffer;
        return captured;
    }

private:
    void restore()
    {
        if (saved_ < 0) return;
        std::fflush(stderr);
        dup2(saved_, 2);
        close(saved_);
        saved_ = -1;
    }

    int   saved_;
    FILE *file_;
};

std::uint64_t held_calls = 0u;

void progress_that_catches_up(void *, AvExecutor *, std::uint64_t *marked, std::uint64_t *reached)
{
    *marked = 5u;
    *reached = ++held_calls > 400u ? 5u : 0u;
}

}

TEST(ring_consumer_service, a_batch_abandoned_while_it_runs_stops_quietly_without_a_fault)
{ Fixture fixture;
  fixture.bind();
  fixture.ops().hold();
  std::uint8_t payload[16];
  two_commands(payload);
  fixture.ring().write_batch(1u, payload, sizeof payload);
  fixture.ring().publish();
  void *thread = nullptr;
  CHECK_STATUS(fixture.host().start_thread([](void *consumer) { static_cast<RingConsumer *>(consumer)->service(); },
                                           &fixture.consumer(), thread),
               Status::ok);
  while (fixture.ops().waiting() == 0u) sched_yield();
  fixture.consumer().abandon();
  fixture.ops().release();
  fixture.host().join_thread(thread);
  CHECK(!fixture.consumer().faulted());
  CHECK(!fixture.page().faulted());
  CHECK_EQ(fixture.consumer().batches(), 0u);
  CHECK_EQ(fixture.ops().executed(), 1u);
  CHECK_EQ(fixture.host().logged(), 0u);
  CHECK_EQ(fixture.page().completed_fence(), 0u);
  CHECK_EQ(fixture.consumer().head(), 0u); }

TEST(ring_consumer_service, a_full_queue_of_held_completions_is_drained_until_the_gpu_catches_up)
{ Fixture fixture;
  held_calls = 0u;
  fixture.ops().editable().gpu_progress = progress_that_catches_up;
  fixture.bind();
  std::uint64_t fence = 1u;
  for (std::uint32_t round = 0u; round < 3u; round++) {
      for (std::uint32_t batch = 0u; batch < 50u; batch++) fixture.ring().write_batch(fence++, 0u);
      fixture.ring().publish();
      fixture.serve();
  }
  CHECK(!fixture.consumer().faulted());
  CHECK_EQ(fixture.consumer().batches(), 150u);
  CHECK(held_calls > 400u);
  CHECK_EQ(fixture.page().completed_fence(), 150u);
  CHECK(fixture.consumer().invariants_hold()); }

TEST(ring_consumer_earliest_pending, a_detached_or_unreadable_or_truncated_ring_holds_nothing_up_or_everything)
{ Fixture detached;
  detached.ring().write_batch(1u, 0u);
  stamp(detached.ring(), 0u, 700u);
  detached.ring().publish();
  detached.bind();
  CHECK(detached.consumer().earliest_pending() == 700u);
  detached.detach();
  CHECK(detached.consumer().earliest_pending() == UINT64_MAX);

  Fixture unreadable;
  unreadable.ring().write_batch(1u, 0u);
  unreadable.ring().publish();
  unreadable.bind();
  unreadable.ring().bytes()[0] = 0u;
  CHECK(!unreadable.consumer().faulted());
  CHECK(unreadable.consumer().earliest_pending() == UINT64_MAX);

  Fixture truncated;
  truncated.bind();
  truncated.ring().write_batch(1u, 0u);
  truncated.ring().publish();
  void         *pointer = truncated.ring().bytes();
  std::uint64_t length = 32u + 20u;
  av::RingConsumerProbe::replace_ring(truncated.consumer(), RingWindow(BlobWindow(&pointer, &length, 1u, length), 12u));
  CHECK(truncated.consumer().earliest_pending() == 0u); }

TEST(ring_consumer_earliest_pending, while_a_batch_runs_it_is_the_stamp_of_the_batch_after_it)
{ Fixture fixture;
  fixture.bind();
  fixture.ops().hold();
  fixture.ring().write_batch(1u, 8u);
  stamp(fixture.ring(), 0u, 300u);
  fixture.ring().write_batch(2u, 0u);
  stamp(fixture.ring(), 40u, 800u);
  fixture.ring().publish();
  CHECK(fixture.consumer().earliest_pending() == 300u);
  void *thread = nullptr;
  CHECK_STATUS(fixture.host().start_thread([](void *consumer) { static_cast<RingConsumer *>(consumer)->service(); },
                                           &fixture.consumer(), thread),
               Status::ok);
  while (fixture.ops().waiting() == 0u) sched_yield();
  CHECK(fixture.consumer().earliest_pending() == 800u);
  fixture.ops().release();
  fixture.host().join_thread(thread);
  CHECK_EQ(fixture.consumer().batches(), 2u);
  CHECK(fixture.consumer().earliest_pending() == UINT64_MAX); }

TEST(ring_consumer_service, with_clock_pairs_on_every_sixteenth_stamped_batch_prints_the_host_and_guest_clocks)
{ setenv("AV_PERFETTO", "1", 1);
  std::string printed;
  {
      CapturedStderr captured;
      Fixture        stamped;
      stamped.bind();
      for (std::uint32_t batch = 0u; batch < 17u; batch++) {
          stamped.ring().write_batch(batch + 1u, 0u);
          stamp(stamped.ring(), batch * 32u, 100u * (batch + 1u));
      }
      stamped.ring().publish();
      stamped.serve();
      CHECK_EQ(stamped.consumer().batches(), 17u);
      Fixture unstamped;
      unstamped.bind();
      unstamped.ring().write_batch(1u, 0u);
      unstamped.ring().publish();
      unstamped.serve();
      CHECK_EQ(unstamped.consumer().batches(), 1u);
      printed = captured.text();
  }
  unsetenv("AV_PERFETTO");
  std::size_t lines = 0u;
  for (std::size_t at = printed.find("av-clock host="); at != std::string::npos; at = printed.find("av-clock host=", at + 1u))
      lines++;
  CHECK_EQ(lines, 2u);
  CHECK(printed.find(" guest=100\n") != std::string::npos);
  CHECK(printed.find(" guest=1700\n") != std::string::npos);
  CHECK(printed.find(" guest=0\n") == std::string::npos); }

TEST(d3d_ordering, a_stream_added_past_the_table_is_not_waited_on)
{ Fixture waiting;
  Fixture other;
  av::D3dOrdering ordering(waiting.host());
  CHECK_STATUS(ordering.initialize(), Status::ok);
  other.ring().write_batch(1u, 0u);
  stamp(other.ring(), 0u, 1u);
  other.ring().publish();
  other.bind();
  waiting.bind();
  for (std::uint32_t member = 0u; member < av::D3dOrdering::capacity; member++)
      ordering.add(waiting.consumer(), waiting.worker());
  ordering.add(other.consumer(), other.worker());
  const std::uint64_t started = now_ns();
  ordering.wait_before(waiting.consumer(), 900u);
  CHECK(now_ns() - started < 20000000u);
  ordering.remove(other.consumer());
  for (std::uint32_t member = 0u; member < av::D3dOrdering::capacity; member++) ordering.remove(waiting.consumer()); }

namespace {

constexpr std::uint64_t watchdog_limit_ns = av::SubmissionWatchdog::default_limit_ns;

}

TEST(ring_consumer_watchdog, a_batch_the_gpu_never_reaches_loses_the_context_after_the_limit)
{ Fixture fixture;
  fixture.ops().editable().gpu_progress = scripted_progress;
  gpu_marked = 4u;
  gpu_reached = 0u;
  fixture.host().set_clock(1000u);
  fixture.bind();
  fixture.ring().write_batch(1u, 8u);
  fixture.ring().publish();
  fixture.serve();
  CHECK(!fixture.consumer().faulted());
  fixture.host().set_clock(1000u + watchdog_limit_ns - 1u);
  CHECK(!fixture.poll());
  CHECK(!fixture.consumer().faulted());
  fixture.host().set_clock(1000u + watchdog_limit_ns + 1u);
  CHECK(!fixture.poll());
  CHECK(fixture.consumer().faulted());
  CHECK(fixture.page().faulted());
  CHECK_EQ(fixture.page().fault_code(), static_cast<std::uint32_t>(FaultCode::device_lost));
  CHECK_EQ(fixture.page().completed_fence(), 0u); }

TEST(ring_consumer_watchdog, a_gpu_that_keeps_reaching_marks_is_never_judged_however_long_the_backlog_is)
{ Fixture fixture;
  fixture.ops().editable().gpu_progress = scripted_progress;
  gpu_marked = 100u;
  gpu_reached = 0u;
  fixture.host().set_clock(1000u);
  fixture.bind();
  fixture.ring().write_batch(1u, 8u);
  fixture.ring().publish();
  fixture.serve();
  for (std::uint64_t step = 1u; step <= 5u; step++) {
      fixture.host().set_clock(1000u + step * (watchdog_limit_ns / 2u));
      gpu_reached = step;
      CHECK(!fixture.poll());
  }
  CHECK(!fixture.consumer().faulted()); }

TEST(ring_consumer_watchdog, a_context_queued_behind_a_busy_gpu_is_not_lost_while_other_contexts_complete_work)
{ Fixture fixture;
  fixture.ops().editable().gpu_progress = scripted_progress;
  gpu_marked = 4u;
  gpu_reached = 0u;
  fixture.host().set_clock(1000u);
  fixture.bind();
  fixture.ring().write_batch(1u, 8u);
  fixture.ring().publish();
  fixture.serve();
  for (std::uint64_t step = 1u; step <= 20u; step++) {
      fixture.host().set_clock(1000u + step * (watchdog_limit_ns / 2u));
      fixture.pulse().note_completion();
      CHECK(!fixture.poll());
      CHECK(!fixture.consumer().faulted());
  }
  CHECK(fixture.consumer().watchdog_deadline() > 1000u + 10u * watchdog_limit_ns);
  fixture.host().set_clock(1000u + 21u * (watchdog_limit_ns / 2u) + watchdog_limit_ns + 1u);
  CHECK(!fixture.poll());
  CHECK(fixture.consumer().faulted());
  CHECK_EQ(fixture.host().logged(), 1u); }

TEST(ring_consumer_watchdog, a_context_with_no_progress_on_an_idle_gpu_is_lost_once_after_the_limit)
{ Fixture fixture;
  fixture.ops().editable().gpu_progress = scripted_progress;
  gpu_marked = 4u;
  gpu_reached = 0u;
  fixture.host().set_clock(1000u);
  fixture.bind();
  fixture.ring().write_batch(1u, 8u);
  fixture.ring().publish();
  fixture.serve();
  fixture.host().set_clock(1000u + watchdog_limit_ns - 1u);
  CHECK(!fixture.poll());
  CHECK(!fixture.consumer().faulted());
  fixture.host().set_clock(1000u + watchdog_limit_ns + 1u);
  CHECK(!fixture.poll());
  CHECK(fixture.consumer().faulted());
  fixture.host().set_clock(1000u + watchdog_limit_ns * 3u);
  CHECK(!fixture.poll());
  CHECK_EQ(fixture.host().logged(), 1u); }

TEST(ring_consumer_watchdog, the_environment_override_sets_the_limit)
{ CHECK_EQ(setenv("AV_SUBMIT_WATCHDOG_MS", "250", 1), 0);
  {
      Fixture fixture;
      fixture.ops().editable().gpu_progress = scripted_progress;
      gpu_marked = 4u;
      gpu_reached = 0u;
      fixture.host().set_clock(1000u);
      fixture.bind();
      fixture.ring().write_batch(1u, 8u);
      fixture.ring().publish();
      fixture.serve();
      CHECK_EQ(fixture.consumer().watchdog_deadline(), std::uint64_t{1000u + 250000000u});
      fixture.host().set_clock(1000u + 250000000u + 1u);
      CHECK(!fixture.poll());
      CHECK(fixture.consumer().faulted());
  }
  unsetenv("AV_SUBMIT_WATCHDOG_MS"); }

TEST(ring_consumer_watchdog, a_context_with_nothing_outstanding_is_never_judged)
{ Fixture fixture;
  fixture.bind();
  fixture.host().set_clock(1000u);
  fixture.ring().write_batch(1u, 8u);
  fixture.ring().publish();
  fixture.serve();
  fixture.host().set_clock(1000u + watchdog_limit_ns * 10u);
  CHECK(!fixture.poll());
  CHECK(!fixture.consumer().faulted()); }

TEST(ring_consumer_watchdog, a_held_submission_exposes_the_deadline_an_idle_worker_must_wake_for)
{ Fixture fixture;
  fixture.ops().editable().gpu_progress = scripted_progress;
  gpu_marked = 4u;
  gpu_reached = 0u;
  fixture.host().set_clock(1000u);
  fixture.bind();
  CHECK_EQ(fixture.consumer().watchdog_deadline(), std::uint64_t{0});
  fixture.ring().write_batch(1u, 8u);
  fixture.ring().publish();
  fixture.serve();
  CHECK_EQ(fixture.consumer().watchdog_deadline(), 1000u + watchdog_limit_ns);
  fixture.host().set_clock(1000u + watchdog_limit_ns + 1u);
  CHECK(!fixture.poll());
  CHECK_EQ(fixture.consumer().watchdog_deadline(), std::uint64_t{0}); }

namespace {

std::uint64_t stuck_progress_calls = 0u;

void progress_that_never_reaches(void *, AvExecutor *, std::uint64_t *marked, std::uint64_t *reached)
{
    stuck_progress_calls++;
    *marked = 1000000u;
    *reached = 0u;
}

}

TEST(ring_consumer_watchdog, a_full_queue_behind_a_stuck_gpu_faults_once_logs_once_and_then_costs_nothing)
{ Fixture fixture;
  fixture.ops().editable().gpu_progress = progress_that_never_reaches;
  stuck_progress_calls = 0u;
  fixture.host().set_clock(1000u);
  fixture.host().advance_clock_per_read(watchdog_limit_ns / 1000u);
  fixture.bind();
  std::uint64_t fence = 1u;
  for (std::uint32_t round = 0u; round < 2u; round++) {
      for (std::uint32_t batch = 0u; batch < 100u; batch++) fixture.ring().write_batch(fence++, 8u);
      fixture.ring().publish();
      fixture.serve();
  }
  CHECK(fixture.consumer().faulted());
  CHECK(fixture.page().faulted());
  CHECK_EQ(fixture.host().logged(), 1u);
  CHECK_EQ(fixture.consumer().batches(), std::uint64_t{av::HeldCompletions::capacity} + 1u);
  CHECK_EQ(fixture.ops().executed(), av::HeldCompletions::capacity + 1u);
  const std::uint64_t batches = fixture.consumer().batches();
  const std::uint64_t calls = stuck_progress_calls;
  fixture.host().advance_clock_per_read(0u);
  fixture.host().set_clock(1000u + watchdog_limit_ns * 1000u);
  for (std::uint32_t round = 0u; round < 20u; round++) {
      fixture.ring().write_batch(fence++, 8u);
      fixture.ring().publish();
      fixture.serve();
      CHECK(!fixture.poll());
      CHECK(!fixture.consumer().has_work());
      CHECK_EQ(fixture.consumer().watchdog_deadline(), std::uint64_t{0});
  }
  av::RingConsumerProbe::fault_again(fixture.consumer());
  CHECK(!av::RingConsumerProbe::settle(fixture.consumer()));
  CHECK_EQ(fixture.host().logged(), 1u);
  CHECK_EQ(stuck_progress_calls, calls);
  CHECK_EQ(fixture.consumer().batches(), batches); }
