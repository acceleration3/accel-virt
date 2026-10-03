/* One staged piece's rows, between the application's layout and a block
 * packed tight. What a transfer depends on: every byte of the piece's rows
 * lands in the block in order with nothing between rows, comes back to the
 * same place, and nothing of the application's around the piece - the gap
 * after a row, the columns before and after a run, the rows of other pieces,
 * other slices - is read into the block or written back over. */
#include "harness/framework/test_registry.hpp"
#include "icd/common/row_pieces.hpp"
#include "icd/opencl/cl_image_region.hpp"
#include "icd/opencl/cl_staged_rows.hpp"
#include "proto/resources/texel_layout.hpp"

#include <cstdint>
#include <cstring>

using av::TexelLayout;
using av::clicd::ClImageRegion;
using av::clicd::ClStagedRows;
using av::icd::RowPieces;

namespace {

/* 2-byte elements, 6 a row in a 16-byte pitch, 4 rows a slice in a 70-byte
 * slice pitch, 2 slices: byte i of the application's memory is i. */
constexpr std::size_t element = 2u, width = 6u, rows = 4u, pitch = 16u, slice_pitch = 70u;
const std::size_t     origin[3] = {0u, 0u, 0u}, extent[3] = {width, rows, 2u};

}  // namespace

TEST(cl_staged_rows, whole_rows_of_a_slice_pack_tight_and_unpack_into_place)
{ const ClImageRegion region(origin, extent, pitch, slice_pitch, element);
  std::uint8_t application[2u * slice_pitch];
  for (std::size_t index = 0u; index < sizeof application; ++index) application[index] = static_cast<std::uint8_t>(index);
  /* A block of three packed rows: the second piece of slice 1 is its last row. */
  RowPieces pieces(TexelLayout(element, 1u, 1u), width, rows, width * element, 3u * width * element);
  CHECK(pieces.next());
  CHECK(pieces.next());
  CHECK_EQ(pieces.top(), 3u);
  std::uint8_t staged[64];
  std::memset(staged, 0xAA, sizeof staged);
  const ClStagedRows second(region, pieces, 1u);
  second.pack(application, staged);
  for (std::size_t x = 0u; x < width * element; ++x)
      CHECK_EQ(staged[x], static_cast<std::uint8_t>(slice_pitch + 3u * pitch + x));
  CHECK_EQ(staged[width * element], 0xAAu);

  RowPieces first(TexelLayout(element, 1u, 1u), width, rows, width * element, 3u * width * element);
  CHECK(first.next());
  const ClStagedRows top(region, first, 0u);
  top.pack(application, staged);
  for (std::size_t y = 0u; y < 3u; ++y)
      for (std::size_t x = 0u; x < width * element; ++x)
          CHECK_EQ(staged[y * width * element + x], static_cast<std::uint8_t>(y * pitch + x));

  std::uint8_t back[2u * slice_pitch];
  std::memset(back, 0xEE, sizeof back);
  top.unpack(staged, back);
  for (std::size_t y = 0u; y < 3u; ++y) {
      CHECK_EQ(back[y * pitch], static_cast<std::uint8_t>(y * pitch));
      CHECK_EQ(back[y * pitch + width * element - 1u], static_cast<std::uint8_t>(y * pitch + width * element - 1u));
      CHECK_EQ(back[y * pitch + width * element], 0xEEu);
  }
  CHECK_EQ(back[3u * pitch], 0xEEu);
  CHECK_EQ(back[slice_pitch], 0xEEu); }

/* A run - a row too long for the block - is its own columns and no others. */
TEST(cl_staged_rows, a_run_of_one_row_is_its_columns_alone)
{ const ClImageRegion region(origin, extent, pitch, slice_pitch, element);
  std::uint8_t application[2u * slice_pitch];
  for (std::size_t index = 0u; index < sizeof application; ++index) application[index] = static_cast<std::uint8_t>(index);
  RowPieces pieces(TexelLayout(element, 1u, 1u), width, rows, width * element, 4u * element);
  CHECK(pieces.next());
  CHECK(pieces.next());
  CHECK_EQ(pieces.left(), 4u);
  CHECK_EQ(pieces.right(), width);
  const ClStagedRows run(region, pieces, 1u);
  std::uint8_t staged[8];
  std::memset(staged, 0xAA, sizeof staged);
  run.pack(application, staged);
  CHECK_EQ(staged[0], static_cast<std::uint8_t>(slice_pitch + 4u * element));
  CHECK_EQ(staged[3], static_cast<std::uint8_t>(slice_pitch + 4u * element + 3u));
  CHECK_EQ(staged[4], 0xAAu);
  std::uint8_t back[2u * slice_pitch];
  std::memset(back, 0xEE, sizeof back);
  run.unpack(staged, back);
  CHECK_EQ(back[slice_pitch + 4u * element - 1u], 0xEEu);
  CHECK_EQ(back[slice_pitch + 4u * element], static_cast<std::uint8_t>(slice_pitch + 4u * element));
  CHECK_EQ(back[slice_pitch + width * element - 1u], static_cast<std::uint8_t>(slice_pitch + width * element - 1u));
  CHECK_EQ(back[slice_pitch + width * element], 0xEEu); }
