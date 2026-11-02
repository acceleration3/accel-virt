#include "miniport/present/present_waiters.hpp"
#include "core/platform/lock_rank.hpp"
#include "core/support/status.hpp"
#include "core/trace/state_writer.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "harness/platform/violation.hpp"
#include "miniport/memory/present_reads.hpp"
#include "miniport/memory/present_writes.hpp"
#include "miniport/present/present_page_view.hpp"
#include "proto/pages/present_progress.hpp"

#include <cstdint>
#include <cstring>

namespace av {

struct PresentWaitersProbe {
    static void time_out_without_waiting(PresentWaiters &waiters) { waiters.timeouts_ = waiters.waits_ + 1u; }
    static void time_out_a_hold_without_waiting(PresentWaiters &waiters)
    {
        waiters.hold_timeouts_ = waiters.holds_ + 1u;
    }
};

}

using av::Irql;
using av::LockRank;
using av::MockPlatform;
using av::PresentPageView;
using av::PresentProgress;
using av::PresentReads;
using av::PresentWaiters;
using av::PresentWaitersProbe;
using av::PresentWrites;
using av::StateWriter;
using av::Status;
using av::Violation;

namespace {

constexpr std::uint64_t ample = 100000000u;

class EnginePage {
public:
    explicit EnginePage(MockPlatform &platform) : platform_(platform), view_(platform)
    {
        CHECK_STATUS(view_.initialize(), Status::ok);
    }

    const PresentPageView &view() const { return view_; }
    void                   completed(std::uint64_t ticket)
    {
        const auto page = static_cast<std::uint8_t *>(
            platform_.translate_physical(view_.address(), PresentProgress::page_size));
        CHECK_STATUS(PresentProgress(ticket, 0u).encode(page, PresentProgress::encoded_size), Status::ok);
    }

private:
    MockPlatform   &platform_;
    PresentPageView view_;
};

struct Script {
    using Step = void (*)(Script &);

    MockPlatform   *platform = nullptr;
    PresentWaiters *waiters = nullptr;
    PresentWrites  *writes = nullptr;
    PresentReads   *reads = nullptr;
    EnginePage     *engine = nullptr;
    Step            steps[8] = {};
    std::uint32_t   count = 0u;
    std::uint32_t   ran = 0u;
    Status          nested[8] = {};
    std::uint64_t   nested_sequence[8] = {};

    void run()
    {
        if (ran < count) steps[ran++](*this);
    }
    void install()
    {
        platform->on_stall([](void *context) { static_cast<Script *>(context)->run(); }, this);
    }
};


bool ceiling_broken(MockPlatform &platform)
{
    bool broken = false;
    while (platform.claim(Violation::irql_ceiling)) broken = true;
    return broken;
}

}

TEST(present_waiters, settled_pages_are_read_at_once_without_a_slot_or_a_sleep)
{ MockPlatform   platform;
  PresentWaiters waiters(platform);
  PresentWrites  writes(platform);
  std::uint64_t  sequence = 99u;
  CHECK_STATUS(waiters.wait_for(writes, nullptr, 1u, ample, sequence), Status::ok);
  CHECK_EQ(sequence, 0u);
  writes.opening();
  writes.written(0u, 0u);
  CHECK_STATUS(waiters.wait_for(writes, nullptr, 1u, ample, sequence), Status::ok);
  CHECK_EQ(sequence, 2u);
  EnginePage engine(platform);
  writes.opening();
  writes.written(7u, 1u);
  engine.completed(7u);
  CHECK_STATUS(waiters.wait_for(writes, &engine.view(), 1u, ample, sequence), Status::ok);
  CHECK_EQ(sequence, 4u);
  CHECK_EQ(platform.slept_ticks(), 0u);
  CHECK_EQ(waiters.waits(), 0u);
  CHECK_EQ(waiters.timeouts(), 0u);
  CHECK(waiters.invariants_hold());
  CHECK_EQ(platform.locks_held(), 0u);
  CHECK_EQ(platform.unclaimed_violations(), 0u); }

