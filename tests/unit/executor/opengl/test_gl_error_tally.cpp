#include "executor/opengl/gl_error_tally.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>

using av::GlErrorTally;

TEST(gl_error_tally, starts_as_the_bare_marker)
{ const GlErrorTally tally;
  CHECK_EQ(tally.word(), GlErrorTally::published);
  CHECK_EQ(GlErrorTally::published, 0x8000000000000000ull); }

TEST(gl_error_tally, counts_each_kind_in_its_own_byte)
{ GlErrorTally tally;
  tally.record(0x0500u);
  tally.record(0x0502u);
  tally.record(0x0502u);
  tally.record(0x0506u);
  CHECK_EQ(tally.word(), 0x8001000000020001ull);
  tally.record(0x0507u);
  tally.record(0u);
  tally.record(0x04FFu);
  tally.record(0x8031u);
  CHECK_EQ(tally.word(), 0x8001000000020001ull); }

TEST(gl_error_tally, a_count_wraps_without_touching_its_neighbours)
{ GlErrorTally tally;
  for (int n = 0; n < 256; n++) tally.record(0x0501u);
  tally.record(0x0500u);
  CHECK_EQ(tally.word(), 0x8000000000000001ull); }
