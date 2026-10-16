/* The surfaces a GPU's sessions present and the frames they stage, judged
 * through what a writer is told and what the present engine reads: a surface
 * no blt has asked for is never staged until one asks; a frame is waited for
 * until its writer reaches it, read once, and given back once; what is kept
 * never exceeds its slots, and a frame being read is never taken away; every
 * surface is remembered until the guest lets go of it. */
#include "executor/d3d/shared_surfaces.hpp"
#include "executor/d3d/staged_frame.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "proto/resources/surface_point.hpp"

#include <cstdint>

using av::MockPlatform;
using av::SharedSurfaces;
using av::StagedFrame;
using av::SurfacePoint;

constexpr SharedSurfaces::Staging when_read = SharedSurfaces::Staging::once_blt_read;
constexpr SharedSurfaces::Staging every_present = SharedSurfaces::Staging::every_present;

namespace {

/* A frame whose reading is scripted: `copying` reads before it maps, or a
 * failure; every read and give-back counted. */
class FakeFrame final : public StagedFrame {
public:
    explicit FakeFrame(std::uint32_t copying = 0u) : copying_(copying), fails_(false), reads_(0u), releases_(0u) {}

    void fail() { fails_ = true; }
    Reading read(AvFrameView &view) override
    {
        reads_++;
        if (fails_) return Reading::failed;
        if (copying_ != 0u) {
            copying_--;
            return Reading::copying;
        }
        view = AvFrameView{pixels_, 8u, 2u, 1u, 0u};
        return Reading::mapped;
    }
    void release() override { releases_++; }

    std::uint32_t reads() const { return reads_; }
    std::uint32_t releases() const { return releases_; }
    const std::uint8_t *pixels() const { return pixels_; }

private:
    std::uint32_t copying_;
    bool          fails_;
    std::uint32_t reads_;
    std::uint32_t releases_;
    std::uint8_t  pixels_[8] = {};
};

constexpr std::uint64_t surface = 0x51u;

}  // namespace

/* The compositor reads a flip-model buffer itself, so no blt ever asks for
 * it: its writer is never told to stage. The first blt to ask for a point the
 * writer passed is refused at once, and from then on the writer stages. */
TEST(shared_surfaces, a_surface_is_staged_only_once_a_blt_has_asked_for_it)
{ MockPlatform   platform;
  SharedSurfaces surfaces(platform);
  CHECK(!surfaces.presented(SurfacePoint(surface, 1u), when_read));
  CHECK(!surfaces.presented(SurfacePoint(surface, 2u), when_read));
  AvFrameView view = {};
  CHECK(!surfaces.map(SurfacePoint(surface, 2u), 1000u, view));
  CHECK(view.pixels == nullptr);
  CHECK(surfaces.presented(SurfacePoint(surface, 3u), when_read));
  CHECK(!surfaces.presented(SurfacePoint(surface + 1u, 1u), when_read));   /* another surface learns for itself */
  CHECK(surfaces.invariants_hold()); }

/* A writer that knows a blt reads every present - Direct3D 9's windowed
 * presents, Direct3D 11's blt-model buffers - stages its surface's first one
 * too, and a blt asking for it before it is kept waits rather than being
 * refused: WPF presents a window that does not change exactly once, and a
 * refusal there left the window black. */
TEST(shared_surfaces, a_writer_staging_every_present_has_its_first_frame_waited_for)
{ MockPlatform   platform;
  SharedSurfaces surfaces(platform);
  FakeFrame      frame;
  AvFrameView    view = {};
  const SurfacePoint first(surface, 1u);
  CHECK(surfaces.presented(first, every_present));
  CHECK(!surfaces.map(first, 0u, view));   /* pending: one look, a wait */
  CHECK(surfaces.keep(first, frame));
  CHECK(surfaces.map(first, 0u, view));
  CHECK(view.pixels == frame.pixels());
  surfaces.unmap(first);
  CHECK(!surfaces.presented(SurfacePoint(surface, 2u), when_read));   /* the wait taught nothing */
  const SurfacePoint third(surface, 3u);
  CHECK(surfaces.presented(third, every_present));
  surfaces.hold(third, nullptr);   /* staging failed: refused at once */
  CHECK(!surfaces.map(third, 1000u, view));
  CHECK(surfaces.invariants_hold()); }

