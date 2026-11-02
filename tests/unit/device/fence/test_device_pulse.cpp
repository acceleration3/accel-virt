#include "core/support/object_storage.hpp"
#include "core/support/status.hpp"
#include "device/fence/device_pulse.hpp"
#include "device/fence/fence_interrupts.hpp"
#include "device/fence/fence_reporter.hpp"
#include "device/fence/fence_window.hpp"
#include "device/memory/blob_window.hpp"
#include "device/recording/wire_recorder.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/host/host_violation.hpp"
#include "harness/host/mock_host.hpp"
#include "proto/submission/fault_code.hpp"
#include "proto/submission/fence_page.hpp"

#include <cstdint>
#include <cstring>
#include <sched.h>

using av::BlobWindow;
using av::DevicePulse;
using av::FaultCode;
using av::FenceInterrupts;
using av::FencePage;
using av::FenceReporter;
using av::FenceWindow;
using av::HostViolation;
using av::MockHost;
using av::Status;
using av::WireRecorder;

namespace av {

struct DevicePulseProbe {
    static void stop(DevicePulse &pulse) { pulse.stopping_ = true; }
    static bool dormant(const DevicePulse &pulse) { return __atomic_load_n(&pulse.dormant_, __ATOMIC_SEQ_CST); }
    static void carry_twice(DevicePulse &pulse, FenceReporter &reporter)
    {
        pulse.carried_[0] = &reporter;
        pulse.carried_[1] = &reporter;
    }
    static void lose_lock(DevicePulse &pulse, void *&lock)
    {
        lock = pulse.lock_;
        pulse.lock_ = nullptr;
        pulse.thread_ = &pulse;
    }
    static void restore(DevicePulse &pulse, void *lock)
    {
        pulse.lock_ = lock;
        pulse.thread_ = nullptr;
    }
};

}

using av::DevicePulseProbe;

namespace {
class Page {
public:
    Page(MockHost &host, FenceInterrupts &interrupts, WireRecorder &recorder, std::uint32_t id)
        : bytes_{}, pointer_(bytes_), length_(sizeof bytes_), reporter_(host, interrupts, recorder, id)
    {
        CHECK_STATUS(reporter_.prepare(FenceWindow(BlobWindow(&pointer_, &length_, 1u, length_)), 0u), Status::ok);
    }

    FenceReporter &reporter() { return reporter_; }
    std::uint64_t  beat() const
    {
        FencePage page;
        static_cast<void>(page.decode(bytes_, sizeof bytes_));
        return page.beat();
    }

private:
    std::uint8_t  bytes_[FencePage::encoded_size];
    void         *pointer_;
    std::uint64_t length_;
    FenceReporter reporter_;
};

class Fixture {
public:
    Fixture() : interrupts_(host_), recorder_(host_), pulse_(host_), first_(host_, interrupts_, recorder_, 1u),
                second_(host_, interrupts_, recorder_, 2u)
    {
        static_cast<void>(interrupts_.initialize());
    }

    MockHost    &host() { return host_; }
    DevicePulse &pulse() { return pulse_; }
    Page        &first() { return first_; }
    Page        &second() { return second_; }
    FenceInterrupts &interrupts() { return interrupts_; }
    WireRecorder &recorder() { return recorder_; }

private:
    MockHost        host_;
    FenceInterrupts interrupts_;
    WireRecorder    recorder_;
    DevicePulse     pulse_;
    Page            first_;
    Page            second_;
};

}

TEST(device_pulse_initialize, makes_its_lock_and_event_once)
{ Fixture fixture;
  CHECK_STATUS(fixture.pulse().initialize(), Status::ok);
  CHECK(fixture.pulse().invariants_hold());
  CHECK_STATUS(fixture.pulse().initialize(), Status::invalid_state); }

