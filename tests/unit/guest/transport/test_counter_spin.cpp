#include "guest/transport/counter_spin.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"

#include <cstdint>

namespace av {

struct CounterSpinProbe {
    static void stop_running(CounterSpin &spin) { spin.running_ = false; }
    static void forget_budget(CounterSpin &spin) { spin.budget_ = 0u; }
};

}

using av::CounterSpin;
using av::CounterSpinProbe;
using av::MockKmt;

namespace {

constexpr std::uint64_t frequency = 1000000u;

MockKmt &ticking(MockKmt &kmt, std::uint64_t per_read)
{
    kmt.set_frequency(frequency);
    kmt.reads_clock(nullptr, per_read);
    return kmt;
}

}

TEST(counter_spin, a_spin_lasts_for_the_microseconds_it_was_given_as_the_counter_moves)
{ MockKmt kmt;
  ticking(kmt, 250u);
  CounterSpin spin(kmt, 1000u);
  CHECK(spin.invariants_hold());
  std::uint32_t lasted = 0u;
  while (spin.lasts() && lasted < 100u) lasted++;
  CHECK(spin.invariants_hold());
  CHECK_EQ(lasted, 4u);
  CHECK(!spin.lasts()); }

TEST(counter_spin, the_budget_starts_at_the_first_look_and_not_at_construction)
{ MockKmt kmt;
  ticking(kmt, 250u);
  CounterSpin spin(kmt, 1000u);
  std::uint64_t ticks_per_second = 0u;
  for (int read = 0; read < 50; read++) static_cast<void>(kmt.performance_counter(ticks_per_second));
  CHECK(spin.lasts());
  CHECK(spin.lasts()); }

TEST(counter_spin, a_spin_of_no_microseconds_never_lasts)
{ MockKmt kmt;
  ticking(kmt, 10u);
  CounterSpin spin(kmt, 0u);
  CHECK(!spin.lasts());
  CHECK(spin.invariants_hold()); }

TEST(counter_spin, a_counter_slower_than_a_microsecond_still_spends_a_fractional_budget)
{ MockKmt kmt;
  kmt.set_frequency(500000u);
  kmt.reads_clock(nullptr, 1u);
  CounterSpin spin(kmt, 4u);
  std::uint32_t lasted = 0u;
  while (spin.lasts() && lasted < 100u) lasted++;
  CHECK_EQ(lasted, 2u); }

TEST(counter_spin, a_spin_that_is_not_running_has_spent_nothing_and_started_nowhere)
{ MockKmt kmt;
  ticking(kmt, 250u);
  CounterSpin budgeted(kmt, 1000u);
  CHECK(budgeted.lasts());
  CounterSpinProbe::stop_running(budgeted);
  CHECK(!budgeted.invariants_hold());
  CounterSpin started(kmt, 1000u);
  CHECK(started.lasts());
  CounterSpinProbe::stop_running(started);
  CounterSpinProbe::forget_budget(started);
  CHECK(!started.invariants_hold()); }
