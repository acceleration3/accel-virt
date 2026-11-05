#include "executor/d3d/frame_stager.hpp"
#include "executor/d3d/shared_surfaces.hpp"
#include "executor/d3d/staged_frame.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "proto/resources/surface_point.hpp"

#include <cstdint>

using av::FrameStager;
using av::MockPlatform;
using av::SharedSurfaces;
using av::StagedFrame;
using av::SurfacePoint;

constexpr SharedSurfaces::Staging when_read = SharedSurfaces::Staging::once_blt_read;
constexpr SharedSurfaces::Staging every_present = SharedSurfaces::Staging::every_present;

namespace {

class FakeFrame final : public StagedFrame {
public:
    explicit FakeFrame(std::uint32_t copying = 0u) : copying_(copying), fails_(false), reads_(0u), releases_(0u)
    {
        size(2u, 1u);
    }

    void fail() { fails_ = true; }

    std::uint32_t       reads() const { return reads_; }
    std::uint32_t       releases() const { return releases_; }
    const std::uint8_t *pixels() const { return pixels_; }

private:
    Reading map_when_copied(const std::uint8_t *&pixels, std::uint32_t &row_pitch) override
    {
        reads_++;
        if (fails_) return Reading::failed;
        if (copying_ != 0u) {
            copying_--;
            return Reading::copying;
        }
        pixels = pixels_;
        row_pitch = 8u;
        return Reading::mapped;
    }
    void unmap() override { releases_++; }

    std::uint32_t copying_;
    bool          fails_;
    std::uint32_t reads_;
    std::uint32_t releases_;
    std::uint8_t  pixels_[8] = {};
};

class Stager final : public FrameStager {
public:
    Stager() : next_(nullptr), stages_(0u) {}

    void give(StagedFrame *frame) { next_ = frame; }
    StagedFrame *stage() override
    {
        stages_++;
        StagedFrame *const frame = next_;
        next_ = nullptr;
        return frame;
    }

    std::uint32_t stages() const { return stages_; }

private:
    StagedFrame  *next_;
    std::uint32_t stages_;
};

constexpr std::uint64_t surface = 0x51u;

}

TEST(shared_surfaces, a_surface_is_staged_only_once_a_blt_has_asked_for_it)
{ MockPlatform   platform;
  SharedSurfaces surfaces(platform);
  Stager         stager;
  surfaces.stage(SurfacePoint(surface, 1u), when_read, stager);
  surfaces.stage(SurfacePoint(surface, 2u), when_read, stager);
  CHECK_EQ(stager.stages(), 0u);
  AvFrameView view = {};
  CHECK(!surfaces.map(SurfacePoint(surface, 2u), 1000u, view));
  CHECK(view.pixels == nullptr);
  surfaces.stage(SurfacePoint(surface, 3u), when_read, stager);
  CHECK_EQ(stager.stages(), 1u);
  surfaces.stage(SurfacePoint(surface + 1u, 1u), when_read, stager);
  CHECK_EQ(stager.stages(), 1u);
  CHECK(surfaces.invariants_hold()); }

TEST(shared_surfaces, a_writer_staging_every_present_has_every_frame_kept_and_given_back_when_read)
{ MockPlatform   platform;
  SharedSurfaces surfaces(platform);
  Stager         stager;
  FakeFrame      frame;
  AvFrameView    view = {};
  const SurfacePoint first(surface, 1u);
  stager.give(&frame);
  surfaces.stage(first, every_present, stager);
  CHECK_EQ(stager.stages(), 1u);
  CHECK(!surfaces.map(SurfacePoint(surface, 2u), 0u, view));
  CHECK(surfaces.map(first, 0u, view));
  CHECK(view.pixels == frame.pixels());
  surfaces.unmap(first);
  CHECK_EQ(frame.releases(), 1u);
  surfaces.stage(SurfacePoint(surface, 2u), when_read, stager);
  CHECK_EQ(stager.stages(), 1u);
  const SurfacePoint third(surface, 3u);
  surfaces.stage(third, every_present, stager);
  CHECK_EQ(stager.stages(), 2u);
  CHECK(!surfaces.map(third, 1000u, view));
  CHECK(surfaces.invariants_hold()); }