/* A frame kept is mapped once, refused to a second read while mapped, and
 * given back exactly once; after that its point is one passed with nothing
 * kept, which is refused without waiting. */
TEST(shared_surfaces, a_kept_frame_is_read_once_and_given_back_once)
{ MockPlatform   platform;
  SharedSurfaces surfaces(platform);
  FakeFrame      frame;
  const SurfacePoint at(surface, 4u);
  static_cast<void>(surfaces.presented(at, when_read));
  CHECK(surfaces.keep(at, frame));
  AvFrameView view = {};
  CHECK(surfaces.map(at, 0u, view));
  CHECK(view.pixels == frame.pixels());
  CHECK_EQ(view.width, 2u);
  AvFrameView again = {};
  CHECK(!surfaces.map(at, 0u, again));
  CHECK(again.pixels == nullptr);
  CHECK_EQ(frame.releases(), 0u);
  surfaces.unmap(at);
  CHECK_EQ(frame.releases(), 1u);
  surfaces.unmap(at);
  CHECK_EQ(frame.releases(), 1u);
  CHECK(!surfaces.map(at, 1000u, view));
  CHECK(surfaces.invariants_hold()); }

/* A frame the GPU is still copying is waited for; one that never will be is
 * given back and refused. */
TEST(shared_surfaces, a_copying_frame_is_waited_for_and_a_failed_one_given_back)
{ MockPlatform   platform;
  SharedSurfaces surfaces(platform);
  FakeFrame      copying(3u);
  const SurfacePoint first(surface, 1u);
  static_cast<void>(surfaces.presented(first, when_read));
  CHECK(surfaces.keep(first, copying));
  AvFrameView view = {};
  CHECK(surfaces.map(first, 1000u, view));
  CHECK_EQ(copying.reads(), 4u);
  surfaces.unmap(first);
  FakeFrame          broken;
  const SurfacePoint second(surface, 2u);
  broken.fail();
  static_cast<void>(surfaces.presented(second, when_read));
  CHECK(surfaces.keep(second, broken));
  CHECK(!surfaces.map(second, 1000u, view));
  CHECK_EQ(broken.reads(), 1u);
  CHECK_EQ(broken.releases(), 1u);
  CHECK(surfaces.invariants_hold()); }

/* A point the writer has not reached is waited for and teaches nothing; one
 * the writer was told to stage and has not yet kept is waited for rather
 * than refused, and one it abandoned is refused. */
TEST(shared_surfaces, a_point_not_reached_or_still_being_staged_is_waited_for)
{ MockPlatform   platform;
  SharedSurfaces surfaces(platform);
  AvFrameView   view = {};
  static_cast<void>(surfaces.presented(SurfacePoint(surface, 1u), when_read));
  CHECK(!surfaces.map(SurfacePoint(surface, 2u), 1u, view));   /* not reached: waits, then gives up */
  CHECK(!surfaces.presented(SurfacePoint(surface, 2u), when_read));        /* and learned nothing */
  CHECK(!surfaces.map(SurfacePoint(surface, 2u), 1000u, view)); /* passed: learned */
  CHECK(surfaces.presented(SurfacePoint(surface, 3u), when_read));
  CHECK(!surfaces.map(SurfacePoint(surface, 3u), 0u, view));    /* pending: one look, a wait */
  FakeFrame frame;
  CHECK(surfaces.keep(SurfacePoint(surface, 3u), frame));
  CHECK(surfaces.map(SurfacePoint(surface, 3u), 0u, view));
  surfaces.unmap(SurfacePoint(surface, 3u));
  CHECK(surfaces.presented(SurfacePoint(surface, 4u), when_read));
  surfaces.abandon(SurfacePoint(surface, 4u));
  CHECK(!surfaces.map(SurfacePoint(surface, 4u), 1000u, view));
  surfaces.abandon(SurfacePoint());
  CHECK(surfaces.invariants_hold()); }