TEST(present_waiters, a_device_without_an_engine_has_no_ticket_to_wait_for)
{ MockPlatform   platform;
  PresentWaiters waiters(platform);
  PresentWrites  writes(platform);
  writes.opening();
  writes.written(5u, 1u);
  std::uint64_t sequence = 0u;
  CHECK_STATUS(waiters.wait_for(writes, nullptr, 1u, ample, sequence), Status::ok);
  CHECK_EQ(sequence, 2u);
  CHECK_EQ(waiters.waits(), 0u); }

TEST(present_waiters, sleeps_until_the_engine_says_the_ticket_has_moved)
{ MockPlatform   platform;
  PresentWaiters waiters(platform);
  PresentWrites  writes(platform);
  EnginePage     engine(platform);
  writes.opening();
  writes.written(3u, 1u);
  engine.completed(2u);
  Script script;
  script.platform = &platform;
  script.waiters = &waiters;
  script.engine = &engine;
  script.steps[0] = [](Script &now) {
      now.engine->completed(3u);
      now.waiters->wake();
  };
  script.count = 1u;
  script.install();
  std::uint64_t sequence = 0u;
  CHECK_STATUS(waiters.wait_for(writes, &engine.view(), 1u, ample, sequence), Status::ok);
  CHECK_EQ(sequence, 2u);
  CHECK_EQ(script.ran, 1u);
  CHECK_EQ(platform.slept_ticks(), 0u);
  CHECK_EQ(waiters.waits(), 1u);
  CHECK_EQ(waiters.timeouts(), 0u);
  CHECK(waiters.invariants_hold());
  CHECK_EQ(platform.unclaimed_violations(), 0u); }

TEST(present_waiters, sleeps_while_a_present_is_being_written_and_begins_after_it)
{ MockPlatform   platform;
  PresentWaiters waiters(platform);
  PresentWrites  writes(platform);
  writes.opening();
  writes.written(0u, 0u);
  writes.opening();
  Script script;
  script.platform = &platform;
  script.waiters = &waiters;
  script.writes = &writes;
  script.steps[0] = [](Script &now) {
      now.writes->written(0u, 0u);
      now.waiters->wake();
  };
  script.count = 1u;
  script.install();
  std::uint64_t sequence = 0u;
  CHECK_STATUS(waiters.wait_for(writes, nullptr, 1u, ample, sequence), Status::ok);
  CHECK_EQ(sequence, 4u);
  CHECK_EQ(waiters.waits(), 1u);
  CHECK(writes.invariants_hold()); }

TEST(present_waiters, a_wake_that_leaves_the_ticket_short_is_one_more_look_and_one_more_sleep)
{ MockPlatform   platform;
  PresentWaiters waiters(platform);
  PresentWrites  writes(platform);
  EnginePage     engine(platform);
  writes.opening();
  writes.written(3u, 1u);
  Script script;
  script.platform = &platform;
  script.waiters = &waiters;
  script.engine = &engine;
  script.steps[0] = [](Script &now) {
      now.engine->completed(2u);
      now.waiters->wake();
  };
  script.steps[1] = [](Script &now) {
      now.engine->completed(3u);
      now.waiters->wake();
  };
  script.count = 2u;
  script.install();
  std::uint64_t sequence = 0u;
  CHECK_STATUS(waiters.wait_for(writes, &engine.view(), 1u, ample, sequence), Status::ok);
  CHECK_EQ(script.ran, 2u);
  CHECK_EQ(sequence, 2u);
  CHECK_EQ(waiters.waits(), 1u);
  CHECK_EQ(waiters.timeouts(), 0u); }

TEST(present_waiters, a_wake_that_finds_a_present_open_again_is_one_more_look_too)
{ MockPlatform   platform;
  PresentWaiters waiters(platform);
  PresentWrites  writes(platform);
  EnginePage     engine(platform);
  writes.opening();
  writes.written(3u, 1u);
  Script script;
  script.platform = &platform;
  script.waiters = &waiters;
  script.writes = &writes;
  script.engine = &engine;
  script.steps[0] = [](Script &now) {
      now.engine->completed(3u);
      now.writes->opening();
      now.waiters->wake();
  };
  script.steps[1] = [](Script &now) {
      now.writes->written(4u, 1u);
      now.engine->completed(4u);
      now.waiters->wake();
  };
  script.count = 2u;
  script.install();
  std::uint64_t sequence = 0u;
  CHECK_STATUS(waiters.wait_for(writes, &engine.view(), 1u, ample, sequence), Status::ok);
  CHECK_EQ(sequence, 4u);
  CHECK_EQ(script.ran, 2u); }

