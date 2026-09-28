/* One context's ring consumption, as its worker runs it: complete batches only,
 * head published after each, and any ring no valid producer could write
 * stopping the context for good. Service runs on a mock worker thread. */
#include "core/support/status.hpp"
#include "device/context/d3d_ordering.hpp"
#include "device/fence/fence_interrupts.hpp"
#include "device/fence/fence_reporter.hpp"
#include "device/fence/fence_window.hpp"
#include "device/memory/blob_window.hpp"
#include "device/ring/ring_consumer.hpp"
#include "device/recording/wire_recorder.hpp"
#include "device/ring/ring_window.hpp"
#include "device/threading/worker.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/host/host_violation.hpp"
#include "harness/executor/mock_context_execution.hpp"
#include "harness/host/mock_host.hpp"
#include "harness/ring/guest_ring.hpp"
#include "proto/submission/fault_code.hpp"
#include "proto/submission/fence_page.hpp"
#include "proto/submission/ring_header.hpp"
#include "proto/submission/ring_progress.hpp"

#include <cstdint>
#include <cstring>
#include <ctime>

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
    static void lose_fence(RingConsumer &consumer) { consumer.batches_ = 1u; consumer.head_ = 32u; consumer.last_fence_ = 0u; }
};



}  // namespace av

namespace {

constexpr std::uint64_t capacity = 4096u;
constexpr std::uint64_t head_offset = GuestRing::head_offset;

/* A ring of order 12 in a buffer, and the guest writing it. */
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
    }

    MockHost     &host() { return host_; }
    RingConsumer &consumer() { return consumer_; }
    /* The thread that would serve the ring; nothing starts it but a test. */
    av::Worker   &worker() { return worker_; }
    RingWindow    window() const { return memory_.window(); }
    GuestRing    &ring() { return memory_.guest(); }

    ~Fixture()
    {
        worker_.stop();
        on_worker([](RingConsumer &consumer) { consumer.detach(); });
    }

    av::MockExecutorOps &ops() { return execution_.ops(); }

    /* The fence page as the guest would read it. */
    FencePage page() const
    {
        FencePage decoded;
        static_cast<void>(decoded.decode(page_, sizeof page_));
        return decoded;
    }

    /* Prepared on the main loop, attached on a worker. */
    void bind()
    {
        CHECK_STATUS(fences_.prepare(FenceWindow(BlobWindow(&page_pointer_, &page_length_, 1u, page_length_)), 0u),
                     Status::ok);
        CHECK_STATUS(consumer_.prepare(memory_.window()), Status::ok);
        CHECK(consumer_.invariants_hold());
        on_worker([](RingConsumer &consumer) { consumer.attach(); });
        CHECK(consumer_.attached());
    }
    /* The worker's poll between batches. */
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
    FenceReporter   fences_;
    av::MockContextExecution execution_;
    RingConsumer    consumer_;
    av::Worker      worker_;
    RingMemory      memory_;
};

}  // namespace

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
  /* A 32-byte header each, and the payloads rounded up to 8. */
  CHECK_EQ(fixture.consumer().head(), 32u + 48u + 96u);
  CHECK_EQ(fixture.ring().head(), fixture.ring().tail());
  CHECK_EQ(fixture.page().completed_fence(), 9u);
  /* Three batches, and one heartbeat: the first command run, the next being
   * 256 commands on. */
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
  fixture.ring().publish(40u + 15u);   /* inside the second header */
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
      if (c.command != 0u) std::memcpy(&fixture.ring().data(40u + 32u), &c.command, 4u);   /* the second batch's command */
      fixture.ring().publish();
      fixture.serve();
      if (fixture.page().fault_code() != static_cast<std::uint32_t>(c.code))
          av::TestRegistry::instance().record_failure(__FILE__, __LINE__, c.name);
      CHECK(fixture.consumer().faulted());
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
      CHECK_EQ(fixture.host().raised(0u), 2u);   /* the completion's service, then the fault's */
      CHECK_EQ(fixture.ops().live(), 0u);        /* the executor went with the fault */
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
  std::uint64_t length = 32u + 32u + 20u;   /* the first batch whole, the second's header cut short */
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
  CHECK(fixture.host().claim(HostViolation::worker_required));   /* detach */
  CHECK(fixture.host().claim(HostViolation::worker_required));   /* the batch executor's stop */
  CHECK(fixture.host().claim(HostViolation::worker_required));   /* the executor's destroy */
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

/* ---- what other threads ask of a ring, and GPU work that outlives its batch -- */

namespace {

/* The guest's stamp on the batch at `counter`: its performance counter when it
 * wrote the batch, 16 bytes into the header. */
void stamp(GuestRing &ring, std::uint64_t counter, std::uint64_t published_at)
{
    for (std::uint64_t i = 0u; i < 8u; i++)
        ring.data(counter + 16u + i) = static_cast<std::uint8_t>(published_at >> (8u * i));
}

std::uint64_t gpu_marked = 0u;
std::uint64_t gpu_reached = 0u;

void scripted_progress(void *, RvkExecutor *, std::uint64_t *marked, std::uint64_t *reached)
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

}  // namespace

