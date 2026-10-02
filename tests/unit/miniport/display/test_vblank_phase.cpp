/* When the device's blanks fall on the guest's clock, from the times the
 * guest saw them: never earlier than they happened, so the earliest
 * arrivals give the phase; a window's worth later the phase is taken afresh,
 * so a guest clock drifting against the host's is followed; a new period
 * starts over. */
#include "miniport/display/vblank_phase.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>

using av::VblankPhase;

namespace {

/* A period of whole 100 ns ticks, so the times below are exact on both clocks. */
constexpr std::uint32_t period_ns = 16666600u;
constexpr std::uint64_t period_ticks = 166666u;
constexpr std::uint64_t origin = 50000000u;      /* where blank 0 falls on the guest's clock, 100 ns */

std::uint64_t blank_at(std::uint64_t sequence) { return origin + sequence * period_ns / 100u; }

}  // namespace

TEST(vblank_phase, nothing_is_known_before_a_blank_is_seen)
{ VblankPhase phase;
  CHECK(!phase.known());
  CHECK(phase.invariants_hold());
  CHECK_EQ(phase.since_blank(123456u), 0u);
  CHECK_EQ(phase.period(), 0u);
  std::uint64_t sequence = 0u, at = 0u;
  CHECK(!phase.next(123456u, sequence, at));
  phase.observe(1u, blank_at(1u), 0u);   /* no period: nothing to go on */
  CHECK(!phase.known()); }

TEST(vblank_phase, the_earliest_arrival_is_the_phase)
{ VblankPhase phase;
  phase.observe(10u, blank_at(10u) + 300u, period_ns);   /* seen 30 us late */
  CHECK(phase.known());
  CHECK_EQ(phase.period(), period_ticks);
  CHECK_EQ(phase.since_blank(blank_at(12u) + 300u), 0u);
  CHECK_EQ(phase.since_blank(blank_at(12u) + 299u), period_ticks - 1u);
  phase.observe(11u, blank_at(11u) + 20u, period_ns);    /* a quicker one: the phase moves earlier */
  CHECK_EQ(phase.since_blank(blank_at(20u) + 20u), 0u);
  CHECK_EQ(phase.since_blank(blank_at(20u) + 1020u), 1000u);
  phase.observe(12u, blank_at(12u) + 5000u, period_ns);  /* a late one changes nothing */
  CHECK_EQ(phase.since_blank(blank_at(30u) + 20u), 0u);
  CHECK(phase.invariants_hold()); }

TEST(vblank_phase, a_window_later_the_phase_is_taken_afresh_so_drift_is_followed)
{ VblankPhase phase;
  phase.observe(1u, blank_at(1u), period_ns);   /* an early outlier */
  for (std::uint64_t blank = 2u; blank <= VblankPhase::window; blank++) phase.observe(blank, blank_at(blank) + 500u, period_ns);
  /* Still the outlier's phase: the window it opened has not closed. */
  CHECK_EQ(phase.since_blank(blank_at(VblankPhase::window + 1u) + 500u), 500u);
  for (std::uint64_t blank = VblankPhase::window + 1u; blank <= 2u * VblankPhase::window; blank++)
      phase.observe(blank, blank_at(blank) + 500u, period_ns);
  /* A window of arrivals 50 us late and none earlier: that is where the blanks are now. */
  CHECK_EQ(phase.since_blank(blank_at(2u * VblankPhase::window + 1u) + 500u), 0u);
  CHECK(phase.invariants_hold()); }

TEST(vblank_phase, the_next_blank_is_the_one_after_now)
{ VblankPhase phase;
  phase.observe(5u, blank_at(5u), period_ns);
  std::uint64_t sequence = 0u, at = 0u;
  CHECK(phase.next(blank_at(7u) + 10u, sequence, at));
  CHECK_EQ(sequence, 8u);
  CHECK_EQ(at, blank_at(8u));
  CHECK(phase.next(blank_at(7u), sequence, at));   /* exactly on a blank: the one after it */
  CHECK_EQ(sequence, 8u);
  CHECK(phase.next(origin - 10u, sequence, at));   /* before blank 0 */
  CHECK_EQ(sequence, 0u);
  CHECK_EQ(at, origin);
  CHECK_EQ(phase.since_blank(origin - 10u), period_ticks - 10u); }

TEST(vblank_phase, a_new_period_starts_over_and_forgetting_leaves_nothing)
{ VblankPhase phase;
  phase.observe(5u, blank_at(5u) + 700u, period_ns);
  phase.observe(6u, 90000000u, 6944444u);   /* 144 Hz from here */
  CHECK_EQ(phase.period(), 69444u);
  CHECK_EQ(phase.since_blank(90000000u), 0u);
  phase.forget();
  CHECK(!phase.known());
  CHECK(phase.invariants_hold()); }