TEST(present_waiters, a_present_opening_during_the_look_is_looked_at_again)
{ MockPlatform   platform;
  PresentWaiters waiters(platform);
  PresentWrites  writes(platform);
  struct Landing {
      PresentWrites *writes;
      std::uint32_t  loads;
      bool           landing;
  } landing{&writes, 0u, false};
  platform.on_load(
      [](void *context) {
          auto *const hook = static_cast<Landing *>(context);
          if (hook->landing || ++hook->loads != 5u) return;
          hook->landing = true;
          hook->writes->opening();
          hook->landing = false;
      },
      &landing);
  Script script;
  script.platform = &platform;
  script.waiters = &waiters;
  script.writes = &writes;
  script.steps[0] = [](Script &now) {
      now.writes->written(0u, 0u);
      now.waiters->wake();
  };
  script.count = 1u;
  script.install();
  std::uint64_t sequence = 0u;
  CHECK_STATUS(waiters.wait_for(writes, nullptr, 1u, ample, sequence), Status::ok);
  CHECK_EQ(sequence, 2u);
  CHECK_EQ(script.ran, 1u);
  CHECK_EQ(waiters.waits(), 1u);
  platform.on_load(nullptr, nullptr); }

TEST(present_waiters, the_last_look_is_taken_with_the_slot_held_so_a_present_finishing_as_it_is_taken_is_seen)
{ MockPlatform   platform;
  PresentWaiters waiters(platform);
  PresentWrites  writes(platform);
  writes.opening();
  struct Landing {
      PresentWrites *writes;
      std::uint32_t  locks;
  } landing{&writes, 0u};
  platform.on_acquire(
      [](void *context) {
          auto *const hook = static_cast<Landing *>(context);
          if (++hook->locks != 1u) return;
          hook->writes->written(0u, 0u);
      },
      &landing);
  std::uint64_t sequence = 0u;
  CHECK_STATUS(waiters.wait_for(writes, nullptr, 1u, ample, sequence), Status::ok);
  CHECK_EQ(sequence, 2u);
  CHECK_EQ(platform.slept_ticks(), 0u);
  platform.on_acquire(nullptr, nullptr); }

TEST(present_waiters, a_page_that_says_nothing_valid_is_a_ticket_that_has_not_completed)
{ MockPlatform   platform;
  PresentWaiters waiters(platform);
  PresentWrites  writes(platform);
  EnginePage     engine(platform);
  writes.opening();
  writes.written(1u, 1u);
  std::uint64_t sequence = 0u;
  CHECK_STATUS(waiters.wait_for(writes, &engine.view(), 1u, 1000u, sequence), Status::not_found);
  CHECK_EQ(waiters.timeouts(), 1u); }

TEST(present_waiters, gives_up_when_the_time_runs_out_and_gives_its_slot_back)
{ MockPlatform   platform;
  PresentWaiters waiters(platform);
  PresentWrites  writes(platform);
  writes.opening();
  std::uint64_t sequence = 77u;
  CHECK_STATUS(waiters.wait_for(writes, nullptr, 1u, 5000u, sequence), Status::not_found);
  CHECK_EQ(sequence, 77u);
  CHECK_EQ(platform.slept_ticks(), 5000u);
  CHECK_EQ(waiters.waits(), 1u);
  CHECK_EQ(waiters.timeouts(), 1u);
  CHECK(waiters.invariants_hold());
  char bytes[128];
  StateWriter writer(bytes, sizeof bytes);
  waiters.report(writer);
  CHECK(std::strcmp(writer.dump(), "present_waiters/0 busy=false waiting=0 waits=1 timeouts=1") == 0);
  for (std::uint32_t again = 0u; again < PresentWaiters::slots + 1u; again++)
      CHECK_STATUS(waiters.wait_for(writes, nullptr, 1u, 10u, sequence), Status::not_found);
  CHECK_EQ(waiters.timeouts(), 1u + PresentWaiters::slots + 1u);
  writes.written(0u, 0u);
  CHECK_STATUS(waiters.wait_for(writes, nullptr, 1u, 10u, sequence), Status::ok);
  CHECK_EQ(sequence, 2u);
  CHECK_EQ(platform.unclaimed_violations(), 0u); }

