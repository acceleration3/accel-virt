/* A run of rows (RowBand): empty by default and when its end is not past its
 * first, and joining two takes the smallest run holding both. */
#include "harness/framework/test_registry.hpp"
#include "icd/d3d/row_band.hpp"

using av::d3d::RowBand;

TEST(row_band, a_default_band_holds_no_row)
{ const RowBand band;
  CHECK(band.empty());
  CHECK_EQ(band.first(), 0u);
  CHECK_EQ(band.end(), 0u); }

TEST(row_band, holds_the_rows_from_first_up_to_end)
{ const RowBand band(2u, 6u);
  CHECK(!band.empty());
  CHECK_EQ(band.first(), 2u);
  CHECK_EQ(band.end(), 6u);
  CHECK(!RowBand(4u, 5u).empty()); }

TEST(row_band, an_end_at_or_before_first_is_empty_at_first)
{ const RowBand same(3u, 3u);
  CHECK(same.empty());
  CHECK_EQ(same.first(), 3u);
  CHECK_EQ(same.end(), 3u);
  const RowBand before(5u, 2u);
  CHECK(before.empty());
  CHECK_EQ(before.first(), 5u);
  CHECK_EQ(before.end(), 5u); }

TEST(row_band, joining_takes_the_smallest_band_holding_both)
{ const RowBand low(2u, 4u), high(7u, 9u);
  const RowBand across = low.joined(high);
  CHECK_EQ(across.first(), 2u);
  CHECK_EQ(across.end(), 9u);
  const RowBand reversed = high.joined(low);
  CHECK_EQ(reversed.first(), 2u);
  CHECK_EQ(reversed.end(), 9u); }

TEST(row_band, joining_with_a_band_inside_or_overlapping_keeps_the_outer_edges)
{ const RowBand outer(1u, 10u), inner(3u, 5u);
  CHECK_EQ(outer.joined(inner).first(), 1u);
  CHECK_EQ(outer.joined(inner).end(), 10u);
  CHECK_EQ(inner.joined(outer).first(), 1u);
  CHECK_EQ(inner.joined(outer).end(), 10u);
  const RowBand left(1u, 6u), right(4u, 12u);
  CHECK_EQ(left.joined(right).first(), 1u);
  CHECK_EQ(left.joined(right).end(), 12u); }

TEST(row_band, an_empty_band_adds_nothing)
{ const RowBand band(2u, 6u), none(40u, 40u);
  CHECK_EQ(band.joined(none).first(), 2u);
  CHECK_EQ(band.joined(none).end(), 6u);
  CHECK_EQ(none.joined(band).first(), 2u);
  CHECK_EQ(none.joined(band).end(), 6u);
  CHECK(none.joined(RowBand()).empty()); }
