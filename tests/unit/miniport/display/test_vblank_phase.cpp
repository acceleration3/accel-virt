#include "miniport/display/vblank_phase.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>

using av::VblankPhase;

namespace av {

struct VblankPhaseProbe {
    static void set_period(VblankPhase &phase, std::uint32_t period) { phase.period_ns_ = period; }
    static void set_seen(VblankPhase &phase, std::uint32_t seen) { phase.seen_ = seen; }
};

}

namespace {
constexpr std::uint32_t period_ns = 16666600u;
constexpr std::uint64_t period_ticks = 166666u;
constexpr std::uint64_t origin = 50000000u;

std::uint64_t blank_at(std::uint64_t sequence) { return origin + sequence * period_ns / 100u; }

}

TEST(vblank_phase, nothing_is_known_before_a_blank_is_seen)
{ VblankPhase phase;
  CHECK(!phase.known());
  CHECK(phase.invariants_hold());
  CHECK_EQ(phase.since_blank(123456u), 0u);
  CHECK_EQ(phase.period(), 0u);
  phase.observe(1u, blank_at(1u), 0u);
  CHECK(!phase.known()); }

TEST(vblank_phase, the_earliest_arrival_is_the_phase)
{ VblankPhase phase;
  phase.observe(10u, blank_at(10u) + 300u, period_ns);
  CHECK(phase.known());
  CHECK_EQ(phase.period(), period_ticks);
  CHECK_EQ(phase.since_blank(blank_at(12u) + 300u), 0u);
  CHECK_EQ(phase.since_blank(blank_at(12u) + 299u), period_ticks - 1u);
  phase.observe(11u, blank_at(11u) + 20u, period_ns);
  CHECK_EQ(phase.since_blank(blank_at(20u) + 20u), 0u);
  CHECK_EQ(phase.since_blank(blank_at(20u) + 1020u), 1000u);
  phase.observe(12u, blank_at(12u) + 5000u, period_ns);
  CHECK_EQ(phase.since_blank(blank_at(30u) + 20u), 0u);
  CHECK(phase.invariants_hold()); }

TEST(vblank_phase, a_window_later_the_phase_is_taken_afresh_so_drift_is_followed)
{ VblankPhase phase;
  phase.observe(1u, blank_at(1u), period_ns);
  for (std::uint64_t blank = 2u; blank <= VblankPhase::window; blank++) phase.observe(blank, blank_at(blank) + 500u, period_ns);
  CHECK_EQ(phase.since_blank(blank_at(VblankPhase::window + 1u) + 500u), 500u);
  for (std::uint64_t blank = VblankPhase::window + 1u; blank <= 2u * VblankPhase::window; blank++)
      phase.observe(blank, blank_at(blank) + 500u, period_ns);
  CHECK_EQ(phase.since_blank(blank_at(2u * VblankPhase::window + 1u) + 500u), 0u);
  CHECK(phase.invariants_hold()); }

TEST(vblank_phase, before_blank_zero_the_time_since_a_blank_still_counts_back)
{ VblankPhase phase;
  phase.observe(5u, blank_at(5u), period_ns);
  CHECK_EQ(phase.since_blank(origin - 10u), period_ticks - 10u); }

TEST(vblank_phase, a_new_period_starts_over_and_forgetting_leaves_nothing)
{ VblankPhase phase;
  phase.observe(5u, blank_at(5u) + 700u, period_ns);
  phase.observe(6u, 90000000u, 6944444u);
  CHECK_EQ(phase.period(), 69444u);
  CHECK_EQ(phase.since_blank(90000000u), 0u);
  phase.forget();
  CHECK(!phase.known());
   CHECK(phase.invariants_hold()); }

TEST(vblank_phase, fractional_period_at_one_hundred_twenty_hertz_tracks_jittered_arrivals)
{ VblankPhase phase;
  constexpr std::uint64_t epoch = std::uint64_t{7u} * 24u * 60u * 60u * 10000000u;
  constexpr std::uint64_t ticks_per_second = 10000000u;
  constexpr std::uint32_t fractional_period_ns = 8333333u;
  for (std::uint64_t blank = 1u; blank <= 120u * 60u; blank++) {
      const std::uint64_t at = epoch + blank * ticks_per_second / 120u;
      const std::uint64_t delay = blank % 4u == 0u ? 0u : 500u;
      phase.observe(blank, at + delay, fractional_period_ns);
      if (blank < 4u) continue;
      const std::uint64_t into = phase.since_blank(at + 1000u);
      CHECK(into >= 999u && into <= 1001u);
      CHECK_EQ(phase.period(), 83333u);
      CHECK(phase.invariants_hold());
  } }

TEST(vblank_phase_invariants, a_known_phase_has_a_period_and_a_window_in_progress_and_an_unknown_one_nothing)
{ VblankPhase phase;
  phase.observe(10u, blank_at(10u), period_ns);
  CHECK(phase.invariants_hold());
  av::VblankPhaseProbe::set_period(phase, 0u);
  CHECK(!phase.invariants_hold());
  av::VblankPhaseProbe::set_period(phase, period_ns);
  av::VblankPhaseProbe::set_seen(phase, VblankPhase::window);
  CHECK(!phase.invariants_hold());
  phase.forget();
  CHECK(phase.invariants_hold());
  av::VblankPhaseProbe::set_period(phase, period_ns);
  CHECK(!phase.invariants_hold());
  av::VblankPhaseProbe::set_period(phase, 0u);
  av::VblankPhaseProbe::set_seen(phase, 1u);
  CHECK(!phase.invariants_hold()); }