TEST(present_waiters, a_wait_of_no_time_refuses_unsettled_pages_without_sleeping)
{ MockPlatform   platform;
  PresentWaiters waiters(platform);
  PresentWrites  writes(platform);
  writes.opening();
  std::uint64_t sequence = 0u;
  CHECK_STATUS(waiters.wait_for(writes, nullptr, 1u, 0u, sequence), Status::not_found);
  CHECK_EQ(platform.slept_ticks(), 0u);
  CHECK_EQ(waiters.waits(), 1u);
  CHECK_EQ(waiters.timeouts(), 1u); }

TEST(present_waiters, a_wait_that_never_ends_is_bounded_by_the_clock_and_not_wrapped)
{ MockPlatform   platform;
  PresentWaiters waiters(platform);
  PresentWrites  writes(platform);
  writes.opening();
  platform.advance_time(10u);
  Script script;
  script.platform = &platform;
  script.waiters = &waiters;
  script.writes = &writes;
  script.steps[0] = [](Script &now) {
      now.writes->written(0u, 0u);
      now.waiters->wake();
  };
  script.count = 1u;
  script.install();
  std::uint64_t sequence = 0u;
  CHECK_STATUS(waiters.wait_for(writes, nullptr, 1u, ~std::uint64_t{0}, sequence), Status::ok);
  CHECK_EQ(sequence, 2u);
  CHECK_EQ(platform.unclaimed_violations(), 0u); }

TEST(present_waiters, more_sleepers_than_slots_leaves_the_last_to_be_told_no_space)
{ MockPlatform   platform;
  PresentWaiters waiters(platform);
  PresentWrites  writes(platform);
  writes.opening();
  Script script;
  script.platform = &platform;
  script.waiters = &waiters;
  script.writes = &writes;
  const Script::Step sleeper = [](Script &now) {
      const std::uint32_t at = now.ran - 1u;
      now.nested[at] = now.waiters->wait_for(*now.writes, nullptr, 1u, ample, now.nested_sequence[at]);
  };
  script.steps[0] = sleeper;
  script.steps[1] = sleeper;
  script.steps[2] = sleeper;
  script.steps[3] = [](Script &now) {
      now.nested[3] = now.waiters->wait_for(*now.writes, nullptr, 1u, ample, now.nested_sequence[3]);
      char bytes[128];
      StateWriter writer(bytes, sizeof bytes);
      now.waiters->report(writer);
      CHECK(std::strcmp(writer.dump(), "present_waiters/0 busy=false waiting=4 waits=0 timeouts=0") == 0);
      now.writes->written(0u, 0u);
      now.waiters->wake();
  };
  script.count = 4u;
  script.install();
  std::uint64_t sequence = 0u;
  CHECK_STATUS(waiters.wait_for(writes, nullptr, 1u, ample, sequence), Status::ok);
  CHECK_EQ(sequence, 2u);
  CHECK_EQ(script.ran, 4u);
  CHECK_STATUS(script.nested[0], Status::ok);
  CHECK_STATUS(script.nested[1], Status::ok);
  CHECK_STATUS(script.nested[2], Status::ok);
  CHECK_STATUS(script.nested[3], Status::no_space);
  CHECK_EQ(script.nested_sequence[0], 2u);
  CHECK_EQ(script.nested_sequence[1], 2u);
  CHECK_EQ(script.nested_sequence[2], 2u);
  CHECK_EQ(waiters.waits(), 4u);
  CHECK_EQ(waiters.timeouts(), 0u);
  char bytes[128];
  StateWriter writer(bytes, sizeof bytes);
  waiters.report(writer);
  CHECK(std::strcmp(writer.dump(), "present_waiters/0 busy=false waiting=0 waits=4 timeouts=0") == 0);
  CHECK(waiters.invariants_hold());
  CHECK_EQ(platform.unclaimed_violations(), 0u); }

