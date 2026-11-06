#include "harness/framework/test_registry.hpp"
#include "icd/common/handed_frame.hpp"

#include <cstdint>

namespace {

struct Owner {
    int shown = 0;
};

struct Stamp {
    std::uint32_t serial = 0u;
};

using Frame = av::icd::HandedFrame<Owner, Stamp>;

}

TEST(handed_frame, a_default_frame_is_owned_by_nobody_and_is_not_chained)
{ const Frame frame;
  CHECK(frame.owner() == nullptr);
  CHECK(frame.pixels() == nullptr);
  CHECK(frame.window() == nullptr);
  CHECK_EQ(frame.slot(), 0u);
  CHECK_EQ(frame.width(), 0u);
  CHECK_EQ(frame.height(), 0u);
  CHECK_EQ(frame.mark().serial, 0u);
  CHECK(frame.rows() == Frame::Rows::bgra_bottom_up);
  CHECK(!frame.chained()); }

TEST(handed_frame, a_frame_keeps_what_it_was_handed)
{ Owner               owner;
  const std::uint8_t  pixels[16] = {};
  int                 window = 0;
  const Frame         frame(owner, 3u, pixels, Frame::Rows::rgba_top_down, 640u, 480u, &window, Stamp{9u});
  CHECK(frame.owner() == &owner);
  CHECK_EQ(frame.slot(), 3u);
  CHECK(frame.pixels() == pixels);
  CHECK(frame.rows() == Frame::Rows::rgba_top_down);
  CHECK_EQ(frame.width(), 640u);
  CHECK_EQ(frame.height(), 480u);
  CHECK(frame.window() == &window);
  CHECK_EQ(frame.mark().serial, 9u);
  CHECK(!frame.chained()); }

TEST(handed_frame, an_owned_frame_with_no_pixels_is_chained)
{ Owner       owner;
  const Frame chained(owner, 1u, nullptr, Frame::Rows::bgra_top_down, 8u, 8u, nullptr, Stamp{});
  CHECK(chained.chained()); }