TEST(ring_consumer_has_work, only_an_attached_healthy_ring_with_a_whole_header_published_has_work)
{ Fixture fixture;
  fixture.ring().write_batch(1u, 0u);
  fixture.ring().publish();
  CHECK(!fixture.consumer().has_work());   /* not attached */
  fixture.bind();
  CHECK(fixture.consumer().has_work());
  fixture.serve();
  CHECK(!fixture.consumer().has_work());
  fixture.ring().write_batch(2u, 0u);
  fixture.ring().publish(fixture.ring().tail() - 1u);   /* a header short of whole */
  CHECK(!fixture.consumer().has_work());
  fixture.ring().publish();
  CHECK(fixture.consumer().has_work());
  fixture.ring().bytes()[0] = 0u;   /* a header no producer writes */
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
  CHECK(fixture.consumer().earliest_pending() == UINT64_MAX);   /* not attached */
  fixture.bind();
  CHECK(fixture.consumer().earliest_pending() == 700u);
  fixture.serve();
  /* Nothing published is left: nothing to wait for. */
  CHECK(fixture.consumer().earliest_pending() == UINT64_MAX);
  fixture.ring().write_batch(3u, 0u);
  fixture.ring().publish(fixture.ring().tail() - 1u);
  CHECK(fixture.consumer().earliest_pending() == UINT64_MAX);
  fixture.ring().publish();
  fixture.ring().data(64u) = 0u;   /* torn by an owner moving on: due now */
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

TEST(ring_consumer_announce_awake, writes_the_flag_the_process_reads_once_attached)
{ Fixture fixture;
  constexpr std::size_t awake_at = 5u;
  fixture.consumer().announce_awake(true);
  CHECK_EQ(fixture.ring().bytes()[awake_at], 0u);   /* no ring attached to write it in */
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
  CHECK_EQ(bytes[4], 12u);   /* the order beside it untouched */
  CHECK_EQ(bytes[6], 0u);
  CHECK_STATUS(window.announce_awake(false), Status::ok);
  CHECK_EQ(bytes[5], 0u);
  std::uint8_t  short_bytes[4] = {};
  void         *short_pointer = short_bytes;
  std::uint64_t short_length = sizeof short_bytes;
  const RingWindow cut(BlobWindow(&short_pointer, &short_length, 1u, 4u), 12u);
  CHECK(cut.announce_awake(true) != Status::ok);
  CHECK_EQ(short_bytes[0], 0u); }

/* A batch whose GPU work has not finished is held, not completed: the guest
 * waiting on its fence waits for the GPU too. Held ones complete oldest first
 * as the GPU passes their marks, from the worker's poll between batches. */
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
  CHECK_EQ(fixture.page().completed_fence(), 0u);   /* held for mark 4 */
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
  /* Nothing held and the GPU caught up: the next completes at once. */
  gpu_marked = 7u;
  gpu_reached = 7u;
  fixture.ring().write_batch(3u, 8u);
  fixture.ring().publish();
  fixture.serve();
  CHECK_EQ(fixture.page().completed_fence(), 3u);
  CHECK(fixture.consumer().invariants_hold()); }

TEST(ring_consumer_poll, a_ring_not_attached_or_faulted_polls_nothing)
{ Fixture fixture;
  CHECK(!fixture.poll());
  fixture.bind();
  fixture.ring().bytes()[0] = 0u;
  fixture.ring().publish(32u);
  fixture.serve();
  CHECK(fixture.consumer().faulted());
  CHECK(!fixture.poll()); }

/* Two Direct3D streams: a batch waits for the other stream's batches the guest
 * published before it, and a batch published first does not wait. The other
 * stream never runs here, so a wait is the ordering's whole bound: 1000 pauses
 * of at least 20 us. */
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
  stamp(waiting.ring(), 0u, 400u);   /* published before the other stream's */
  waiting.ring().publish();
  std::uint64_t started = now_ns();
  waiting.serve();
  const std::uint64_t unhindered = now_ns() - started;
  CHECK(unhindered < 20000000u);
  CHECK_EQ(waiting.consumer().batches(), 1u);

  waiting.ring().write_batch(2u, 0u);
  stamp(waiting.ring(), 32u, 600u);   /* after it: waits the bound out */
  waiting.ring().publish();
  started = now_ns();
  waiting.serve();
  CHECK(now_ns() - started >= 20000000u);
  CHECK_EQ(waiting.consumer().batches(), 2u);

  /* Unstamped, or the other stream gone: nothing to wait for. */
  ordering.remove(other.consumer());
  waiting.ring().write_batch(3u, 0u);
  stamp(waiting.ring(), 64u, 700u);
  waiting.ring().publish();
  started = now_ns();
  waiting.serve();
  CHECK(now_ns() - started < 20000000u);
  CHECK_EQ(waiting.consumer().batches(), 3u);
  ordering.remove(waiting.consumer()); }

/* A stream waited on is woken to run what it holds. A process publishes its
 * own tail, so a batch can sit in a ring whose worker is asleep with no doorbell
 * coming - the kernel rings that only once dxgkrnl hands the packet over, which
 * it holds while the waiting stream's own packet is still running. The waited-on
 * worker here is asleep before its batch is published and nothing else rings
 * it: without the wake the wait would run to its 20 ms bound and the batch
 * never run. */
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
  other.host().advance_clock_per_read(1000u);   /* so the worker's spin ends and it sleeps */
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
  ordering.add(other.consumer(), other.worker());   /* no mutex: not added */
  ordering.add(waiting.consumer(), waiting.worker());
  /* Waiting would take 1000 pauses of 20 us at least. */
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
  /* Settling waits for every stream, its bound 100 pauses. */
  started = now_ns();
  ordering.settle();
  CHECK(now_ns() - started >= 2000000u);
  ordering.remove(other.consumer());
  ordering.remove(other.consumer());
  started = now_ns();
  ordering.wait_before(waiting.consumer(), 900u);
  CHECK(now_ns() - started < 20000000u); }