TEST(device_pulse_initialize, a_host_with_no_mutex_or_event_leaves_nothing_made)
{ Fixture no_mutex;
  no_mutex.host().fail_mutex_at(1u);
  CHECK_STATUS(no_mutex.pulse().initialize(), Status::no_memory);
  CHECK_STATUS(no_mutex.pulse().carry(no_mutex.first().reporter()), Status::invalid_state);
  CHECK_STATUS(no_mutex.pulse().initialize(), Status::ok);
  Fixture no_event;
  const std::uint32_t mutexes = no_event.host().live_mutexes();
  no_event.host().fail_event_at(1u);
  CHECK_STATUS(no_event.pulse().initialize(), Status::no_memory);
  CHECK_EQ(no_event.host().live_mutexes(), mutexes);
  CHECK_STATUS(no_event.pulse().initialize(), Status::ok); }

TEST(device_pulse_carry, carries_each_page_once_and_drops_it)
{ Fixture fixture;
  CHECK_STATUS(fixture.pulse().carry(fixture.first().reporter()), Status::invalid_state);
  CHECK_STATUS(fixture.pulse().initialize(), Status::ok);
  CHECK_STATUS(fixture.pulse().carry(fixture.first().reporter()), Status::ok);
  CHECK_STATUS(fixture.pulse().carry(fixture.first().reporter()), Status::invalid_state);
  CHECK_STATUS(fixture.pulse().carry(fixture.second().reporter()), Status::ok);
  CHECK_EQ(fixture.pulse().carried(), 2u);
  CHECK(fixture.pulse().invariants_hold());
  fixture.pulse().drop(fixture.first().reporter());
  fixture.pulse().drop(fixture.first().reporter());
  CHECK_EQ(fixture.pulse().carried(), 1u);
  CHECK_STATUS(fixture.pulse().carry(fixture.first().reporter()), Status::ok);
  CHECK_EQ(fixture.pulse().carried(), 2u); }

TEST(device_pulse_carry, a_full_pulse_carries_no_more)
{ Fixture fixture;
  CHECK_STATUS(fixture.pulse().initialize(), Status::ok);
  static av::ObjectStorage<FenceReporter> reporters[DevicePulse::capacity];
  for (std::uint32_t index = 0u; index < DevicePulse::capacity; index++) {
      CHECK_STATUS(reporters[index].construct(fixture.host(), fixture.interrupts(), fixture.recorder(), index + 3u),
                   Status::ok);
      CHECK_STATUS(fixture.pulse().carry(reporters[index].object()), Status::ok);
  }
  CHECK_STATUS(fixture.pulse().carry(fixture.first().reporter()), Status::no_space);
  CHECK_EQ(fixture.pulse().carried(), DevicePulse::capacity);
  CHECK(fixture.pulse().invariants_hold());
  for (av::ObjectStorage<FenceReporter> &reporter : reporters) {
      fixture.pulse().drop(reporter.object());
      reporter.destroy();
  }
  CHECK_EQ(fixture.pulse().carried(), 0u); }

TEST(device_pulse_carry, a_thread_that_cannot_start_carries_nothing)
{ Fixture fixture;
  CHECK_STATUS(fixture.pulse().initialize(), Status::ok);
  fixture.host().fail_thread_start_at(1u);
  CHECK_STATUS(fixture.pulse().carry(fixture.first().reporter()), Status::no_memory);
  CHECK_EQ(fixture.pulse().carried(), 0u);
  CHECK_EQ(fixture.host().live_threads(), 0u);
  CHECK_STATUS(fixture.pulse().carry(fixture.first().reporter()), Status::ok);
  CHECK_EQ(fixture.host().live_threads(), 1u); }

TEST(device_pulse_tick, one_context_at_work_beats_every_page)
{ Fixture fixture;
  CHECK_STATUS(fixture.pulse().initialize(), Status::ok);
  CHECK_STATUS(fixture.pulse().carry(fixture.first().reporter()), Status::ok);
  CHECK_STATUS(fixture.pulse().carry(fixture.second().reporter()), Status::ok);
  const std::uint64_t first = fixture.first().beat();
  const std::uint64_t second = fixture.second().beat();
  fixture.first().reporter().at_work(true);
  bool working = false;
  CHECK(fixture.pulse().tick(working));
  CHECK(working);
  CHECK(fixture.first().beat() > first);
  CHECK(fixture.second().beat() > second);
  CHECK(fixture.pulse().invariants_hold()); }

