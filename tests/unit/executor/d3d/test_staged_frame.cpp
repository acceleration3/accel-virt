#include "device/abi/av_executor.h"
#include "executor/d3d/staged_frame.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>

using av::StagedFrame;

namespace {

class Frame final : public StagedFrame {
public:
    Frame(std::uint32_t width, std::uint32_t height) { size(width, height); }

    void becomes(Reading reading) { next_ = reading; }
    void admit_again() { readmit(); }
    bool is_forsaken() const { return forsaken(); }
    std::uint32_t wide() const { return width(); }
    std::uint32_t high() const { return height(); }
    std::uint32_t maps() const { return maps_; }
    std::uint32_t unmaps() const { return unmaps_; }

protected:
    Reading map_when_copied(const std::uint8_t *&pixels, std::uint32_t &row_pitch) override
    {
        maps_++;
        pixels = pixels_;
        row_pitch = 256u;
        return next_;
    }
    void unmap() override { unmaps_++; }

private:
    std::uint8_t  pixels_[16] = {};
    Reading       next_ = Reading::mapped;
    std::uint32_t maps_ = 0u;
    std::uint32_t unmaps_ = 0u;
};

}

TEST(staged_frame, a_frame_is_claimed_once_until_it_is_released)
{ Frame frame(64u, 32u);
  CHECK(!frame.out());
  CHECK(frame.claim());
  CHECK(frame.out());
  CHECK(!frame.claim());
  frame.release();
  CHECK(!frame.out());
  CHECK_EQ(frame.unmaps(), 1u);
  CHECK(frame.claim()); }

TEST(staged_frame, a_read_that_maps_hands_over_the_pixels_pitch_and_size)
{ Frame frame(64u, 32u);
  CHECK_EQ(frame.wide(), 64u);
  CHECK_EQ(frame.high(), 32u);
  AvFrameView view = {};
  view.reserved = 9u;
  CHECK(frame.read(view) == StagedFrame::Reading::mapped);
  CHECK(view.pixels != nullptr);
  CHECK_EQ(view.row_pitch, 256u);
  CHECK_EQ(view.width, 64u);
  CHECK_EQ(view.height, 32u);
  CHECK_EQ(view.reserved, 0u); }

TEST(staged_frame, a_read_that_is_still_copying_or_failed_leaves_the_view_alone)
{ Frame frame(64u, 32u);
  AvFrameView view = {};
  frame.becomes(StagedFrame::Reading::copying);
  CHECK(frame.read(view) == StagedFrame::Reading::copying);
  CHECK(view.pixels == nullptr);
  CHECK_EQ(view.width, 0u);
  frame.becomes(StagedFrame::Reading::failed);
  CHECK(frame.read(view) == StagedFrame::Reading::failed);
  CHECK(view.pixels == nullptr);
  CHECK_EQ(frame.maps(), 2u); }

TEST(staged_frame, a_forsaken_frame_fails_every_read_without_asking_the_copy_and_can_be_readmitted)
{ Frame frame(64u, 32u);
  CHECK(!frame.is_forsaken());
  frame.forsake();
  CHECK(frame.is_forsaken());
  AvFrameView view = {};
  CHECK(frame.read(view) == StagedFrame::Reading::failed);
  CHECK_EQ(frame.maps(), 0u);
  CHECK(view.pixels == nullptr);
  frame.admit_again();
  CHECK(!frame.is_forsaken());
  CHECK(frame.read(view) == StagedFrame::Reading::mapped);
  CHECK_EQ(frame.maps(), 1u); }
