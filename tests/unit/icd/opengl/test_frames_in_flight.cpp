/* How far ahead of the host an application runs: the end of each frame names
 * the frame FramesInFlight::depth before it, which must be finished first. */
#include "harness/framework/test_registry.hpp"
#include "icd/opengl/frames_in_flight.hpp"

#include <cstdint>

using av::glicd::FramesInFlight;

namespace av::glicd {

struct FramesInFlightProbe {
    static void misorder(FramesInFlight &frames) { frames.ends_[frames.oldest_] = 9u; }
    static void lose_place(FramesInFlight &frames) { frames.oldest_ = FramesInFlight::depth; }
};

}  // namespace av::glicd

TEST(frames_in_flight_end, the_first_frames_wait_for_nothing)
{ FramesInFlight frames;
  for (std::uint64_t frame = 1u; frame <= FramesInFlight::depth; frame++) {
      CHECK_EQ(frames.end(frame * 10u), 0u);
      CHECK(frames.invariants_hold());
  } }

TEST(frames_in_flight_end, each_frame_waits_for_the_one_depth_before_it)
{ FramesInFlight frames;
  const std::uint64_t ends[] = {3u, 7u, 8u, 15u, 15u, 40u};
  for (std::uint32_t frame = 0u; frame < 6u; frame++) {
      const std::uint64_t waited = frames.end(ends[frame]);
      CHECK_EQ(waited, frame < FramesInFlight::depth ? 0u : ends[frame - FramesInFlight::depth]);
      CHECK(frames.invariants_hold());
  } }

/* A context made again numbers its batches from 1: what the old one ended
 * with is no fence of the new. */
TEST(frames_in_flight_forget, a_context_made_again_waits_for_nothing_from_before)
{ FramesInFlight frames;
  static_cast<void>(frames.end(50u));
  static_cast<void>(frames.end(60u));
  frames.forget();
  CHECK(frames.invariants_hold());
  CHECK_EQ(frames.end(1u), 0u);
  CHECK_EQ(frames.end(2u), 0u);
  CHECK_EQ(frames.end(3u), 1u); }

TEST(frames_in_flight_invariants, frames_out_of_order_or_a_lost_place_are_detected)
{ FramesInFlight misordered;
  static_cast<void>(misordered.end(4u));
  static_cast<void>(misordered.end(5u));
  av::glicd::FramesInFlightProbe::misorder(misordered);
  CHECK(!misordered.invariants_hold());
  FramesInFlight lost;
  av::glicd::FramesInFlightProbe::lose_place(lost);
  CHECK(!lost.invariants_hold()); }