TEST(device_pulse_tick, nothing_at_work_or_nothing_carried_beats_nothing)
{ Fixture fixture;
  CHECK_STATUS(fixture.pulse().initialize(), Status::ok);
  CHECK_STATUS(fixture.pulse().carry(fixture.first().reporter()), Status::ok);
  CHECK_STATUS(fixture.pulse().carry(fixture.second().reporter()), Status::ok);
  bool working = true;
  CHECK(fixture.pulse().tick(working));
  CHECK(!working);
  CHECK_EQ(fixture.first().beat(), 0u);
  CHECK_EQ(fixture.second().beat(), 0u);
  fixture.pulse().drop(fixture.second().reporter());
  fixture.first().reporter().at_work(true);
  CHECK(fixture.pulse().tick(working));
  CHECK(fixture.first().beat() > 0u);
  CHECK_EQ(fixture.second().beat(), 0u); }

TEST(device_pulse_tick, a_faulted_page_is_not_beaten_while_others_are)
{ Fixture fixture;
  CHECK_STATUS(fixture.pulse().initialize(), Status::ok);
  CHECK_STATUS(fixture.pulse().carry(fixture.first().reporter()), Status::ok);
  CHECK_STATUS(fixture.pulse().carry(fixture.second().reporter()), Status::ok);
  fixture.second().reporter().fault(FaultCode::device_lost);
  CHECK(fixture.host().claim(HostViolation::worker_required));
  fixture.first().reporter().at_work(true);
  bool working = false;
  CHECK(fixture.pulse().tick(working));
  CHECK(fixture.first().beat() > 0u);
  CHECK_EQ(fixture.second().beat(), 0u); }

TEST(device_pulse_tick, a_stopping_pulse_beats_nothing_and_says_so)
{ Fixture fixture;
  CHECK_STATUS(fixture.pulse().initialize(), Status::ok);
  CHECK_STATUS(fixture.pulse().carry(fixture.first().reporter()), Status::ok);
  DevicePulseProbe::stop(fixture.pulse());
  fixture.first().reporter().at_work(true);
  bool working = false;
  CHECK(!fixture.pulse().tick(working));
  CHECK_EQ(fixture.first().beat(), 0u);
  CHECK(!fixture.pulse().rest());   }

TEST(device_pulse_rest, sleeps_only_while_nothing_works_and_a_worker_stirs_it)
{ Fixture fixture;
  CHECK_STATUS(fixture.pulse().initialize(), Status::ok);
  CHECK_STATUS(fixture.pulse().carry(fixture.first().reporter()), Status::ok);
  fixture.first().reporter().at_work(true);
  CHECK(!fixture.pulse().rest());
  CHECK(!DevicePulseProbe::dormant(fixture.pulse()));
  fixture.first().reporter().at_work(false);
  CHECK(fixture.pulse().rest());
  CHECK(DevicePulseProbe::dormant(fixture.pulse()));
  fixture.first().reporter().at_work(true);
  CHECK(!DevicePulseProbe::dormant(fixture.pulse())); }

TEST(device_pulse_stir, wakes_a_dormant_pulse_and_leaves_an_awake_one_be)
{ Fixture fixture;
  CHECK_STATUS(fixture.pulse().initialize(), Status::ok);
  CHECK(fixture.pulse().rest());
  CHECK(DevicePulseProbe::dormant(fixture.pulse()));
  fixture.pulse().stir();
  CHECK(!DevicePulseProbe::dormant(fixture.pulse()));
  fixture.pulse().stir();
  CHECK(!DevicePulseProbe::dormant(fixture.pulse())); }

TEST(device_pulse_stir, a_reporter_stirs_the_pulse_it_is_beaten_by)
{ Fixture fixture;
  CHECK_STATUS(fixture.pulse().initialize(), Status::ok);
  CHECK(fixture.pulse().rest());
  fixture.first().reporter().at_work(true);
  CHECK(DevicePulseProbe::dormant(fixture.pulse()));
  fixture.first().reporter().at_work(false);
  fixture.first().reporter().beat_by(fixture.pulse());
  fixture.first().reporter().at_work(false);
  CHECK(DevicePulseProbe::dormant(fixture.pulse()));
  fixture.first().reporter().at_work(true);
  CHECK(!DevicePulseProbe::dormant(fixture.pulse())); }

