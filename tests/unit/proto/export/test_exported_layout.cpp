/* An exported surface's layout: a picture only with a size, a format and
 * rows at least as wide as the picture, and empty only when every field is. */
#include "harness/framework/test_registry.hpp"
#include "proto/export/exported_layout.hpp"

#include <cstdint>

using av::ExportedLayout;

TEST(exported_layout, keeps_what_it_was_given)
{ const ExportedLayout layout(1003u, 701u, 0x34325241u, 4096u, 256u, 0x00FFFFFFFFFFFFFFull);
  CHECK_EQ(layout.width(), 1003u);
  CHECK_EQ(layout.height(), 701u);
  CHECK_EQ(layout.fourcc(), 0x34325241u);
  CHECK_EQ(layout.stride(), 4096u);
  CHECK_EQ(layout.offset(), 256u);
  CHECK_EQ(layout.modifier(), 0x00FFFFFFFFFFFFFFull);
  CHECK(layout.describes_a_picture());
  CHECK(!layout.empty()); }

TEST(exported_layout, a_picture_needs_a_size_a_format_and_rows_as_wide_as_it)
{ CHECK(!ExportedLayout(0u, 1u, 1u, 4u, 0u, 0u).describes_a_picture());
  CHECK(!ExportedLayout(1u, 0u, 1u, 4u, 0u, 0u).describes_a_picture());
  CHECK(!ExportedLayout(1u, 1u, 0u, 4u, 0u, 0u).describes_a_picture());
  CHECK(!ExportedLayout(8u, 1u, 1u, 7u, 0u, 0u).describes_a_picture());
  CHECK(ExportedLayout(8u, 1u, 1u, 8u, 0u, 0u).describes_a_picture()); }

TEST(exported_layout, is_empty_only_when_every_field_is_zero)
{ CHECK(ExportedLayout().empty());
  CHECK(!ExportedLayout().describes_a_picture());
  CHECK(!ExportedLayout(0u, 0u, 0u, 0u, 0u, 1u).empty());
  CHECK(!ExportedLayout(0u, 0u, 0u, 0u, 1u, 0u).empty());
  CHECK(!ExportedLayout(0u, 0u, 0u, 1u, 0u, 0u).empty());
  CHECK(!ExportedLayout(0u, 0u, 1u, 0u, 0u, 0u).empty());
  CHECK(!ExportedLayout(0u, 1u, 0u, 0u, 0u, 0u).empty());
  CHECK(!ExportedLayout(1u, 0u, 0u, 0u, 0u, 0u).empty()); }
