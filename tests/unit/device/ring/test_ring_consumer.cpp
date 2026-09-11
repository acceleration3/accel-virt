/* One context's ring consumption, as its worker runs it: complete batches only,
 * head published after each, and any ring no valid producer could write
 * stopping the context for good. Service runs on a mock worker thread. */
#include "core/support/status.hpp"
#include "device/fence/fence_interrupts.hpp"
#include "device/fence/fence_reporter.hpp"
#include "device/fence/fence_window.hpp"
#include "device/memory/blob_window.hpp"
#include "device/ring/ring_consumer.hpp"
#include "device/ring/ring_window.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/host/host_violation.hpp"
#include "harness/host/mock_host.hpp"
#include "harness/ring/guest_ring.hpp"
#include "proto/submission/fence_page.hpp"
#include "proto/submission/ring_header.hpp"
#include "proto/submission/ring_progress.hpp"

#include <cstdint>
#include <cstring>

using av::BlobWindow;
using av::FenceInterrupts;
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
    static void lose_fence(RingConsumer &consumer) { consumer.batches_ = 1u; consumer.head_ = 16u; consumer.last_fence_ = 0u; }
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
        : page_{}, page_pointer_(page_), page_length_(sizeof page_), interrupts_(host_), fences_(host_, interrupts_),
          consumer_(host_, fences_)
    {
        static_cast<void>(interrupts_.initialize());
        static_cast<void>(interrupts_.connect());
    }

    MockHost     &host() { return host_; }
    RingConsumer &consumer() { return consumer_; }
    RingWindow    window() const { return memory_.window(); }
    GuestRing    &ring() { return memory_.guest(); }

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
    FenceReporter   fences_;
    RingConsumer    consumer_;
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
  CHECK(!fixture.consumer().stopped());
  CHECK_EQ(fixture.consumer().batches(), 3u);
  CHECK_EQ(fixture.consumer().last_fence(), 9u);
  CHECK_EQ(fixture.consumer().head(), 16u + 32u + 80u);
  CHECK_EQ(fixture.ring().head(), fixture.ring().tail());
  CHECK_EQ(fixture.page().completed_fence(), 9u);
  CHECK_EQ(fixture.page().progress(), 3u);
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
  fixture.ring().publish(24u + 15u);   /* inside the second header */
  fixture.serve();
  CHECK_EQ(fixture.consumer().batches(), 1u);
  CHECK_EQ(fixture.ring().head(), 24u);
  fixture.ring().publish(fixture.ring().tail() - 1u);
  fixture.serve();
  CHECK_EQ(fixture.consumer().head(), 24u);
  fixture.ring().publish();
  fixture.serve();
  CHECK_EQ(fixture.consumer().batches(), 2u);
  CHECK_EQ(fixture.ring().head(), fixture.ring().tail());
  CHECK(!fixture.consumer().stopped());
  CHECK_EQ(fixture.page().completed_fence(), 2u);
  CHECK_EQ(fixture.host().raised(0u), 2u); }

TEST(ring_consumer_service, reads_batches_across_the_end_of_the_data_area)
{ Fixture fixture;
  fixture.bind();
  std::uint64_t fence = 1u;
  for (std::uint32_t round = 0u; round < 5u; round++) {
      for (std::uint32_t i = 0u; i < 7u; i++) fixture.ring().write_batch(fence++, 3u + round * 37u + i * 11u);
      fixture.ring().publish();
      fixture.serve();
      CHECK_EQ(fixture.ring().head(), fixture.ring().tail());
  }
  CHECK(fixture.ring().tail() > capacity);
  CHECK_EQ(fixture.consumer().batches(), 35u);
  CHECK(!fixture.consumer().stopped()); }

TEST(ring_consumer_service, a_full_ring_is_consumed_whole)
{ Fixture fixture;
  fixture.bind();
  fixture.ring().write_batch(1u, 2000u);
  fixture.ring().write_batch(2u, capacity - 2016u - 16u);
  CHECK_EQ(fixture.ring().tail(), capacity);
  fixture.ring().publish();
  fixture.serve();
  CHECK_EQ(fixture.consumer().head(), capacity);
  fixture.ring().write_batch(3u, capacity - 16u);
  fixture.ring().publish();
  fixture.serve();
  CHECK_EQ(fixture.consumer().head(), 2u * capacity);
  CHECK(!fixture.consumer().stopped()); }

