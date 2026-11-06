#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "icd/common/poll_pause.hpp"

#include <cstdint>

using av::MockKmt;
using av::icd::PollPause;

namespace {

constexpr std::uint64_t frequency = 1000000u;

}

TEST(poll_pause, a_pause_spins_for_its_microseconds_and_between_rests_only_after_that)
{ MockKmt kmt;
  kmt.set_frequency(frequency);
  kmt.reads_clock(nullptr, 500u);
  PollPause pause(kmt);
  std::uint32_t spun = 0u;
  for (; spun < 100u; spun++) {
      pause.between();
      if (kmt.sleeps() != 0u) break;
  }
  CHECK_EQ(spun, 2u);
  CHECK_EQ(kmt.sleeps(), 1u);
  pause.between();
  CHECK_EQ(kmt.sleeps(), 2u); }

TEST(poll_pause, spinning_is_true_while_the_budget_lasts_and_never_sleeps)
{ MockKmt kmt;
  kmt.set_frequency(frequency);
  kmt.reads_clock(nullptr, 500u);
  PollPause pause(kmt);
  CHECK(pause.spinning());
  CHECK(pause.spinning());
  CHECK(!pause.spinning());
  CHECK_EQ(kmt.sleeps(), 0u); }

TEST(poll_pause, resting_sleeps_whatever_the_spin_says)
{ MockKmt kmt;
  kmt.set_frequency(frequency);
  kmt.reads_clock(nullptr, 1u);
  PollPause pause(kmt);
  CHECK(pause.spinning());
  pause.rest();
  pause.rest();
  CHECK_EQ(kmt.sleeps(), 2u);
  CHECK(pause.spinning()); }