TEST(device_pulse_run, a_dormant_thread_wakes_and_beats_when_a_worker_goes_to_work)
{ Fixture fixture;
  CHECK_STATUS(fixture.pulse().initialize(), Status::ok);
  CHECK_STATUS(fixture.pulse().carry(fixture.first().reporter()), Status::ok);
  for (std::uint32_t spin = 0u; spin < 100000000u && !DevicePulseProbe::dormant(fixture.pulse()); spin++)
      sched_yield();
  CHECK(DevicePulseProbe::dormant(fixture.pulse()));
  CHECK_EQ(fixture.first().beat(), 0u);
  fixture.first().reporter().at_work(true);
  for (std::uint32_t spin = 0u; spin < 100000000u && fixture.first().beat() == 0u; spin++) sched_yield();
  CHECK(fixture.first().beat() > 0u); }

TEST(device_pulse_drop, the_last_page_dropped_stops_the_thread)
{ Fixture fixture;
  CHECK_STATUS(fixture.pulse().initialize(), Status::ok);
  CHECK_STATUS(fixture.pulse().carry(fixture.first().reporter()), Status::ok);
  CHECK_STATUS(fixture.pulse().carry(fixture.second().reporter()), Status::ok);
  CHECK_EQ(fixture.host().live_threads(), 1u);
  fixture.pulse().drop(fixture.first().reporter());
  CHECK_EQ(fixture.host().live_threads(), 1u);
  fixture.pulse().drop(fixture.second().reporter());
  CHECK_EQ(fixture.host().live_threads(), 0u);
  CHECK_STATUS(fixture.pulse().carry(fixture.first().reporter()), Status::ok);
  CHECK_EQ(fixture.host().live_threads(), 1u);
  fixture.first().reporter().at_work(true);
  bool working = false;
  CHECK(fixture.pulse().tick(working));
  CHECK(working); }

TEST(device_pulse_run, the_thread_beats_each_period_while_a_context_works)
{ MockHost host;
  {
      FenceInterrupts interrupts(host);
      WireRecorder    recorder(host);
      Page            page(host, interrupts, recorder, 1u);
      DevicePulse     pulse(host);
      CHECK_STATUS(pulse.initialize(), Status::ok);
      CHECK_STATUS(pulse.carry(page.reporter()), Status::ok);
      page.reporter().at_work(true);
      for (std::uint64_t period = 1u; period <= 2000u && page.beat() < 2u; period++) {
          host.set_clock(period * DevicePulse::period_ns);
          for (std::uint32_t spin = 0u; spin < 1000u && page.beat() < (period + 1u) / 2u; spin++) sched_yield();
      }
      CHECK(page.beat() >= 2u);
      pulse.drop(page.reporter());
  }
  CHECK_EQ(host.live_threads(), 0u); }

TEST(device_pulse, is_set_up_on_the_main_loop)
{ MockHost host;
  DevicePulse pulse(host);
  void *thread = nullptr;
  CHECK_STATUS(host.start_thread([](void *context) {
                   auto *const p = static_cast<DevicePulse *>(context);
                   static_cast<void>(p->initialize());
               },
                                 &pulse, thread),
               Status::ok);
  host.join_thread(thread);
  CHECK(host.claim(HostViolation::main_loop_required));
  CHECK_EQ(host.unclaimed_violations(), 0u); }

TEST(device_pulse_invariants, a_page_carried_twice_or_a_thread_with_no_lock_is_detected)
{ Fixture twice;
  DevicePulseProbe::carry_twice(twice.pulse(), twice.first().reporter());
  CHECK(!twice.pulse().invariants_hold());
  Fixture lockless;
  CHECK_STATUS(lockless.pulse().initialize(), Status::ok);
  void *lock = nullptr;
  DevicePulseProbe::lose_lock(lockless.pulse(), lock);
  CHECK(!lockless.pulse().invariants_hold());
  DevicePulseProbe::restore(lockless.pulse(), lock);
  CHECK(lockless.pulse().invariants_hold()); }
