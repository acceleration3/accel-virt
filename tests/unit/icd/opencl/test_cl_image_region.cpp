/* Where each row of an image transfer is in the application's memory, and
 * which regions the API would refuse before anything is sent. */
#include "harness/framework/test_registry.hpp"
#include "icd/opencl/cl_image_region.hpp"

#include <cstddef>
#include <cstdint>

using av::clicd::ClImageRegion;

namespace {
constexpr std::size_t origin[3] = {5u, 7u, 2u};
constexpr std::size_t extent[3] = {10u, 4u, 3u};
constexpr std::size_t no_origin[3] = {0u, 0u, 0u};
}  // namespace

TEST(cl_image_region, tight_pitches_are_the_rows_and_slices_of_the_region_itself)
{ const ClImageRegion region(origin, extent, 0u, 0u, 4u);
  CHECK(region.valid());
  CHECK_EQ(region.width(), 10u);
  CHECK_EQ(region.rows(), 4u);
  CHECK_EQ(region.slices(), 3u);
  CHECK_EQ(region.element_bytes(), 4u);
  CHECK_EQ(region.row_bytes(), 40u);
  CHECK_EQ(region.row_pitch(), 40u);
  CHECK_EQ(region.slice_pitch(), 160u);
  CHECK(region.rows_packed());
  /* Row 2 of slice 1: one whole slice and two rows in. */
  CHECK_EQ(region.row_offset(2u, 1u), 160u + 80u); }

TEST(cl_image_region, a_pitch_wider_than_a_row_leaves_a_gap_and_is_what_rows_are_found_by)
{ const ClImageRegion region(origin, extent, 64u, 0u, 4u);
  CHECK(region.valid());
  CHECK(!region.rows_packed());
  CHECK_EQ(region.row_pitch(), 64u);
  /* A tight slice of pitched rows. */
  CHECK_EQ(region.slice_pitch(), 256u);
  CHECK_EQ(region.row_offset(3u, 2u), 2u * 256u + 3u * 64u);
  const ClImageRegion sliced(origin, extent, 64u, 1000u, 4u);
  CHECK(sliced.valid());
  CHECK_EQ(sliced.slice_pitch(), 1000u);
  CHECK_EQ(sliced.row_offset(1u, 1u), 1000u + 64u); }

TEST(cl_image_region, a_texel_of_the_region_is_found_in_the_image_from_the_origin)
{ const ClImageRegion region(origin, extent, 0u, 0u, 4u);
  std::size_t at[3] = {};
  region.origin_of(3u, 1u, 2u, at);
  CHECK_EQ(at[0], 8u);
  CHECK_EQ(at[1], 8u);
  CHECK_EQ(at[2], 4u); }

TEST(cl_image_region, a_region_of_nothing_or_of_no_element_size_is_refused)
{ CHECK(!ClImageRegion(origin, extent, 0u, 0u, 0u).valid());
  const std::size_t flat_x[3] = {0u, 4u, 1u}, flat_y[3] = {4u, 0u, 1u}, flat_z[3] = {4u, 4u, 0u};
  CHECK(!ClImageRegion(no_origin, flat_x, 0u, 0u, 4u).valid());
  CHECK(!ClImageRegion(no_origin, flat_y, 0u, 0u, 4u).valid());
  CHECK(!ClImageRegion(no_origin, flat_z, 0u, 0u, 4u).valid()); }

TEST(cl_image_region, a_pitch_too_small_for_what_it_must_hold_is_refused)
{ /* A row is 40 bytes, so a pitch of 39 would overlap the next. */
  CHECK(!ClImageRegion(origin, extent, 39u, 0u, 4u).valid());
  CHECK(ClImageRegion(origin, extent, 40u, 0u, 4u).valid());
  /* A slice of four 40-byte rows is 160 bytes. */
  CHECK(!ClImageRegion(origin, extent, 0u, 159u, 4u).valid());
  CHECK(ClImageRegion(origin, extent, 0u, 160u, 4u).valid()); }

TEST(cl_image_region, extents_whose_bytes_do_not_fit_a_size_are_refused_rather_than_wrapped)
{ const std::size_t huge = ~std::size_t{0} / 2u;
  const std::size_t wide[3] = {huge, 1u, 1u};
  CHECK(!ClImageRegion(no_origin, wide, 0u, 0u, 4u).valid());
  const std::size_t tall[3] = {1024u, huge, 1u};
  CHECK(!ClImageRegion(no_origin, tall, 0u, 0u, 4u).valid());
  const std::size_t deep[3] = {1024u, 1024u, huge};
  CHECK(!ClImageRegion(no_origin, deep, 0u, 0u, 4u).valid()); }