TEST(ring_consumer_service, the_head_the_guest_writes_is_ignored)
{ Fixture fixture;
  fixture.bind();
  fixture.ring().write_batch(1u, 0u);
  fixture.ring().publish();
  fixture.ring().store(head_offset, 0xDEADu);
  fixture.serve();
  CHECK_EQ(fixture.ring().head(), 16u);
  fixture.ring().store(head_offset, 0u);
  fixture.ring().write_batch(2u, 0u);
  fixture.ring().publish();
  fixture.serve();
  CHECK_EQ(fixture.ring().head(), 32u);
  CHECK(!fixture.consumer().stopped()); }

TEST(ring_consumer_service, a_hostile_ring_stops_the_context_logs_once_and_ignores_later_doorbells)
{ struct Case {
      const char *name;
      void      (*spoil)(GuestRing &ring);
  };
  const Case cases[] = {
      {"header magic", [](GuestRing &ring) { ring.bytes()[0] = 0u; }},
      {"order changed", [](GuestRing &ring) { ring.bytes()[4] = 13u; }},
      {"tail backwards", [](GuestRing &ring) { ring.publish(8u); }},
      {"tail past a whole ring", [](GuestRing &ring) { ring.publish(16u + capacity + 1u); }},
      {"tail far away", [](GuestRing &ring) { ring.publish(~std::uint64_t{0}); }},
      {"batch magic", [](GuestRing &ring) { ring.write_batch(6u, 0u); ring.data(16u) = 0u; ring.publish(); }},
      {"fence zero", [](GuestRing &ring) { ring.write_batch(6u, 0u); ring.data(16u + 8u) = 0u; ring.publish(); }},
      {"fence repeated", [](GuestRing &ring) { ring.write_batch(5u, 0u); ring.publish(); }},
      {"fence lower", [](GuestRing &ring) { ring.write_batch(4u, 0u); ring.publish(); }},
      {"batch longer than the ring", [](GuestRing &ring) { ring.write_batch(6u, 0u); ring.data(16u + 6u) = 1u;
                                                           ring.publish(16u + capacity); }},
  };
  for (const Case &c : cases) {
      Fixture fixture;
      fixture.bind();
      fixture.ring().write_batch(5u, 0u);
      fixture.ring().publish();
      fixture.serve();
      CHECK_EQ(fixture.consumer().head(), 16u);
      c.spoil(fixture.ring());
      fixture.serve();
      if (!fixture.consumer().stopped()) av::TestRegistry::instance().record_failure(__FILE__, __LINE__, c.name);
      CHECK_EQ(fixture.host().logged(), 1u);
      CHECK_EQ(fixture.consumer().head(), 16u);
      CHECK_EQ(fixture.page().completed_fence(), 5u);
      CHECK_EQ(fixture.page().progress(), 1u);
      CHECK_EQ(fixture.host().raised(0u), 1u);
      static_cast<void>(RingHeader(12u, 0u, 0u).encode(fixture.ring().bytes(), 32u));
      fixture.ring().store(head_offset, 16u);
      fixture.serve();
      CHECK_EQ(fixture.host().logged(), 1u);
      CHECK_EQ(fixture.consumer().head(), 16u);
  } }

TEST(ring_consumer_service, a_ring_the_blob_cannot_hold_stops_without_reading_outside_it)
{ Fixture unreadable_header;
  unreadable_header.bind();
  std::uint8_t  short_bytes[8] = {};
  void         *short_pointer = short_bytes;
  std::uint64_t short_length = sizeof short_bytes;
  av::RingConsumerProbe::replace_ring(unreadable_header.consumer(),
                                      RingWindow(BlobWindow(&short_pointer, &short_length, 1u, 8u), 12u));
  unreadable_header.serve();
  CHECK(unreadable_header.consumer().stopped());
  Fixture unreadable_data;
  unreadable_data.bind();
  unreadable_data.ring().write_batch(1u, 0u);
  unreadable_data.ring().write_batch(2u, 0u);
  unreadable_data.ring().publish();
  void         *pointer = unreadable_data.ring().bytes();
  std::uint64_t length = 32u + 20u;
  av::RingConsumerProbe::replace_ring(unreadable_data.consumer(), RingWindow(BlobWindow(&pointer, &length, 1u, length), 12u));
  unreadable_data.serve();
  CHECK(unreadable_data.consumer().stopped());
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