TEST(present_waiters, a_ticket_of_an_engine_that_is_gone_is_not_waited_for)
{ MockPlatform   platform;
  PresentWaiters waiters(platform);
  PresentWrites  writes(platform);
  EnginePage     engine(platform);
  writes.opening();
  writes.written(500u, 1u);
  engine.completed(2u);
  std::uint64_t sequence = 0u;
  CHECK_STATUS(waiters.wait_for(writes, &engine.view(), 2u, ample, sequence), Status::ok);
  CHECK_EQ(sequence, 2u);
  CHECK_EQ(platform.slept_ticks(), 0u);
  CHECK_EQ(waiters.waits(), 0u);
  CHECK_STATUS(waiters.wait_for(writes, &engine.view(), 1u, 1000u, sequence), Status::not_found);
  CHECK_EQ(waiters.timeouts(), 1u); }

TEST(present_waiters, a_present_is_admitted_at_once_when_no_read_is_open_and_is_opened)
{ MockPlatform   platform;
  PresentWaiters waiters(platform);
  PresentWrites  writes(platform);
  PresentReads   reads(platform);
  CHECK_STATUS(waiters.admit_write(reads, writes, ample), Status::ok);
  CHECK_EQ(writes.sequence(), 1u);
  CHECK(writes.in_progress(writes.sequence()));
  CHECK_EQ(platform.slept_ticks(), 0u);
  CHECK_EQ(waiters.holds(), 0u);
  CHECK_EQ(waiters.hold_timeouts(), 0u);
  CHECK(waiters.invariants_hold());
  CHECK_EQ(platform.locks_held(), 0u);
  CHECK_EQ(platform.unclaimed_violations(), 0u); }

TEST(present_waiters, a_present_is_held_while_a_read_is_open_and_goes_in_when_it_ends)
{ MockPlatform   platform;
  PresentWaiters waiters(platform);
  PresentWrites  writes(platform);
  PresentReads   reads(platform);
  CHECK(!reads.open());
  Script script;
  script.platform = &platform;
  script.waiters = &waiters;
  script.writes = &writes;
  script.reads = &reads;
  script.steps[0] = [](Script &now) {
      CHECK_EQ(now.writes->sequence(), 0u);
      now.waiters->wake();
  };
  script.steps[1] = [](Script &now) {
      now.reads->close();
      now.waiters->wake();
  };
  script.count = 2u;
  script.install();
  CHECK_STATUS(waiters.admit_write(reads, writes, ample), Status::ok);
  CHECK_EQ(script.ran, 2u);
  CHECK_EQ(writes.sequence(), 1u);
  CHECK_EQ(platform.slept_ticks(), 0u);
  CHECK_EQ(waiters.holds(), 1u);
  CHECK_EQ(waiters.hold_timeouts(), 0u);
  CHECK_EQ(waiters.waits(), 0u);
  CHECK(waiters.invariants_hold());
  CHECK_EQ(platform.unclaimed_violations(), 0u); }

TEST(present_waiters, a_present_held_past_its_time_is_told_so_and_is_not_opened)
{ MockPlatform   platform;
  PresentWaiters waiters(platform);
  PresentWrites  writes(platform);
  PresentReads   reads(platform);
  CHECK(!reads.open());
  CHECK_STATUS(waiters.admit_write(reads, writes, 5000u), Status::not_found);
  CHECK_EQ(platform.slept_ticks(), 5000u);
  CHECK_EQ(writes.sequence(), 0u);
  CHECK_EQ(waiters.holds(), 1u);
  CHECK_EQ(waiters.hold_timeouts(), 1u);
  CHECK_STATUS(waiters.admit_write(reads, writes, 0u), Status::not_found);
  CHECK_EQ(platform.slept_ticks(), 5000u);
  CHECK_EQ(waiters.hold_timeouts(), 2u);
  CHECK_EQ(waiters.timeouts(), 0u);
  char        bytes[128];
  StateWriter writer(bytes, sizeof bytes);
  waiters.report(writer);
  CHECK(std::strcmp(writer.dump(), "present_waiters/0 busy=false waiting=0 waits=0 timeouts=0") == 0);
  CHECK(waiters.invariants_hold()); }

