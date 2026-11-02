#include "device/memory/blob_window.hpp"
#include "device/memory/window_cursor.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>
#include <cstring>

using av::BlobWindow;
using av::Status;
using av::WindowCursor;

namespace {
class Extents {
public:
    Extents()
    {
        for (std::uint8_t at = 0u; at < 5u; at++) first_[at] = at;
        for (std::uint8_t at = 0u; at < 3u; at++) second_[at] = static_cast<std::uint8_t>(5u + at);
        for (std::uint8_t at = 0u; at < 8u; at++) third_[at] = static_cast<std::uint8_t>(8u + at);
    }
    BlobWindow window(std::uint64_t claimed = 16u) const { return BlobWindow(pointers_, lengths_, 4u, claimed); }
    std::uint8_t byte(std::uint32_t at) const
    {
        if (at < 5u) return first_[at];
        return at < 8u ? second_[at - 5u] : third_[at - 8u];
    }

private:
    std::uint8_t   first_[5] = {};
    std::uint8_t   empty_[1] = {};
    std::uint8_t   second_[3] = {};
    std::uint8_t   third_[8] = {};
    void          *pointers_[4] = {first_, empty_, second_, third_};
    std::uint64_t  lengths_[4] = {5u, 0u, 3u, 8u};
};

}

TEST(window_cursor, reads_a_range_across_extents_in_order)
{ Extents      extents;
  WindowCursor cursor(extents.window());
  std::uint8_t bytes[11] = {};
  CHECK_STATUS(cursor.read(3u, bytes, sizeof bytes), Status::ok);
  for (std::uint32_t at = 0u; at < sizeof bytes; at++) CHECK_EQ(bytes[at], 3u + at);
  CHECK_STATUS(cursor.read(15u, bytes, 1u), Status::ok);
  CHECK_EQ(bytes[0], 15u); }

TEST(window_cursor, an_offset_before_the_last_starts_over)
{ Extents      extents;
  WindowCursor cursor(extents.window());
  std::uint8_t bytes[3] = {};
  CHECK_STATUS(cursor.read(12u, bytes, 2u), Status::ok);
  CHECK_EQ(bytes[0], 12u);
  CHECK_STATUS(cursor.read(1u, bytes, 3u), Status::ok);
  CHECK_EQ(bytes[0], 1u);
  CHECK_EQ(bytes[2], 3u); }

TEST(window_cursor, writes_a_range_across_extents_in_order)
{ Extents            extents;
  WindowCursor       cursor(extents.window());
  const std::uint8_t bytes[6] = {0xA0u, 0xA1u, 0xA2u, 0xA3u, 0xA4u, 0xA5u};
  CHECK_STATUS(cursor.write(4u, bytes, sizeof bytes), Status::ok);
  for (std::uint32_t at = 0u; at < sizeof bytes; at++) CHECK_EQ(extents.byte(4u + at), 0xA0u + at);
  CHECK_EQ(extents.byte(3u), 3u);
  CHECK_EQ(extents.byte(10u), 10u); }

TEST(window_cursor, a_range_past_the_window_is_refused_and_moves_nothing)
{ Extents      extents;
  WindowCursor cursor(extents.window());
  std::uint8_t bytes[7] = {0xEEu, 0xEEu, 0xEEu, 0xEEu, 0xEEu, 0xEEu, 0xEEu};
  CHECK_STATUS(cursor.read(10u, bytes, sizeof bytes), Status::invalid_parameter);
  CHECK_EQ(bytes[0], 0xEEu);
  CHECK_STATUS(cursor.read(17u, bytes, 0u), Status::invalid_parameter);
  CHECK_STATUS(cursor.write(10u, bytes, sizeof bytes), Status::invalid_parameter);
  CHECK_EQ(extents.byte(10u), 10u); }

TEST(window_cursor, extents_short_of_the_window_are_refused_where_they_end)
{ Extents      extents;
  WindowCursor cursor(extents.window(20u));
  std::uint8_t bytes[4] = {};
  CHECK_STATUS(cursor.read(14u, bytes, sizeof bytes), Status::invalid_parameter);
  CHECK_EQ(bytes[0], 14u);
  const std::uint8_t written[4] = {0xB0u, 0xB1u, 0xB2u, 0xB3u};
  CHECK_STATUS(cursor.write(15u, written, sizeof written), Status::invalid_parameter);
  CHECK_EQ(extents.byte(15u), 0xB0u); }

TEST(window_cursor, copies_a_range_between_windows_across_the_extents_of_both)
{ const Extents source;
  Extents       target;
  WindowCursor  from(source.window());
  WindowCursor  to(target.window());
  CHECK_STATUS(from.copy_to(to, 2u, 6u, 9u), Status::ok);
  for (std::uint32_t at = 0u; at < 9u; at++) CHECK_EQ(target.byte(6u + at), source.byte(2u + at));
  CHECK_STATUS(from.copy_to(to, 10u, 0u, 7u), Status::invalid_parameter);
  CHECK_STATUS(from.copy_to(to, 0u, 12u, 5u), Status::invalid_parameter); }
