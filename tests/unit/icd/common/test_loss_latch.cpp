#include "core/log/keyed_log.hpp"
#include "core/trace/state_writer.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/icd/recording_log.hpp"
#include "harness/platform/mock_platform.hpp"
#include "icd/common/loss_latch.hpp"
#include "proto/submission/fault_code.hpp"

#include <cstring>

using av::FaultCode;
using av::icd::LossLatch;

namespace av::icd {

struct LossLatchProbe {
    static void forget_the_lost_bit(LossLatch &latch) { latch.word_ = static_cast<std::uint32_t>(FaultCode::device_lost); }
};

}

using av::icd::LossLatchProbe;

namespace {

class Fixture {
public:
    Fixture() : platform_(), sink_(), lines_(platform_, sink_, "avtest: "), latch_(lines_) {}

    LossLatch        &latch() { return latch_; }
    av::RecordingLog &sink() { return sink_; }

private:
    av::MockPlatform platform_;
    av::RecordingLog sink_;
    av::KeyedLog     lines_;
    LossLatch        latch_;
};

}

TEST(loss_latch, a_latch_nothing_lost_refuses_nothing_and_says_nothing)
{ Fixture fixture;
  CHECK(fixture.latch().invariants_hold());
  CHECK(!fixture.latch().lost());
  CHECK(fixture.latch().cause() == FaultCode::none);
  CHECK(!fixture.latch().guilty());
  CHECK(!fixture.latch().refuses("pfnPresent"));
  CHECK_EQ(fixture.sink().lines(), 0u); }

TEST(loss_latch, the_first_cause_is_kept_and_a_later_one_changes_nothing)
{ Fixture fixture;
  fixture.latch().lose(FaultCode::device_lost);
  CHECK(fixture.latch().lost());
  CHECK(fixture.latch().cause() == FaultCode::device_lost);
  fixture.latch().lose(FaultCode::undecodable);
  fixture.latch().lose(FaultCode::ring_tail);
  CHECK(fixture.latch().cause() == FaultCode::device_lost);
  CHECK(fixture.latch().invariants_hold()); }

TEST(loss_latch, only_the_codes_of_the_contexts_own_commands_are_guilty)
{ const FaultCode own[] = {FaultCode::ring_header,     FaultCode::ring_tail,       FaultCode::batch_header,
                           FaultCode::batch_order,     FaultCode::command_framing, FaultCode::undecodable};
  const FaultCode other[] = {FaultCode::device_lost, FaultCode::no_executor, FaultCode::broken_executor};
  for (const FaultCode code : own) {
      Fixture fixture;
      fixture.latch().lose(code);
      CHECK(fixture.latch().guilty());
  }
  for (const FaultCode code : other) {
      Fixture fixture;
      fixture.latch().lose(code);
      CHECK(fixture.latch().lost());
      CHECK(!fixture.latch().guilty());
  } }

TEST(loss_latch, a_refusal_is_logged_once_per_entry_point_however_many_callers_notice)
{ Fixture fixture;
  fixture.latch().lose(FaultCode::device_lost);
  CHECK(fixture.latch().refuses("pfnBlt"));
  CHECK(fixture.latch().refuses("pfnBlt"));
  CHECK(fixture.latch().refuses("pfnBlt"));
  CHECK_EQ(fixture.sink().lines(), 1u);
  CHECK(fixture.latch().refuses("pfnPresent"));
  CHECK(fixture.latch().refuses("pfnPresent"));
  CHECK_EQ(fixture.sink().lines(), 2u);
  CHECK_EQ(fixture.sink().holding("pfnBlt: the context is lost, fault code 7"), 1u);
  CHECK_EQ(fixture.sink().holding("pfnPresent: the context is lost, fault code 7"), 1u);
  CHECK(fixture.latch().invariants_hold()); }

TEST(loss_latch, forgetting_clears_the_loss_and_the_next_one_is_a_new_cause)
{ Fixture fixture;
  fixture.latch().lose(FaultCode::device_lost);
  fixture.latch().forget();
  CHECK(!fixture.latch().lost());
  CHECK(fixture.latch().cause() == FaultCode::none);
  CHECK(!fixture.latch().refuses("pfnBlt"));
  fixture.latch().lose(FaultCode::ring_header);
  CHECK(fixture.latch().cause() == FaultCode::ring_header);
  CHECK(fixture.latch().guilty());
  CHECK(fixture.latch().invariants_hold()); }

TEST(loss_latch, reports_whether_it_is_lost_and_to_what)
{ Fixture fixture;
  char bytes[128] = {};
  av::StateWriter quiet(bytes, sizeof bytes);
  quiet.open("latch", 0u);
  fixture.latch().report(quiet);
  CHECK(std::strstr(quiet.dump(), "lost=false") != nullptr);
  CHECK(std::strstr(quiet.dump(), "fault_code=0") != nullptr);
  fixture.latch().lose(FaultCode::no_executor);
  char more[128] = {};
  av::StateWriter lost(more, sizeof more);
  lost.open("latch", 0u);
  fixture.latch().report(lost);
  CHECK(std::strstr(lost.dump(), "lost=true") != nullptr);
  CHECK(std::strstr(lost.dump(), "fault_code=8") != nullptr); }

TEST(loss_latch, the_invariant_catches_a_cause_with_no_loss_marked)
{ Fixture fixture;
  fixture.latch().lose(FaultCode::device_lost);
  CHECK(fixture.latch().invariants_hold());
  LossLatchProbe::forget_the_lost_bit(fixture.latch());
  CHECK(!fixture.latch().invariants_hold()); }
