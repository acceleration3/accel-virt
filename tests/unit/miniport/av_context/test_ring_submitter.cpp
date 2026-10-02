/* Publishing a context's submissions into its ring: in order, within what the
 * device has not consumed, with a head the device cannot lie about. */
#include "miniport/av_context/ring_submitter.hpp"
#include "core/support/status.hpp"
#include "harness/device/fake_doorbell_pages.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "harness/platform/violation.hpp"
#include "proto/submission/fence_progress.hpp"
#include "proto/submission/ring_header.hpp"
#include "proto/submission/submission_descriptor.hpp"

#include <cstdint>
#include <cstring>

using av::FakeDoorbellPages;
using av::FenceProgress;
using av::Irql;
using av::MockPlatform;
using av::RingHeader;
using av::RingSubmitter;
using av::Status;
using av::SubmissionDescriptor;
using av::Violation;

namespace av {

struct RingSubmitterProbe {
    static void pass_tail(RingSubmitter &submitter) { submitter.published_ = 1u; submitter.tail_ = 0u; }
    static void rewind(RingSubmitter &submitter) { submitter.published_ = 0u; submitter.tail_ = 1u; }
};

}  // namespace av

namespace {

constexpr std::uint64_t capacity = 4096u;

class Fixture {
public:
    Fixture() : ring_{}, submitter_(platform_, doorbells_, fences_)
    {
        static_cast<void>(RingHeader(12u, 0u, 0u).encode(ring_, 32u));
        platform_.set_irql(Irql::passive);
    }

    MockPlatform      &platform() { return platform_; }
    FakeDoorbellPages &doorbells() { return doorbells_; }
    FenceProgress     &fences() { return fences_; }
    RingSubmitter     &submitter() { return submitter_; }
    std::uint8_t      *ring() { return ring_; }

    Status attach(std::uint32_t doorbell = 5u) { return submitter_.attach(ring_, 12u, doorbell); }
    std::uint64_t published_tail() const
    {
        std::uint64_t tail = 0u;
        std::memcpy(&tail, ring_ + 8u, 8u);
        return tail;
    }
    void device_consumes(std::uint64_t head) { std::memcpy(ring_ + 16u, &head, 8u); }
    /* What a process publishing its own tail writes into the header. */
    void publish_tail(std::uint64_t tail) { std::memcpy(ring_ + 8u, &tail, 8u); }
    void user_mode_publishes()
    {
        platform_.set_irql(Irql::dispatch);
        submitter_.user_mode_publishes();
        platform_.set_irql(Irql::passive);
    }
    Status submit(std::uint64_t fence, std::uint64_t begin, std::uint64_t end)
    {
        platform_.set_irql(Irql::dispatch);
        const Status status = submitter_.submit(SubmissionDescriptor(fence, begin, end));
        platform_.set_irql(Irql::passive);
        CHECK(submitter_.invariants_hold());
        return status;
    }

private:
    MockPlatform      platform_;
    FakeDoorbellPages doorbells_;
    FenceProgress     fences_;
    std::uint8_t      ring_[32u + capacity];
    RingSubmitter     submitter_;
};

}  // namespace

TEST(ring_submitter_submit, publishes_the_tail_and_rings_the_doorbell)
{ Fixture fixture;
  CHECK_STATUS(fixture.submit(1u, 0u, 16u), Status::invalid_state);   /* not attached */
  CHECK_STATUS(fixture.attach(), Status::ok);
  CHECK(fixture.submitter().invariants_hold());
  CHECK_STATUS(fixture.submit(1u, 0u, 64u), Status::ok);
  CHECK(fixture.published_tail() == 64u);
  CHECK_EQ(fixture.doorbells().rings(5u), 1u);
  CHECK_STATUS(fixture.submit(2u, 64u, 128u), Status::ok);
  CHECK(fixture.submitter().tail() == 128u);
  CHECK(fixture.fences().highest_submitted() == 2u);
  CHECK_EQ(fixture.doorbells().rings(5u), 2u);
  CHECK_EQ(fixture.doorbells().stray_stores(), 0u); }