/* What a writer staged is held: kept for a blt to read, given back when no
 * slot takes it, and a frame it could not stage is abandoned - a blt asking
 * for it is refused at once rather than left waiting. */
TEST(shared_surfaces, a_staged_frame_is_held_or_given_back_and_none_abandons)
{ MockPlatform   platform;
  SharedSurfaces surfaces(platform);
  AvFrameView   view = {};
  FakeFrame     kept, unkept;
  surfaces.hold(SurfacePoint(surface, 1u), &kept);
  CHECK(surfaces.map(SurfacePoint(surface, 1u), 0u, view));
  surfaces.unmap(SurfacePoint(surface, 1u));
  CHECK_EQ(kept.releases(), 1u);
  surfaces.hold(SurfacePoint(), &unkept);   /* names nothing: no slot takes it */
  CHECK_EQ(unkept.releases(), 1u);
  CHECK(!surfaces.presented(SurfacePoint(surface, 2u), when_read));          /* no blt has asked yet */
  CHECK(!surfaces.map(SurfacePoint(surface, 2u), 1000u, view));   /* passed: learned */
  CHECK(surfaces.presented(SurfacePoint(surface, 3u), when_read));
  surfaces.hold(SurfacePoint(surface, 3u), nullptr);
  CHECK(!surfaces.map(SurfacePoint(surface, 3u), 1000u, view));
  CHECK(surfaces.invariants_hold()); }

/* Keeping past the slots evicts the frame kept longest ago, never one being
 * read; with every slot being read nothing more is kept. A frame kept again
 * under the same point replaces the first, which is given back. */
TEST(shared_surfaces, keeping_past_its_slots_evicts_the_oldest_frame_not_being_read)
{ MockPlatform   platform;
  SharedSurfaces surfaces(platform);
  FakeFrame      frames[SharedSurfaces::max_frames + 2u];
  AvFrameView   view = {};
  for (std::uint32_t index = 0u; index < SharedSurfaces::max_frames; index++)
      CHECK(surfaces.keep(SurfacePoint(surface, index + 1u), frames[index]));
  CHECK(surfaces.map(SurfacePoint(surface, 1u), 0u, view));   /* the oldest, being read */
  CHECK(surfaces.keep(SurfacePoint(surface, SharedSurfaces::max_frames + 1u), frames[SharedSurfaces::max_frames]));
  CHECK_EQ(frames[0].releases(), 0u);
  CHECK_EQ(frames[1].releases(), 1u);
  FakeFrame replacement;
  CHECK(surfaces.keep(SurfacePoint(surface, 3u), replacement));
  CHECK_EQ(frames[2].releases(), 1u);
  for (std::uint32_t index = 2u; index < SharedSurfaces::max_frames; index++) {
      const std::uint32_t point = index + 1u;
      CHECK(surfaces.map(SurfacePoint(surface, point), 0u, view));
  }
  CHECK(surfaces.map(SurfacePoint(surface, SharedSurfaces::max_frames + 1u), 0u, view));
  FakeFrame late;
  CHECK(!surfaces.keep(SurfacePoint(surface, SharedSurfaces::max_frames + 2u), late));
  CHECK(!surfaces.keep(SurfacePoint(surface, 1u), late));   /* the same point, being read */
  CHECK_EQ(late.releases(), 0u);
  CHECK(!surfaces.keep(SurfacePoint(), late));
  CHECK(surfaces.invariants_hold()); }

/* The guest letting go of a surface gives back its frames not being read
 * and forgets what it learned; 0 does so for every surface. */
