#include "executor/d3d/format_caps_rows.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/d3d/dxgi_format.hpp"
#include "proto/generated/d3d/d3d_wire_types.h"

#include <cstdint>
#include <cstring>

using av::DxgiFormat;
using av::FormatCapsRows;
using av::d3dproto::FormatCaps;

namespace {

constexpr std::uint64_t row_bytes = sizeof(FormatCaps);

FormatCaps row(std::uint32_t support, std::uint32_t support2, std::uint32_t quality)
{
    FormatCaps made = {};
    made.support = support;
    made.support2 = support2;
    made.quality_levels[0] = quality;
    made.quality_levels[3] = quality + 3u;
    return made;
}

FormatCaps row_at(const std::uint8_t *bytes, std::uint32_t format)
{
    FormatCaps seen = {};
    std::memcpy(&seen, bytes + std::uint64_t{format} * row_bytes, sizeof seen);
    return seen;
}

}

TEST(format_caps_rows, every_format_starts_with_no_support_at_all)
{ FormatCapsRows rows;
  std::uint8_t   bytes[DxgiFormat::count * row_bytes];
  std::memset(bytes, 0xAB, sizeof bytes);
  CHECK_EQ(rows.give(bytes, sizeof bytes), sizeof bytes);
  for (std::uint8_t byte : bytes) CHECK_EQ(byte, 0u); }

TEST(format_caps_rows, an_answered_row_is_given_back_at_its_formats_place_and_no_other)
{ FormatCapsRows rows;
  rows.answer(2u, row(0x11u, 0x22u, 4u));
  rows.answer(DxgiFormat::count - 1u, row(0x33u, 0x44u, 8u));
  std::uint8_t bytes[DxgiFormat::count * row_bytes];
  CHECK_EQ(rows.give(bytes, sizeof bytes), sizeof bytes);
  const FormatCaps first = row_at(bytes, 2u);
  CHECK_EQ(first.support, 0x11u);
  CHECK_EQ(first.support2, 0x22u);
  CHECK_EQ(first.quality_levels[0], 4u);
  CHECK_EQ(first.quality_levels[3], 7u);
  const FormatCaps last = row_at(bytes, DxgiFormat::count - 1u);
  CHECK_EQ(last.support, 0x33u);
  CHECK_EQ(last.quality_levels[0], 8u);
  CHECK_EQ(row_at(bytes, 1u).support, 0u);
  CHECK_EQ(row_at(bytes, 3u).support, 0u); }

TEST(format_caps_rows, an_answer_for_a_format_past_the_table_is_dropped_and_a_later_answer_replaces_an_earlier)
{ FormatCapsRows rows;
  rows.answer(DxgiFormat::count, row(0x55u, 0u, 0u));
  rows.answer(0xFFFFFFFFu, row(0x66u, 0u, 0u));
  rows.answer(5u, row(0x77u, 0u, 0u));
  rows.answer(5u, row(0x88u, 0x99u, 0u));
  std::uint8_t bytes[DxgiFormat::count * row_bytes];
  CHECK_EQ(rows.give(bytes, sizeof bytes), sizeof bytes);
  CHECK_EQ(row_at(bytes, 5u).support, 0x88u);
  CHECK_EQ(row_at(bytes, 5u).support2, 0x99u);
  for (std::uint32_t format = 0u; format < DxgiFormat::count; format++)
      if (format != 5u) CHECK_EQ(row_at(bytes, format).support, 0u); }

TEST(format_caps_rows, a_short_buffer_gets_only_the_whole_rows_that_fit)
{ FormatCapsRows rows;
  rows.answer(0u, row(0xA1u, 0u, 0u));
  rows.answer(1u, row(0xA2u, 0u, 0u));
  rows.answer(2u, row(0xA3u, 0u, 0u));
  std::uint8_t bytes[3u * row_bytes + 5u];
  std::memset(bytes, 0xEE, sizeof bytes);
  CHECK_EQ(rows.give(bytes, 2u * row_bytes + 7u), 2u * row_bytes);
  CHECK_EQ(row_at(bytes, 0u).support, 0xA1u);
  CHECK_EQ(row_at(bytes, 1u).support, 0xA2u);
  CHECK_EQ(bytes[2u * row_bytes], 0xEEu);
  CHECK_EQ(rows.give(bytes, row_bytes - 1u), 0u);
  CHECK_EQ(rows.give(bytes, 0u), 0u);
  CHECK_EQ(rows.give(nullptr, 0u), 0u);
  std::uint8_t roomy[(DxgiFormat::count + 2u) * row_bytes];
  CHECK_EQ(rows.give(roomy, sizeof roomy), DxgiFormat::count * row_bytes); }