TEST(shared_surfaces, a_kept_frame_is_read_once_and_given_back_once)
{ MockPlatform   platform;
  SharedSurfaces surfaces(platform);
  Stager         stager;
  FakeFrame      frame;
  const SurfacePoint at(surface, 4u);
  stager.give(&frame);
  surfaces.stage(at, every_present, stager);
  AvFrameView view = {};
  CHECK(surfaces.map(at, 0u, view));
  CHECK(view.pixels == frame.pixels());
  CHECK_EQ(view.width, 2u);
  CHECK_EQ(view.height, 1u);
  CHECK_EQ(view.row_pitch, 8u);
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

TEST(shared_surfaces, a_copying_frame_is_waited_for_and_a_failed_one_given_back)
{ MockPlatform   platform;
  SharedSurfaces surfaces(platform);
  Stager         stager;
  FakeFrame      copying(3u);
  const SurfacePoint first(surface, 1u);
  stager.give(&copying);
  surfaces.stage(first, every_present, stager);
  AvFrameView view = {};
  CHECK(surfaces.map(first, 1000u, view));
  CHECK_EQ(copying.reads(), 4u);
  surfaces.unmap(first);
  FakeFrame          broken;
  const SurfacePoint second(surface, 2u);
  broken.fail();
  stager.give(&broken);
  surfaces.stage(second, every_present, stager);
  CHECK(!surfaces.map(second, 1000u, view));
  CHECK_EQ(broken.reads(), 1u);
  CHECK_EQ(broken.releases(), 1u);
  CHECK(surfaces.invariants_hold()); }

TEST(shared_surfaces, a_point_not_reached_or_still_being_staged_is_waited_for)
{ MockPlatform   platform;
  SharedSurfaces surfaces(platform);
  Stager         stager;
  AvFrameView    view = {};
  surfaces.stage(SurfacePoint(surface, 1u), when_read, stager);
  CHECK(!surfaces.map(SurfacePoint(surface, 2u), 1u, view));
  surfaces.stage(SurfacePoint(surface, 2u), when_read, stager);
  CHECK_EQ(stager.stages(), 0u);
  CHECK(!surfaces.map(SurfacePoint(surface, 2u), 1000u, view));
  FakeFrame frame;
  stager.give(&frame);
  surfaces.stage(SurfacePoint(surface, 3u), when_read, stager);
  CHECK_EQ(stager.stages(), 1u);
  CHECK(surfaces.map(SurfacePoint(surface, 3u), 0u, view));
  surfaces.unmap(SurfacePoint(surface, 3u));
  CHECK_EQ(frame.releases(), 1u);
  surfaces.stage(SurfacePoint(surface, 4u), when_read, stager);
  CHECK_EQ(stager.stages(), 2u);
  CHECK(!surfaces.map(SurfacePoint(surface, 4u), 1000u, view));
  CHECK(surfaces.invariants_hold()); }

TEST(shared_surfaces, a_stager_that_makes_no_frame_leaves_the_point_to_be_given_up_on)
{ MockPlatform   platform;
  SharedSurfaces surfaces(platform);
  Stager         stager;
  AvFrameView    view = {};
  surfaces.stage(SurfacePoint(surface, 1u), every_present, stager);
  CHECK_EQ(stager.stages(), 1u);
  CHECK(!surfaces.map(SurfacePoint(surface, 1u), 1000u, view));
  surfaces.stage(SurfacePoint(), every_present, stager);
  CHECK_EQ(stager.stages(), 1u);
  CHECK(surfaces.invariants_hold()); }

TEST(shared_surfaces, staging_past_its_slots_evicts_the_oldest_frame_not_being_read)
{ MockPlatform   platform;
  SharedSurfaces surfaces(platform);
  Stager         stager;
  FakeFrame      frames[SharedSurfaces::max_frames + 1u];
  AvFrameView    view = {};
  for (std::uint32_t index = 0u; index < SharedSurfaces::max_frames; index++) {
      stager.give(&frames[index]);
      surfaces.stage(SurfacePoint(surface, index + 1u), every_present, stager);
  }
  CHECK(surfaces.map(SurfacePoint(surface, 1u), 0u, view));
  stager.give(&frames[SharedSurfaces::max_frames]);
  surfaces.stage(SurfacePoint(surface, SharedSurfaces::max_frames + 1u), every_present, stager);
  CHECK_EQ(frames[0].releases(), 0u);
  CHECK_EQ(frames[1].releases(), 1u);
  FakeFrame replacement;
  stager.give(&replacement);
  surfaces.stage(SurfacePoint(surface, 3u), every_present, stager);
  CHECK_EQ(frames[2].releases(), 1u);
  CHECK(!surfaces.map(SurfacePoint(surface, 2u), 0u, view));
  for (std::uint32_t point = 3u; point <= SharedSurfaces::max_frames + 1u; point++)
      CHECK(surfaces.map(SurfacePoint(surface, point), 0u, view));
  FakeFrame late, repeated;
  stager.give(&late);
  surfaces.stage(SurfacePoint(surface, SharedSurfaces::max_frames + 2u), every_present, stager);
  CHECK_EQ(late.releases(), 1u);
  stager.give(&repeated);
  surfaces.stage(SurfacePoint(surface, 1u), every_present, stager);
  CHECK_EQ(repeated.releases(), 1u);
  CHECK_EQ(frames[0].releases(), 0u);
  CHECK(surfaces.invariants_hold()); }

TEST(shared_surfaces, release_gives_back_a_surface_s_frames_and_forgets_it)
{ MockPlatform   platform;
  SharedSurfaces surfaces(platform);
  Stager         stager;
  FakeFrame      kept, reading, other;
  AvFrameView    view = {};
  surfaces.stage(SurfacePoint(surface, 1u), when_read, stager);
  CHECK(!surfaces.map(SurfacePoint(surface, 1u), 0u, view));
  stager.give(&kept);
  surfaces.stage(SurfacePoint(surface, 2u), every_present, stager);
  stager.give(&reading);
  surfaces.stage(SurfacePoint(surface, 3u), every_present, stager);
  stager.give(&other);
  surfaces.stage(SurfacePoint(surface + 1u, 1u), every_present, stager);
  CHECK(surfaces.map(SurfacePoint(surface, 3u), 0u, view));
  surfaces.release(surface);
  CHECK_EQ(kept.releases(), 1u);
  CHECK_EQ(reading.releases(), 0u);
  CHECK_EQ(other.releases(), 0u);
  const std::uint32_t staged = stager.stages();
  surfaces.stage(SurfacePoint(surface, 4u), when_read, stager);
  CHECK_EQ(stager.stages(), staged);
  surfaces.unmap(SurfacePoint(surface, 3u));
  CHECK_EQ(reading.releases(), 1u);
  surfaces.release(0u);
  CHECK_EQ(other.releases(), 1u);
  CHECK(surfaces.invariants_hold()); }

TEST(shared_surfaces, every_surface_remembers_what_it_learned_until_it_is_released)
{ MockPlatform   platform;
  SharedSurfaces surfaces(platform);
  Stager         stager;
  AvFrameView    view = {};
  constexpr std::uint64_t many = 1000u;
  for (std::uint64_t one = 1u; one <= many; one++) {
      surfaces.stage(SurfacePoint(one, 1u), when_read, stager);
      CHECK(!surfaces.map(SurfacePoint(one, 1u), 0u, view));
  }
  CHECK_EQ(stager.stages(), 0u);
  for (std::uint64_t one = 1u; one <= many; one++) surfaces.stage(SurfacePoint(one, 2u), when_read, stager);
  CHECK_EQ(stager.stages(), many);
  surfaces.release(7u);
  surfaces.stage(SurfacePoint(7u, 3u), when_read, stager);
  CHECK_EQ(stager.stages(), many);
  surfaces.stage(SurfacePoint(8u, 3u), when_read, stager);
  CHECK_EQ(stager.stages(), many + 1u);
  surfaces.release(0u);
  surfaces.stage(SurfacePoint(8u, 4u), when_read, stager);
  CHECK_EQ(stager.stages(), many + 1u);
  CHECK(surfaces.invariants_hold()); }

TEST(shared_surfaces, a_surface_that_cannot_be_remembered_stages_every_present)
{ MockPlatform   platform;
  SharedSurfaces surfaces(platform);
  Stager         stager;
  platform.fail_allocation_at(1u);
  surfaces.stage(SurfacePoint(surface, 1u), when_read, stager);
  CHECK_EQ(stager.stages(), 1u);
  CHECK(surfaces.invariants_hold());
  surfaces.stage(SurfacePoint(surface, 2u), when_read, stager);
  CHECK_EQ(stager.stages(), 1u);
  CHECK(surfaces.invariants_hold()); }

TEST(shared_surfaces, points_compare_across_the_wrap_and_nothing_names_nothing)
{ MockPlatform   platform;
  SharedSurfaces surfaces(platform);
  Stager         stager;
  AvFrameView    view = {};
  surfaces.stage(SurfacePoint(surface, 0xFFFFFFFFu), when_read, stager);
  surfaces.stage(SurfacePoint(surface, 1u), when_read, stager);
  CHECK(!surfaces.map(SurfacePoint(surface, 0xFFFFFFFFu), 1000u, view));
  surfaces.stage(SurfacePoint(surface, 2u), when_read, stager);
  CHECK_EQ(stager.stages(), 1u);
  surfaces.stage(SurfacePoint(), when_read, stager);
  surfaces.stage(SurfacePoint(surface, 0u), when_read, stager);
  CHECK_EQ(stager.stages(), 1u);
  CHECK(!surfaces.map(SurfacePoint(), 1000u, view));
  surfaces.unmap(SurfacePoint());
  CHECK(surfaces.invariants_hold()); }