TEST(present_waiters, a_read_that_expires_while_a_present_waits_lets_it_in)
{ MockPlatform   platform;
  PresentWaiters waiters(platform);
  PresentWrites  writes(platform);
  PresentReads   reads(platform);
  CHECK(!reads.open());
  CHECK_STATUS(waiters.admit_write(reads, writes, 2u * PresentReads::expiry_ticks), Status::ok);
  CHECK_EQ(writes.sequence(), 1u);
  CHECK_EQ(waiters.holds(), 1u);
  CHECK_EQ(waiters.hold_timeouts(), 0u); }

TEST(present_waiters, a_present_beyond_the_slots_is_told_no_space_and_is_not_opened)
{ MockPlatform   platform;
  PresentWaiters waiters(platform);
  PresentWrites  writes(platform);
  PresentReads   reads(platform);
  CHECK(!reads.open());
  Script script;
  script.platform = &platform;
  script.waiters = &waiters;
  script.writes = &writes;
  script.reads = &reads;
  const Script::Step sleeper = [](Script &now) {
      const std::uint32_t at = now.ran - 1u;
      now.nested[at] = now.waiters->admit_write(*now.reads, *now.writes, ample);
  };
  script.steps[0] = sleeper;
  script.steps[1] = sleeper;
  script.steps[2] = sleeper;
  script.steps[3] = [](Script &now) {
      now.nested[3] = now.waiters->admit_write(*now.reads, *now.writes, ample);
      now.reads->close();
      now.waiters->wake();
  };
  script.count = 4u;
  script.install();
  CHECK_STATUS(waiters.admit_write(reads, writes, ample), Status::ok);
  CHECK_STATUS(script.nested[0], Status::ok);
  CHECK_STATUS(script.nested[1], Status::ok);
  CHECK_STATUS(script.nested[2], Status::ok);
  CHECK_STATUS(script.nested[3], Status::no_space);
  CHECK_EQ(writes.sequence(), 4u);
  CHECK_EQ(waiters.holds(), 4u);
  CHECK_EQ(waiters.hold_timeouts(), 0u);
  CHECK(waiters.invariants_hold());
  CHECK_EQ(platform.unclaimed_violations(), 0u); }

TEST(present_waiters, admitting_a_present_is_passive_only)
{ MockPlatform   platform;
  PresentWaiters waiters(platform);
  PresentWrites  writes(platform);
  PresentReads   reads(platform);
  platform.set_irql(Irql::dispatch);
  CHECK_STATUS(waiters.admit_write(reads, writes, ample), Status::ok);
  platform.set_irql(Irql::passive);
  CHECK(platform.claim(Violation::irql_ceiling));
  CHECK_EQ(platform.unclaimed_violations(), 0u); }

TEST(present_waiters_report, counts_the_readers_asleep_and_says_when_the_lock_is_busy)
{ MockPlatform   platform;
  PresentWaiters waiters(platform);
  PresentWrites  writes(platform);
  char           bytes[128];
  StateWriter    quiet(bytes, sizeof bytes);
  waiters.report(quiet);
  CHECK(std::strcmp(quiet.dump(), "present_waiters/0 busy=false waiting=0 waits=0 timeouts=0") == 0);
  CHECK_EQ(platform.locks_held(), 0u);
  platform.hold_elsewhere(LockRank::present_waiters);
  StateWriter busy(bytes, sizeof bytes);
  waiters.report(busy);
  CHECK(std::strcmp(busy.dump(), "present_waiters/0 busy=true") == 0);
  platform.released_elsewhere();
  CHECK_EQ(platform.unclaimed_violations(), 0u); }