TEST(ring_submitter_submit, a_submission_that_does_not_follow_the_tail_or_its_fence_is_refused_untouched)
{ Fixture fixture;
  CHECK_STATUS(fixture.attach(), Status::ok);
  CHECK_STATUS(fixture.submit(4u, 0u, 32u), Status::ok);
  struct Case {
      const char   *name;
      std::uint64_t fence;
      std::uint64_t begin;
      std::uint64_t end;
  };
  const Case cases[] = {
      {"a gap", 5u, 40u, 64u},
      {"rewinds", 5u, 0u, 64u},
      {"fence repeated", 4u, 32u, 64u},
      {"fence lower", 3u, 32u, 64u},
      {"longer than the ring", 5u, 32u, 32u + capacity + 1u},
  };
  for (const Case &c : cases) {
      if (fixture.submit(c.fence, c.begin, c.end) != Status::invalid_parameter)
          av::TestRegistry::instance().record_failure(__FILE__, __LINE__, c.name);
      CHECK(fixture.published_tail() == 32u);
  }
  CHECK_EQ(fixture.doorbells().rings(5u), 1u);
  CHECK(fixture.fences().highest_submitted() == 4u);
  CHECK(!fixture.submitter().failed()); }

TEST(ring_submitter_submit, bytes_the_device_has_not_consumed_hold_back_the_next_submission)
{ Fixture fixture;
  CHECK_STATUS(fixture.attach(), Status::ok);
  CHECK_STATUS(fixture.submit(1u, 0u, 3000u), Status::ok);
  CHECK_STATUS(fixture.submit(2u, 3000u, 4000u), Status::ok);
  CHECK_STATUS(fixture.submit(3u, 4000u, 4200u), Status::no_space);
  CHECK(fixture.published_tail() == 4000u);
  CHECK(fixture.fences().highest_submitted() == 2u);
  fixture.device_consumes(3000u);
  CHECK_STATUS(fixture.submit(3u, 4000u, 4200u), Status::ok);
  CHECK(fixture.submitter().head() == 3000u);
  CHECK_STATUS(fixture.submit(4u, 4200u, 3000u + capacity), Status::ok);   /* exactly full */
  CHECK_STATUS(fixture.submit(5u, 3000u + capacity, 3001u + capacity), Status::no_space); }

TEST(ring_submitter_submit, a_head_the_device_could_not_have_written_fails_the_ring)
{ struct Case {
      const char   *name;
      std::uint64_t head;
  };
  const Case cases[] = {{"past the tail", 1101u}, {"backwards", 100u}};
  for (const Case &c : cases) {
      Fixture fixture;
      CHECK_STATUS(fixture.attach(), Status::ok);
      CHECK_STATUS(fixture.submit(1u, 0u, 1000u), Status::ok);
      fixture.device_consumes(500u);
      CHECK_STATUS(fixture.submit(2u, 1000u, 1100u), Status::ok);
      fixture.device_consumes(c.head);
      if (fixture.submit(3u, 1100u, 1200u) != Status::invalid_state || !fixture.submitter().failed())
          av::TestRegistry::instance().record_failure(__FILE__, __LINE__, c.name);
      CHECK(fixture.published_tail() == 1100u);
      fixture.device_consumes(1100u);
      CHECK_STATUS(fixture.submit(3u, 1100u, 1200u), Status::invalid_state);   /* failed for good */
      CHECK_EQ(fixture.platform().logged(), 1u);
  } }

TEST(ring_submitter_attach, an_order_or_page_outside_the_protocol_is_refused)
{ Fixture fixture;
  CHECK(!fixture.submitter().attached());
  CHECK_STATUS(fixture.submitter().attach(fixture.ring(), 11u, 0u), Status::invalid_parameter);
  CHECK_STATUS(fixture.submitter().attach(fixture.ring(), 25u, 0u), Status::invalid_parameter);
  CHECK_STATUS(fixture.submitter().attach(nullptr, 12u, 0u), Status::invalid_parameter);
  CHECK_STATUS(fixture.submitter().attach(fixture.ring(), 12u, FakeDoorbellPages::pages), Status::invalid_parameter);
  CHECK_STATUS(fixture.attach(63u), Status::ok);
  CHECK(fixture.submitter().attached());
  CHECK_STATUS(fixture.attach(0u), Status::invalid_state);
  fixture.submitter().detach();
  CHECK_STATUS(fixture.submit(1u, 0u, 16u), Status::invalid_state);
  CHECK_STATUS(fixture.attach(0u), Status::ok);
  CHECK_STATUS(fixture.submit(1u, 0u, 16u), Status::ok);
  CHECK_EQ(fixture.doorbells().rings(0u), 1u); }

