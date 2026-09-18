/* How many vertical blanks a swap waits for (feature 051, US2.2).
 *
 * WGL_EXT_swap_control defines 0 and 1, and leaves anything above 1 to the
 * implementation; what it does not define is a negative interval, which is
 * WGL_EXT_swap_control_tear's, and this driver reports no tear control - so an
 * application asking for one is refused and keeps the interval it had. */
#include "harness/framework/test_registry.hpp"
#include "icd/opengl/swap_interval.hpp"

#include <cstdint>

using av::glicd::SwapInterval;

TEST(swap_interval, a_new_context_waits_for_one_blank)
{ SwapInterval interval;
  CHECK(interval.invariants_hold());
  CHECK_EQ(interval.blanks(), 1u);
  CHECK_EQ(interval.waited(), 0u); }

TEST(swap_interval, what_is_asked_for_within_range_is_kept)
{ SwapInterval interval;
  for (std::int32_t asked = 0; asked <= static_cast<std::int32_t>(SwapInterval::max_blanks); asked++) {
      CHECK(interval.ask(asked));
      CHECK_EQ(interval.blanks(), static_cast<std::uint32_t>(asked));
      CHECK(interval.invariants_hold());
  } }

TEST(swap_interval, an_interval_nobody_defines_is_refused_and_changes_nothing)
{ SwapInterval interval;
  CHECK(interval.ask(2));
  const std::int32_t refused[] = {-1, -100, static_cast<std::int32_t>(SwapInterval::max_blanks) + 1, 1000,
                                  -2147483647 - 1};
  for (const std::int32_t asked : refused) {
      CHECK(!interval.ask(asked));
      CHECK_EQ(interval.blanks(), 2u);
      CHECK(interval.invariants_hold());
  } }

TEST(swap_interval, every_blank_waited_for_is_counted)
{ SwapInterval interval;
  CHECK_EQ(interval.waited(), 0u);
  interval.waited_one();
  interval.waited_one();
  CHECK_EQ(interval.waited(), 2u);
  /* An interval of nothing waits for nothing, and the count stands. */
  CHECK(interval.ask(0));
  CHECK_EQ(interval.blanks(), 0u);
  CHECK_EQ(interval.waited(), 2u);
  CHECK(interval.invariants_hold()); }
