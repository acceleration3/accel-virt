/* Where the scan-out is within its refresh: the blank begins it, so every
 * blank line comes before every visible one, and lines rise within each. */
#include "harness/framework/test_registry.hpp"
#include "guest/kmt/beam_position.hpp"

using av::BeamPosition;

TEST(beam_position, a_blank_line_is_earlier_than_any_visible_one)
{ const BeamPosition blank(true, 1124u);
  const BeamPosition visible(false, 0u);
  CHECK(blank.earlier_than(visible));
  CHECK(!visible.earlier_than(blank)); }

TEST(beam_position, within_the_blank_or_the_picture_a_lower_line_is_earlier)
{ CHECK(BeamPosition(false, 10u).earlier_than(BeamPosition(false, 500u)));
  CHECK(!BeamPosition(false, 500u).earlier_than(BeamPosition(false, 10u)));
  CHECK(BeamPosition(true, 1080u).earlier_than(BeamPosition(true, 1100u)));
  CHECK(!BeamPosition(true, 1100u).earlier_than(BeamPosition(true, 1080u))); }

TEST(beam_position, the_same_place_is_not_earlier_either_way)
{ const BeamPosition here(false, 300u);
  CHECK(!here.earlier_than(BeamPosition(false, 300u)));
  CHECK(!BeamPosition().earlier_than(BeamPosition(false, 0u)));
  CHECK(BeamPosition(true, 1080u).earlier_than(BeamPosition())); }
