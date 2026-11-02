#include "miniport/adapter/power_transition.hpp"
#include "core/support/status.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>

using av::PowerTransition;
using av::Status;

TEST(power_transition_hold, remembers_what_was_running)
{ PowerTransition transition;
  CHECK(!transition.held());
  CHECK_STATUS(transition.hold(true, 0x5u), Status::ok);
  CHECK(transition.held());
  CHECK(transition.started());
  CHECK(transition.scanning_out(0u));
  CHECK(!transition.scanning_out(1u));
  CHECK(transition.scanning_out(2u)); }

TEST(power_transition_release, answers_once_and_forgets_it)
{ PowerTransition transition;
  CHECK_STATUS(transition.hold(true, 0x2u), Status::ok);
  transition.release();
  CHECK(!transition.held());
  CHECK(!transition.started());
  CHECK(!transition.scanning_out(1u));
  CHECK_STATUS(transition.hold(false, 0u), Status::ok);
  CHECK(transition.held());
  CHECK(!transition.started()); }

TEST(power_transition_hold, a_second_hold_without_a_release_is_refused)
{ PowerTransition transition;
  CHECK_STATUS(transition.hold(true, 0x1u), Status::ok);
  CHECK_STATUS(transition.hold(false, 0x6u), Status::invalid_state);
  CHECK(transition.started());
  CHECK(transition.scanning_out(0u));
  CHECK(!transition.scanning_out(2u)); }

TEST(power_transition, a_transition_that_held_nothing_restores_nothing)
{ PowerTransition transition;
  CHECK(!transition.started());
  for (std::uint32_t source = 0u; source < 4u; source++) CHECK(!transition.scanning_out(source));
  transition.release();
  CHECK(!transition.held()); }

TEST(power_transition_scanning_out, a_source_past_what_it_holds_scanned_out_nothing)
{ av::PowerTransition transition;
  CHECK_STATUS(transition.hold(true, 0xFFFFFFFFu), av::Status::ok);
  CHECK(transition.scanning_out(31u));
  CHECK(!transition.scanning_out(32u));
  CHECK(!transition.scanning_out(0xFFFFFFFFu)); }