TEST(ring_submitter, attaches_and_submits_at_dispatch_at_most)
{ Fixture fixture;
  fixture.platform().set_irql(Irql::isr);
  CHECK_STATUS(fixture.attach(), Status::ok);
  fixture.submitter().detach();
  fixture.platform().set_irql(Irql::isr);
  CHECK_STATUS(fixture.submitter().submit(SubmissionDescriptor(1u, 0u, 1u)), Status::invalid_state);
  fixture.platform().set_irql(Irql::passive);
  for (int count = 0; count < 3; count++) CHECK(fixture.platform().claim(Violation::irql_ceiling)); }

TEST(ring_submitter_invariants, a_published_tail_the_driver_never_wrote_is_detected)
{ Fixture fixture;
  CHECK_STATUS(fixture.attach(), Status::ok);
  av::RingSubmitterProbe::pass_tail(fixture.submitter());
  CHECK(!fixture.submitter().invariants_hold());
  Fixture rewound;
  CHECK_STATUS(rewound.attach(), Status::ok);
  CHECK_STATUS(rewound.submit(1u, 0u, 64u), Status::ok);
  CHECK_STATUS(rewound.submit(2u, 64u, 128u), Status::ok);   /* the device's view now has tail 64 */
  av::RingSubmitterProbe::rewind(rewound.submitter());
  CHECK(!rewound.submitter().invariants_hold()); }

/* Feature 049: once a doorbell is connected the process publishes this ring's
 * tail, and a batch it falls back to the driver with is one the header already
 * ends at. The driver takes the submission's own beginning as its tail then -
 * and judges it, because the header is memory a process writes. */
TEST(ring_submitter_submit, a_tail_the_process_published_is_taken_and_judged)
{ Fixture fixture;
  CHECK_STATUS(fixture.attach(), Status::ok);
  CHECK_STATUS(fixture.submit(1u, 0u, 64u), Status::ok);
  fixture.user_mode_publishes();

  /* Four batches the process wrote and published for itself, and a fifth it
   * falls back to the driver with: the driver's own tail is 64, the ring's is
   * 320, and the submission is what closes the gap. */
  fixture.publish_tail(320u);
  CHECK_STATUS(fixture.submit(6u, 256u, 320u), Status::ok);
  CHECK(fixture.submitter().tail() == 320u);
  CHECK(fixture.fences().highest_submitted() == 6u);
  CHECK_EQ(fixture.doorbells().rings(5u), 2u);

  /* A header short of the submission's end, and not at its beginning, is no
   * header of this batch. */
  fixture.publish_tail(360u);
  CHECK_STATUS(fixture.submit(7u, 320u, 384u), Status::invalid_parameter);
  /* Nor is a batch that begins behind what the driver already published. */
  fixture.publish_tail(300u);
  CHECK_STATUS(fixture.submit(7u, 256u, 300u), Status::invalid_parameter);
  CHECK(fixture.submitter().tail() == 320u);
  CHECK_EQ(fixture.doorbells().rings(5u), 2u);

  /* A header past the end is a process that published later batches before
   * the scheduler handed this one over: the batch is taken, and the newer tail
   * is never stored back over. */
  fixture.publish_tail(400u);
  CHECK_STATUS(fixture.submit(7u, 320u, 384u), Status::ok);
  CHECK(fixture.submitter().tail() == 384u);
  CHECK_EQ(fixture.published_tail(), 400u);
  CHECK_EQ(fixture.doorbells().rings(5u), 3u);

  /* And the ring still fills up: a batch longer than the ring is refused,
   * however it was published. */
  fixture.publish_tail(384u + capacity + 1u);
  CHECK_STATUS(fixture.submit(8u, 384u, 384u + capacity + 1u), Status::invalid_parameter);
  CHECK(fixture.submitter().invariants_hold()); }

