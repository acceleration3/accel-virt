#include "../../src/proto/rectangle.hpp"
#include "../../src/proto/wire_writer.hpp"
#include "../harness/test_registry.hpp"

#include <cstdint>

using av::Rectangle;
using av::WireWriter;

TEST(rectangle_is_valid, when_neither_edge_is_inverted)
{ CHECK(Rectangle(0, 0, 0, 0).is_valid());
  CHECK(Rectangle(-5, -5, 10, 20).is_valid());
  CHECK(!Rectangle(10, 0, 9, 1).is_valid());
  CHECK(!Rectangle(0, 10, 1, 9).is_valid()); }

TEST(rectangle_write, writes_left_top_right_bottom_little_endian)
{ std::uint8_t bytes[16] = {};
  WireWriter writer(bytes, sizeof bytes);
  Rectangle(1, -2, 0x01020304, 4).write(writer);
  CHECK_EQ(writer.offset(), 16u);
  CHECK_EQ(bytes[0], 1u);
  CHECK_EQ(bytes[4], 0xFEu);
  CHECK_EQ(bytes[7], 0xFFu);
  CHECK_EQ(bytes[8], 0x04u);
  CHECK_EQ(bytes[11], 0x01u);
  CHECK_EQ(bytes[12], 4u); }