TEST(present_waiters, is_woken_from_dispatch_by_the_dpc_and_counts_and_reports_there)
{ MockPlatform   platform;
  PresentWaiters waiters(platform);
  PresentWrites  writes(platform);
  writes.opening();
  Script script;
  script.platform = &platform;
  script.waiters = &waiters;
  script.writes = &writes;
  script.steps[0] = [](Script &now) {
      now.platform->set_irql(Irql::dispatch);
      now.writes->written(0u, 0u);
      now.waiters->wake();
      char        bytes[128];
      StateWriter writer(bytes, sizeof bytes);
      now.waiters->report(writer);
      CHECK(std::strcmp(writer.dump(), "present_waiters/0 busy=false waiting=1 waits=0 timeouts=0") == 0);
      now.platform->set_irql(Irql::passive);
  };
  script.count = 1u;
  script.install();
  std::uint64_t sequence = 0u;
  CHECK_STATUS(waiters.wait_for(writes, nullptr, 1u, ample, sequence), Status::ok);
  CHECK_EQ(sequence, 2u);
  platform.set_irql(Irql::dispatch);
  waiters.wake();
  CHECK_EQ(waiters.waits(), 1u);
  CHECK_EQ(waiters.timeouts(), 0u);
  platform.set_irql(Irql::passive);
  CHECK_EQ(platform.unclaimed_violations(), 0u); }

TEST(present_waiters, waiting_is_passive_only)
{ MockPlatform   platform;
  PresentWaiters waiters(platform);
  PresentWrites  writes(platform);
  std::uint64_t  sequence = 0u;
  platform.set_irql(Irql::dispatch);
  CHECK_STATUS(waiters.wait_for(writes, nullptr, 1u, ample, sequence), Status::ok);
  platform.set_irql(Irql::passive);
  CHECK(platform.claim(Violation::irql_ceiling));
  CHECK_EQ(platform.unclaimed_violations(), 0u); }

TEST(present_waiters, the_counters_and_the_dump_are_legal_at_dispatch_and_not_above)
{ MockPlatform   platform;
  PresentWaiters waiters(platform);
  char           bytes[128];
  StateWriter    writer(bytes, sizeof bytes);
  platform.set_irql(Irql::dispatch);
  CHECK_EQ(waiters.waits(), 0u);
  CHECK_EQ(waiters.timeouts(), 0u);
  CHECK_EQ(waiters.holds(), 0u);
  CHECK_EQ(waiters.hold_timeouts(), 0u);
  waiters.wake();
  waiters.report(writer);
  CHECK_EQ(platform.unclaimed_violations(), 0u);
  platform.set_irql(Irql::isr);
  CHECK_EQ(waiters.waits(), 0u);
  CHECK(ceiling_broken(platform));
  CHECK_EQ(waiters.timeouts(), 0u);
  CHECK(ceiling_broken(platform));
  CHECK_EQ(waiters.holds(), 0u);
  CHECK(ceiling_broken(platform));
  CHECK_EQ(waiters.hold_timeouts(), 0u);
  CHECK(ceiling_broken(platform));
  waiters.wake();
  CHECK(ceiling_broken(platform));
  StateWriter above(bytes, sizeof bytes);
  waiters.report(above);
  CHECK(ceiling_broken(platform));
  platform.set_irql(Irql::passive);
  CHECK_EQ(platform.unclaimed_violations(), 0u); }

TEST(present_waiters_invariants, more_timeouts_than_waits_is_detected)
{ MockPlatform   platform;
  PresentWaiters waiters(platform);
  CHECK(waiters.invariants_hold());
  PresentWaitersProbe::time_out_without_waiting(waiters);
  CHECK(!waiters.invariants_hold()); }

TEST(present_waiters_invariants, more_hold_timeouts_than_holds_is_detected)
{ MockPlatform   platform;
  PresentWaiters waiters(platform);
  PresentWaitersProbe::time_out_a_hold_without_waiting(waiters);
  CHECK(!waiters.invariants_hold()); }