TEST(shared_surfaces, release_gives_back_a_surface_s_frames_and_forgets_it)
{ MockPlatform   platform;
  SharedSurfaces surfaces(platform);
  FakeFrame      kept, reading, other;
  AvFrameView   view = {};
  static_cast<void>(surfaces.presented(SurfacePoint(surface, 1u), when_read));
  CHECK(!surfaces.map(SurfacePoint(surface, 1u), 0u, view));   /* learned */
  CHECK(surfaces.keep(SurfacePoint(surface, 2u), kept));
  CHECK(surfaces.keep(SurfacePoint(surface, 3u), reading));
  CHECK(surfaces.keep(SurfacePoint(surface + 1u, 1u), other));
  CHECK(surfaces.map(SurfacePoint(surface, 3u), 0u, view));
  surfaces.release(surface);
  CHECK_EQ(kept.releases(), 1u);
  CHECK_EQ(reading.releases(), 0u);
  CHECK_EQ(other.releases(), 0u);
  CHECK(!surfaces.presented(SurfacePoint(surface, 4u), when_read));   /* forgotten: learns again */
  surfaces.unmap(SurfacePoint(surface, 3u));
  CHECK_EQ(reading.releases(), 1u);
  surfaces.release(0u);
  CHECK_EQ(other.releases(), 1u);
  CHECK(surfaces.invariants_hold()); }

/* The old table remembered 64 surfaces and forgot the one presented
 * longest ago: a desktop of many windows lost a frame each time a forgotten
 * one was read again. Now every surface keeps what it learned until the
 * guest lets go of it, and only then learns again. */
TEST(shared_surfaces, every_surface_remembers_what_it_learned_until_it_is_released)
{ MockPlatform   platform;
  SharedSurfaces surfaces(platform);
  AvFrameView    view = {};
  constexpr std::uint64_t many = 1000u;
  for (std::uint64_t one = 1u; one <= many; one++) {
      static_cast<void>(surfaces.presented(SurfacePoint(one, 1u), when_read));
      CHECK(!surfaces.map(SurfacePoint(one, 1u), 0u, view));   /* teaches it */
  }
  std::uint32_t staged = 0u;
  for (std::uint64_t one = 1u; one <= many; one++) staged += surfaces.presented(SurfacePoint(one, 2u), when_read);
  CHECK_EQ(staged, many);
  surfaces.release(7u);
  CHECK(!surfaces.presented(SurfacePoint(7u, 3u), when_read));   /* learns again */
  CHECK(surfaces.presented(SurfacePoint(8u, 3u), when_read));
  surfaces.release(0u);
  CHECK(!surfaces.presented(SurfacePoint(8u, 4u), when_read));
  CHECK(surfaces.invariants_hold()); }

/* A surface that cannot be remembered for want of memory stages every
 * present: a frame costs a copy rather than going unshown. */
TEST(shared_surfaces, a_surface_that_cannot_be_remembered_stages_every_present)
{ MockPlatform   platform;
  SharedSurfaces surfaces(platform);
  platform.fail_allocation_at(1u);
  CHECK(surfaces.presented(SurfacePoint(surface, 1u), when_read));
  CHECK(surfaces.invariants_hold());
  CHECK(!surfaces.presented(SurfacePoint(surface, 2u), when_read));
  CHECK(surfaces.invariants_hold()); }

/* Points count on past 2^32 as their difference does: a writer that wrapped
 * to 1 has passed the last point before the wrap. A frame naming nothing is
 * none of the table's business. */
TEST(shared_surfaces, points_compare_across_the_wrap_and_nothing_names_nothing)
{ MockPlatform   platform;
  SharedSurfaces surfaces(platform);
  AvFrameView   view = {};
  static_cast<void>(surfaces.presented(SurfacePoint(surface, 0xFFFFFFFFu), when_read));
  static_cast<void>(surfaces.presented(SurfacePoint(surface, 1u), when_read));
  CHECK(!surfaces.map(SurfacePoint(surface, 0xFFFFFFFFu), 1000u, view));
  CHECK(surfaces.presented(SurfacePoint(surface, 2u), when_read));   /* that ask was a passed point's */
  CHECK(!surfaces.presented(SurfacePoint(), when_read));
  CHECK(!surfaces.presented(SurfacePoint(surface, 0u), when_read));
  CHECK(!surfaces.map(SurfacePoint(), 1000u, view));
  surfaces.unmap(SurfacePoint());
  CHECK(surfaces.invariants_hold()); }