/* The driver's own tail is short of the one a process published, and the
 * store that ends the submission moves the header only while it is: a newer
 * tail the process stored meanwhile is left where it is. */
TEST(ring_submitter_submit, a_shared_tail_is_only_ever_moved_forward)
{ Fixture fixture;
  CHECK_STATUS(fixture.attach(), Status::ok);
  CHECK_STATUS(fixture.submit(1u, 0u, 64u), Status::ok);
  fixture.user_mode_publishes();
  /* A process that has stopped publishing for itself leaves the header where
   * the submission begins, and the driver writes the tail again. */
  fixture.publish_tail(64u);
  CHECK_STATUS(fixture.submit(2u, 64u, 128u), Status::ok);
  CHECK_EQ(fixture.published_tail(), 128u);
  CHECK_EQ(fixture.doorbells().rings(5u), 2u); }

/* A kick rings the doorbell with nothing submitted - the device looks at a
 * tail the process moved itself - and never on a ring that is not there or
 * has failed. */
TEST(ring_submitter_kick, rings_the_doorbell_of_an_attached_ring_only)
{ Fixture fixture;
  fixture.platform().set_irql(Irql::dispatch);
  fixture.submitter().kick();
  fixture.platform().set_irql(Irql::passive);
  CHECK_EQ(fixture.doorbells().rings(5u), 0u);
  CHECK_STATUS(fixture.attach(), Status::ok);
  fixture.platform().set_irql(Irql::dispatch);
  fixture.submitter().kick();
  fixture.submitter().kick();
  fixture.platform().set_irql(Irql::passive);
  CHECK_EQ(fixture.doorbells().rings(5u), 2u);
  CHECK(fixture.submitter().tail() == 0u);
  CHECK(fixture.fences().highest_submitted() == 0u);
  /* A head the device could not have written fails the ring; no kick after. */
  fixture.device_consumes(64u);
  CHECK_STATUS(fixture.submit(1u, 0u, 32u), Status::invalid_state);
  CHECK(fixture.submitter().failed());
  fixture.platform().set_irql(Irql::dispatch);
  fixture.submitter().kick();
  fixture.platform().set_irql(Irql::passive);
  CHECK_EQ(fixture.doorbells().rings(5u), 2u);
  CHECK_EQ(fixture.doorbells().stray_stores(), 0u); }

/* The tail goes out before the store (SubmissionPath::submit), so a batch a
 * process falls back to the driver with may be one the device has already
 * taken from the ring. It must run once: the driver publishes nothing, rings
 * nothing, and answers the caller as if it had. */
TEST(ring_submitter_submit, a_batch_the_device_already_took_is_not_published_again)
{ Fixture fixture;
  CHECK_STATUS(fixture.attach(), Status::ok);
  CHECK_STATUS(fixture.submit(1u, 0u, 64u), Status::ok);
  fixture.user_mode_publishes();
  /* The process published 128 and the device consumed all of it before the
   * fallback reached the driver. */
  fixture.publish_tail(128u);
  fixture.device_consumes(128u);
  CHECK_STATUS(fixture.submit(2u, 64u, 128u), Status::ok);
  CHECK_EQ(fixture.doorbells().rings(5u), 1u);          /* the first submission's, and no second */
  CHECK_EQ(fixture.published_tail(), 128u);
  CHECK(fixture.submitter().tail() == 128u);
  /* The fence was the device's to complete, not the driver's to record. */
  CHECK(fixture.fences().highest_submitted() == 1u);

  /* A batch it has not taken yet is published and rung as always, even where
   * the device is partway through the one before it. */
  fixture.publish_tail(192u);
  CHECK_STATUS(fixture.submit(3u, 128u, 192u), Status::ok);
  CHECK_EQ(fixture.doorbells().rings(5u), 2u);
  CHECK(fixture.fences().highest_submitted() == 3u);
  CHECK(fixture.submitter().invariants_hold()); }
