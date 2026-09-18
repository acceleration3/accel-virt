/* The last frame presented on a window, read back as the front buffer
 * (feature 051, US2 edge case): this driver's swap already leaves those pixels
 * in the guest, so a read of GL_FRONT is answered without a host round trip -
 * for what a present produced, and for nothing else. */
#include "harness/framework/test_registry.hpp"
#include "icd/opengl/front_buffer.hpp"

#include <cstdint>
#include <cstring>

using av::glicd::FrontBuffer;

namespace {

/* Four pixels: a 2x2 frame, BGRA, bottom row first. */
constexpr std::uint8_t frame[] = {
    1u, 2u, 3u, 4u,      5u, 6u, 7u, 8u,        /* bottom row */
    9u, 10u, 11u, 12u,   13u, 14u, 15u, 16u,    /* top row */
};

}  // namespace

TEST(front_buffer, nothing_presented_answers_nothing)
{ FrontBuffer front;
  CHECK(front.invariants_hold());
  CHECK(!front.presented());
  CHECK_EQ(front.width(), 0u);
  CHECK_EQ(front.height(), 0u);
  std::uint8_t pixels[16] = {};
  CHECK(!front.read(0, 0, 2, 2, FrontBuffer::gl_bgra, FrontBuffer::gl_unsigned_byte, pixels)); }

TEST(front_buffer, the_presented_frame_is_answered_in_the_order_it_was_kept)
{ FrontBuffer front;
  front.presented(frame, 2u, 2u);
  CHECK(front.invariants_hold());
  CHECK(front.presented());
  CHECK_EQ(front.width(), 2u);
  CHECK_EQ(front.height(), 2u);
  std::uint8_t pixels[16] = {};
  CHECK(front.read(0, 0, 2, 2, FrontBuffer::gl_bgra, FrontBuffer::gl_unsigned_byte, pixels));
  CHECK(std::memcmp(pixels, frame, sizeof frame) == 0);
  /* GL_RGBA asks for the two ends swapped. */
  CHECK(front.read(0, 0, 2, 2, FrontBuffer::gl_rgba, FrontBuffer::gl_unsigned_byte, pixels));
  CHECK_EQ(pixels[0], 3u);
  CHECK_EQ(pixels[1], 2u);
  CHECK_EQ(pixels[2], 1u);
  CHECK_EQ(pixels[3], 4u); }

TEST(front_buffer, a_rectangle_inside_the_frame_is_answered_from_where_it_is)
{ FrontBuffer front;
  front.presented(frame, 2u, 2u);
  std::uint8_t pixel[4] = {};
  CHECK(front.read(1, 1, 1, 1, FrontBuffer::gl_bgra, FrontBuffer::gl_unsigned_byte, pixel));
  CHECK_EQ(pixel[0], 13u);
  CHECK_EQ(pixel[3], 16u);
  CHECK(front.read(0, 1, 1, 1, FrontBuffer::gl_bgra, FrontBuffer::gl_unsigned_byte, pixel));
  CHECK_EQ(pixel[0], 9u); }

/* Hostile: the rectangle and the format are the application's, and a read this
 * cannot answer is one the host answers instead - never a read past the frame. */
TEST(front_buffer, a_read_it_cannot_answer_writes_nothing)
{ FrontBuffer front;
  front.presented(frame, 2u, 2u);
  std::uint8_t pixels[64];
  std::memset(pixels, 0xEE, sizeof pixels);
  const std::int32_t outside[][4] = {{-1, 0, 1, 1}, {0, -1, 1, 1}, {0, 0, 3, 1}, {0, 0, 1, 3},
                                     {2, 0, 1, 1},  {0, 2, 1, 1},  {0, 0, 0, 1}, {0, 0, 1, 0},
                                     {0, 0, -1, -1}, {1, 1, 2, 2}};
  for (const auto &rectangle : outside)
      CHECK(!front.read(rectangle[0], rectangle[1], rectangle[2], rectangle[3], FrontBuffer::gl_bgra,
                        FrontBuffer::gl_unsigned_byte, pixels));
  /* A format or type no present produced. */
  CHECK(!front.read(0, 0, 2, 2, 0x1907u /* GL_RGB */, FrontBuffer::gl_unsigned_byte, pixels));
  CHECK(!front.read(0, 0, 2, 2, FrontBuffer::gl_bgra, 0x1406u /* GL_FLOAT */, pixels));
  /* Nowhere to write. */
  CHECK(!front.read(0, 0, 2, 2, FrontBuffer::gl_bgra, FrontBuffer::gl_unsigned_byte, nullptr));
  for (const std::uint8_t byte : pixels) CHECK_EQ(byte, 0xEEu); }

TEST(front_buffer, a_frame_given_back_is_forgotten)
{ FrontBuffer front;
  front.presented(frame, 2u, 2u);
  front.forget();
  CHECK(front.invariants_hold());
  CHECK(!front.presented());
  std::uint8_t pixels[16] = {};
  CHECK(!front.read(0, 0, 2, 2, FrontBuffer::gl_bgra, FrontBuffer::gl_unsigned_byte, pixels));
  /* A present of nothing is the same as none. */
  front.presented(nullptr, 2u, 2u);
  CHECK(!front.presented());
  front.presented(frame, 0u, 2u);
  CHECK(!front.presented());
  front.presented(frame, 2u, 0u);
  CHECK(!front.presented());
  CHECK(front.invariants_hold()); }
