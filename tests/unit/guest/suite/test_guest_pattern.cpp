/* The pattern a guest writes for someone else to read: its bytes are the ones
 * tools/guest/run_guest_suite.py looks for, none of them zero, and what a view
 * holds is counted byte by byte. */
#include "guest/suite/guest_pattern.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>
#include <cstring>

using av::GuestPattern;

TEST(guest_pattern, writes_one_plus_seven_n_mod_251_and_nothing_past_its_length)
{ const GuestPattern pattern(600u);
  CHECK_EQ(pattern.bytes(), 600u);
  unsigned char view[602] = {};
  pattern.write(view);
  CHECK_EQ(view[0], 1u);
  CHECK_EQ(view[1], 8u);
  CHECK_EQ(view[35], 246u);   /* 1 + 245 */
  CHECK_EQ(view[36], 2u);     /* 1 + 252 % 251 */
  CHECK_EQ(view[250], 245u);  /* 1 + 1750 % 251 */
  CHECK_EQ(view[251], 1u);    /* the period */
  CHECK_EQ(view[599], static_cast<unsigned char>(1u + 599u * 7u % 251u));
  CHECK_EQ(view[600], 0u);
  CHECK_EQ(pattern.filled(view), 600u);
  CHECK_EQ(pattern.matched(view), 600u); }

TEST(guest_pattern, matched_and_filled_count_each_byte)
{ const GuestPattern pattern(300u);
  unsigned char view[300] = {};
  CHECK_EQ(pattern.matched(view), 0u);
  CHECK_EQ(pattern.filled(view), 0u);
  pattern.write(view);
  view[0] = 0u;
  view[299] = static_cast<unsigned char>(view[299] + 1u);
  CHECK_EQ(pattern.matched(view), 298u);
  CHECK_EQ(pattern.filled(view), 299u);
  pattern.clear(view);
  CHECK_EQ(pattern.filled(view), 0u);
  const GuestPattern empty(0u);
  pattern.write(view);
  empty.clear(view);
  CHECK_EQ(pattern.matched(view), 300u); }
