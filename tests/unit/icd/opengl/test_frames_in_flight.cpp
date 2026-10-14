/* How far ahead of the host an application runs: a frame ending waits for
 * the frames that ended FramesInFlight::latency_ms ago or more and are still
 * unfinished, and for the oldest once FramesInFlight::most are. */
#include "harness/framework/test_registry.hpp"
#include "icd/opengl/frames_in_flight.hpp"

#include <cstdint>

using av::glicd::FramesInFlight;

namespace av::glicd {

struct FramesInFlightProbe {
    static void misorder(FramesInFlight &frames) { frames.fences_[frames.oldest_] = 99u; }
    static void lose_place(FramesInFlight &frames) { frames.oldest_ = FramesInFlight::most; }
    static void overcount(FramesInFlight &frames) { frames.count_ = FramesInFlight::most + 1u; }
};

}  // namespace av::glicd

namespace {

/* A counter of a tick a millisecond, so ticks read as milliseconds. */
constexpr std::uint64_t per_second = 1000u;

}  // namespace

/* A host that keeps up is never waited for, however many frames end. */
TEST(frames_in_flight_end, a_host_that_keeps_up_is_never_waited_for)
{ FramesInFlight frames;
  for (std::uint64_t frame = 1u; frame <= 500u; frame++) {
      CHECK_EQ(frames.end(frame, frame * 10u, per_second, frame - 1u), 0u);
      CHECK(frames.invariants_hold());
  }
  CHECK_EQ(frames.unfinished(), 1u); }

/* A host behind by more than the latency is waited for, up to the newest
 * frame that old - not the frames younger than it. */
TEST(frames_in_flight_end, waits_for_the_newest_frame_older_than_the_latency)
{ FramesInFlight frames;
  CHECK_EQ(frames.end(1u, 0u, per_second, 0u), 0u);
  CHECK_EQ(frames.end(2u, 20u, per_second, 0u), 0u);
  CHECK_EQ(frames.end(3u, 40u, per_second, 0u), 0u);
  CHECK_EQ(frames.end(4u, 49u, per_second, 0u), 0u);   /* the first is 49 ms old */
  CHECK_EQ(frames.end(5u, 70u, per_second, 0u), 2u);   /* 1 and 2 are 50 ms old or more */
  CHECK_EQ(frames.unfinished(), 5u);
  /* The host finished through 3: those are forgotten, and 4 is not old enough. */
  CHECK_EQ(frames.end(6u, 98u, per_second, 3u), 0u);
  CHECK_EQ(frames.unfinished(), 3u);
  CHECK_EQ(frames.end(7u, 99u, per_second, 3u), 4u);
  CHECK(frames.invariants_hold()); }

/* With every remembered frame unfinished, the oldest is waited for however
 * young it is. */
TEST(frames_in_flight_end, the_oldest_is_waited_for_once_every_frame_is_unfinished)
{ FramesInFlight frames;
  for (std::uint64_t frame = 1u; frame <= FramesInFlight::most; frame++)
      CHECK_EQ(frames.end(frame, 0u, per_second, 0u), 0u);
  CHECK_EQ(frames.end(FramesInFlight::most + 1u, 0u, per_second, 0u), 1u);
  CHECK_EQ(frames.unfinished(), FramesInFlight::most);
  CHECK(frames.invariants_hold()); }

/* A frame that sent nothing the host has not already finished is no frame in
 * flight. */
TEST(frames_in_flight_end, a_frame_already_finished_is_not_remembered)
{ FramesInFlight frames;
  CHECK_EQ(frames.end(0u, 0u, per_second, 0u), 0u);
  CHECK_EQ(frames.end(5u, 100u, per_second, 5u), 0u);
  CHECK_EQ(frames.unfinished(), 0u); }

/* A context made again numbers its batches from 1: what the old one ended
 * with is no fence of the new. */
TEST(frames_in_flight_forget, a_context_made_again_waits_for_nothing_from_before)
{ FramesInFlight frames;
  static_cast<void>(frames.end(50u, 0u, per_second, 0u));
  frames.forget();
  CHECK_EQ(frames.unfinished(), 0u);
  CHECK(frames.invariants_hold());
  CHECK_EQ(frames.end(1u, 1000u, per_second, 0u), 0u); }

TEST(frames_in_flight_invariants, frames_out_of_order_or_a_lost_place_are_detected)
{ FramesInFlight misordered;
  static_cast<void>(misordered.end(4u, 0u, per_second, 0u));
  static_cast<void>(misordered.end(5u, 1u, per_second, 0u));
  av::glicd::FramesInFlightProbe::misorder(misordered);
  CHECK(!misordered.invariants_hold());
  FramesInFlight lost;
  av::glicd::FramesInFlightProbe::lose_place(lost);
  CHECK(!lost.invariants_hold());
  FramesInFlight over;
  av::glicd::FramesInFlightProbe::overcount(over);
  CHECK(!over.invariants_hold()); }
