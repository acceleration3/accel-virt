/* The device's vertical blanks, run on their own thread against the mock's
 * scripted clock. Each blank is published once it is due and not before,
 * every blank a late wake passed is counted, the frame posted last before a
 * blank is the one it latches and the one shown - once, on the main loop -
 * and the display line is raised on exactly the blanks the guest asked for.
 * A new rate takes over from the last blank; a reset stops the thread and
 * forgets every frame, and the count never goes back. */
#include "device/display/display_clock.hpp"
#include "device/fence/fence_interrupts.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/host/mock_host.hpp"
#include "proto/pages/clock_page_header.hpp"
#include "proto/pages/vblank_record.hpp"

#include <cstdint>
#include <cstring>
#include <time.h>

using av::ClockPageHeader;
using av::DisplayClock;
using av::FenceInterrupts;
using av::MockHost;
using av::Status;
using av::VblankRecord;

namespace {

constexpr std::uint64_t ram = 0x200000u;
constexpr std::uint64_t page = ram + 0x1000u;
constexpr std::uint64_t other_page = ram + 0x4000u;
constexpr std::uint64_t start_at = 1000u;
constexpr std::uint64_t period_60 = 16666667u;   /* at least one 60 Hz period */

/* What the main-loop routine saw: the clock it serves, and the keys it took. */
struct Shows {
    DisplayClock *clock;
    std::uint32_t runs;
    std::uint32_t taken;
    std::uint64_t keys[DisplayClock::heads];
};

void show(void *context)
{
    Shows &shows = *static_cast<Shows *>(context);
    shows.runs++;
    shows.clock->showing();
    for (std::uint32_t head = 0u; head < DisplayClock::heads; head++) {
        std::uint64_t key = 0u;
        if (!shows.clock->take_unshown(head, key)) continue;
        shows.keys[head] = key;
        shows.taken++;
    }
}

struct Rig {
    MockHost        host;
    FenceInterrupts interrupts;
    Shows           shows;
    DisplayClock    clock;

    Rig() : host(), interrupts(host), shows{}, clock(host, interrupts, &show, &shows)
    {
        shows.clock = &clock;
        host.add_guest_ram(ram, 0x10000u);
        host.set_clock(start_at);
    }
    ~Rig() { clock.detach(); }

    /* Head `head`'s record, whole: false when it never read an even generation
     * on both sides of the copy. */
    bool read(std::uint64_t at, std::uint32_t head, VblankRecord &record)
    {
        const std::uint8_t *const base = host.guest_ram(at + 32u + 64u * head);
        for (std::uint32_t attempt = 0u; attempt < 1000u; attempt++) {
            std::uint32_t before = 0u;
            std::uint32_t after = 0u;
            std::uint8_t  fields[VblankRecord::encoded_size] = {};
            std::memcpy(&before, base, sizeof before);
            std::memcpy(fields, base + VblankRecord::fields_at, sizeof fields);
            std::memcpy(&after, base, sizeof after);
            if (before == after && before % 2u == 0u) return record.decode(fields, sizeof fields) == Status::ok;
        }
        return false;
    }
    /* Waits up to two seconds for head `head` of the page at `at` to reach
     * `sequence`: the thread runs when the scripted clock moves. */
    bool reaches(std::uint64_t at, std::uint32_t head, std::uint64_t sequence, VblankRecord &record)
    {
        const timespec pause = {0, 100000};
        for (std::uint32_t waited = 0u; waited < 20000u; waited++) {
            if (read(at, head, record) && record.sequence() >= sequence) return true;
            nanosleep(&pause, nullptr);
        }
        return false;
    }
    void ask_every(std::uint64_t at, std::uint32_t head, std::uint32_t every)
    {
        std::memcpy(host.guest_ram(at + 32u + 64u * head + VblankRecord::interrupt_every_at), &every, sizeof every);
    }
};

}  // namespace

TEST(display_clock, a_registered_page_is_headed_and_every_head_published_before_its_first_blank)
{ Rig rig;
  CHECK_STATUS(rig.clock.initialize(), Status::ok);
  CHECK(!rig.clock.latches(0u));
  CHECK_STATUS(rig.clock.attach(page), Status::ok);
  CHECK(rig.clock.invariants_hold());
  CHECK_EQ(rig.host.live_threads(), 1u);
  ClockPageHeader header;
  CHECK_STATUS(header.decode(rig.host.guest_ram(page), ClockPageHeader::encoded_size), Status::ok);
  CHECK_EQ(header.heads(), DisplayClock::heads);
  for (std::uint32_t head = 0u; head < DisplayClock::heads; head++) {
      VblankRecord record;
      CHECK(rig.read(page, head, record));
      CHECK_EQ(record.sequence(), 0u);
      CHECK_EQ(record.latched(), 0u);
      CHECK_EQ(record.refresh_millihz(), 60000u);
      CHECK_EQ(record.vblank_ns(), start_at);
  }
  CHECK(rig.clock.latches(0u));
  CHECK(rig.clock.latches(DisplayClock::heads - 1u));
  CHECK(!rig.clock.latches(DisplayClock::heads));
  rig.clock.detach();
  CHECK_EQ(rig.host.live_threads(), 0u);
  CHECK_EQ(rig.host.live_guest_mappings(), 0u);
  CHECK(!rig.clock.latches(0u));
  CHECK(rig.clock.invariants_hold()); }

TEST(display_clock, each_blank_is_published_when_it_is_due_and_a_late_wake_counts_all_it_passed)
{ Rig rig;
  CHECK_STATUS(rig.clock.initialize(), Status::ok);
  CHECK_STATUS(rig.clock.attach(page), Status::ok);
  CHECK(rig.host.wait_for_sleepers(1u, 2000u));
  rig.host.set_clock(start_at + 16666665u);   /* a nanosecond early */
  CHECK(rig.host.wait_for_sleepers(1u, 2000u));
  VblankRecord record;
  CHECK(rig.read(page, 0u, record));
  CHECK_EQ(record.sequence(), 0u);
  rig.host.set_clock(start_at + 16666666u);
  CHECK(rig.reaches(page, 0u, 1u, record));
  CHECK_EQ(record.sequence(), 1u);
  CHECK_EQ(record.vblank_ns(), start_at + 16666666u);
  CHECK(rig.reaches(page, 15u, 1u, record));   /* every head keeps its own count */
  rig.host.set_clock(start_at + 5u * period_60 + 10u);
  CHECK(rig.reaches(page, 0u, 5u, record));
  CHECK_EQ(record.sequence(), 5u);
  CHECK_EQ(record.vblank_ns(), start_at + 83333333u);
  CHECK(rig.clock.invariants_hold()); }

TEST(display_clock, the_frame_posted_last_before_a_blank_is_latched_and_shown_once)
{ Rig rig;
  CHECK_STATUS(rig.clock.initialize(), Status::ok);
  CHECK_STATUS(rig.clock.attach(page), Status::ok);
  rig.clock.post(2u, 6u, 0x66u);
  rig.clock.post(2u, 7u, 0x77u);
  VblankRecord record;
  CHECK(rig.read(page, 2u, record));
  CHECK_EQ(record.latched(), 0u);   /* nothing moves before the blank */
  rig.host.set_clock(start_at + period_60);
  CHECK(rig.reaches(page, 2u, 1u, record));
  CHECK_EQ(record.latched(), 7u);
  CHECK(rig.host.wait_for_main_loop_work(1u, 2000u));
  CHECK_EQ(rig.host.run_main_loop(), 1u);
  CHECK_EQ(rig.shows.taken, 1u);
  CHECK_EQ(rig.shows.keys[2], 0x77u);
  CHECK(rig.read(page, 0u, record));
  CHECK_EQ(record.latched(), 0u);   /* other heads latched nothing */
  rig.host.set_clock(start_at + 2u * period_60);
  CHECK(rig.reaches(page, 2u, 2u, record));
  CHECK_EQ(record.latched(), 7u);   /* a blank with nothing new keeps the frame */
  CHECK(rig.host.wait_for_sleepers(1u, 2000u));
  CHECK_EQ(rig.host.run_main_loop(), 0u);   /* and shows it no second time */
  CHECK(rig.clock.invariants_hold()); }

TEST(display_clock, blanks_that_latch_before_the_show_runs_queue_it_once_and_it_shows_the_newest)
{ Rig rig;
  CHECK_STATUS(rig.clock.initialize(), Status::ok);
  CHECK_STATUS(rig.clock.attach(page), Status::ok);
  VblankRecord record;
  rig.clock.post(0u, 1u, 0x10u);
  rig.host.set_clock(start_at + period_60);
  CHECK(rig.reaches(page, 0u, 1u, record));
  rig.clock.post(0u, 2u, 0x20u);
  rig.clock.post(1u, 9u, 0x90u);
  rig.host.set_clock(start_at + 2u * period_60);
  CHECK(rig.reaches(page, 1u, 2u, record));
  CHECK_EQ(record.latched(), 9u);
  CHECK(rig.host.wait_for_sleepers(1u, 2000u));
  CHECK_EQ(rig.host.run_main_loop(), 1u);
  CHECK_EQ(rig.shows.taken, 2u);
  CHECK_EQ(rig.shows.keys[0], 0x20u);
  CHECK_EQ(rig.shows.keys[1], 0x90u);
  rig.clock.post(0u, 3u, 0x30u);   /* a latch after the show ran queues it again */
  rig.host.set_clock(start_at + 3u * period_60);
  CHECK(rig.host.wait_for_main_loop_work(1u, 2000u));
  CHECK_EQ(rig.host.run_main_loop(), 1u);
  CHECK_EQ(rig.shows.keys[0], 0x30u); }

TEST(display_clock, the_display_line_is_raised_on_the_blanks_the_guest_asked_for_and_no_others)
{ Rig rig;
  CHECK_STATUS(rig.interrupts.initialize(), Status::ok);
  CHECK_STATUS(rig.interrupts.connect(), Status::ok);
  CHECK_STATUS(rig.clock.initialize(), Status::ok);
  CHECK_STATUS(rig.clock.attach(page), Status::ok);
  VblankRecord record;
  rig.host.set_clock(start_at + period_60);
  CHECK(rig.reaches(page, 0u, 1u, record));
  CHECK_EQ(rig.host.raised(FenceInterrupts::display_line), 0u);   /* nobody asked */
  rig.ask_every(page, 0u, 2u);
  for (std::uint64_t blank = 2u; blank <= 5u; blank++) {
      rig.host.set_clock(start_at + blank * period_60);
      CHECK(rig.reaches(page, 0u, blank, record));
      CHECK(rig.host.wait_for_sleepers(1u, 2000u));
  }
  CHECK_EQ(rig.host.raised(FenceInterrupts::display_line), 2u);   /* blanks 2 and 4 */
  rig.ask_every(page, 0u, 1u);
  rig.ask_every(page, 3u, 1u);
  rig.host.set_clock(start_at + 9u * period_60);   /* four blanks at once, two heads asking */
  CHECK(rig.reaches(page, 3u, 9u, record));
  CHECK(rig.host.wait_for_sleepers(1u, 2000u));
  CHECK_EQ(rig.host.raised(FenceInterrupts::display_line), 3u);
  rig.ask_every(page, 0u, 0xFFFFFFFFu);   /* any value is a divisor: no blank reaches this one */
  rig.ask_every(page, 3u, 0u);
  rig.host.set_clock(start_at + 10u * period_60);
  CHECK(rig.reaches(page, 0u, 10u, record));
  CHECK(rig.host.wait_for_sleepers(1u, 2000u));
  CHECK_EQ(rig.host.raised(FenceInterrupts::display_line), 3u);
  for (std::uint32_t line = 0u; line < FenceInterrupts::display_line; line++) CHECK_EQ(rig.host.raised(line), 0u); }

TEST(display_clock, a_new_rate_takes_over_from_the_last_blank_and_the_earliest_head_sets_the_wake)
{ Rig rig;
  CHECK_STATUS(rig.clock.initialize(), Status::ok);
  CHECK_STATUS(rig.clock.attach(page), Status::ok);
  VblankRecord record;
  rig.host.set_clock(start_at + period_60);
  CHECK(rig.reaches(page, 5u, 1u, record));
  const std::uint64_t last = record.vblank_ns();
  CHECK_STATUS(rig.clock.retime(5u, 1000000u), Status::ok);   /* 1 kHz on head 5 alone */
  CHECK(rig.host.wait_for_sleepers(1u, 2000u));
  rig.host.set_clock(last + 1000000u);
  CHECK(rig.reaches(page, 5u, 2u, record));
  CHECK_EQ(record.refresh_millihz(), 1000000u);
  CHECK_EQ(record.period_ns(), 1000000u);
  CHECK_EQ(record.vblank_ns(), last + 1000000u);
  CHECK(rig.read(page, 0u, record));
  CHECK_EQ(record.sequence(), 1u);   /* head 0 is still waiting for its second */
  CHECK_EQ(record.refresh_millihz(), 60000u);
  CHECK(rig.clock.invariants_hold()); }

TEST(display_clock, rates_and_heads_the_protocol_does_not_allow_are_refused)
{ Rig rig;
  CHECK_STATUS(rig.clock.retime(0u, 60000u), Status::invalid_state);
  CHECK_STATUS(rig.clock.initialize(), Status::ok);
  CHECK_STATUS(rig.clock.retime(DisplayClock::heads, 60000u), Status::invalid_parameter);
  CHECK_STATUS(rig.clock.retime(0u, VblankRecord::least_millihz - 1u), Status::invalid_parameter);
  CHECK_STATUS(rig.clock.retime(0u, VblankRecord::most_millihz + 1u), Status::invalid_parameter);
  CHECK_STATUS(rig.clock.retime(0u, VblankRecord::least_millihz), Status::ok);
  CHECK_STATUS(rig.clock.retime(0u, VblankRecord::most_millihz), Status::ok);
  CHECK(rig.clock.invariants_hold()); }

TEST(display_clock, nothing_is_latched_without_a_page_and_a_reset_forgets_every_frame)
{ Rig rig;
  CHECK_STATUS(rig.clock.initialize(), Status::ok);
  rig.clock.post(0u, 4u, 0x40u);   /* no page: shown at once by the caller, never here */
  CHECK_STATUS(rig.clock.attach(page), Status::ok);
  VblankRecord record;
  rig.host.set_clock(start_at + period_60);
  CHECK(rig.reaches(page, 0u, 1u, record));
  CHECK_EQ(record.latched(), 0u);
  rig.clock.post(0u, 5u, 0x50u);
  rig.clock.detach();
  std::uint64_t key = 0u;
  CHECK(!rig.clock.take_unshown(0u, key));
  CHECK_STATUS(rig.clock.attach(page), Status::ok);
  CHECK(rig.read(page, 0u, record));
  CHECK_EQ(record.sequence(), 1u);   /* the count carries on across the reset */
  CHECK_EQ(record.latched(), 0u);
  rig.host.set_clock(start_at + 3u * period_60);
  CHECK(rig.reaches(page, 0u, 2u, record));
  CHECK_EQ(record.latched(), 0u);   /* the post before the reset is gone */
  CHECK_EQ(rig.host.run_main_loop(), 0u); }

TEST(display_clock, a_page_registered_again_is_published_by_the_same_thread)
{ Rig rig;
  CHECK_STATUS(rig.clock.initialize(), Status::ok);
  CHECK_STATUS(rig.clock.attach(page), Status::ok);
  CHECK_STATUS(rig.clock.attach(other_page), Status::ok);
  CHECK_EQ(rig.host.live_threads(), 1u);
  CHECK_EQ(rig.host.live_guest_mappings(), 1u);
  VblankRecord record;
  rig.host.set_clock(start_at + period_60);
  CHECK(rig.reaches(other_page, 0u, 1u, record));
  CHECK_STATUS(rig.clock.attach(0u), Status::ok);   /* none: publishing stops */
  CHECK_EQ(rig.host.live_threads(), 0u);
  CHECK_EQ(rig.host.live_guest_mappings(), 0u);
  CHECK(!rig.clock.latches(0u)); }

TEST(display_clock, a_clock_that_cannot_start_holds_nothing)
{ Rig rig;
  CHECK_STATUS(rig.clock.attach(page), Status::invalid_state);
  rig.clock.detach();
  rig.clock.showing();
  std::uint64_t key = 0u;
  CHECK(!rig.clock.take_unshown(0u, key));
  rig.host.fail_mutex_at(1u);
  CHECK_STATUS(rig.clock.initialize(), Status::no_memory);
  rig.host.fail_event_at(1u);
  CHECK_STATUS(rig.clock.initialize(), Status::no_memory);
  CHECK_EQ(rig.host.live_mutexes(), 0u);
  CHECK_STATUS(rig.clock.initialize(), Status::ok);
  CHECK_STATUS(rig.clock.initialize(), Status::invalid_state);
  CHECK_STATUS(rig.clock.attach(0x80000000u), Status::not_found);
  CHECK_EQ(rig.host.live_threads(), 0u);
  rig.host.fail_thread_start_at(1u);
  CHECK_STATUS(rig.clock.attach(page), Status::no_memory);
  CHECK_EQ(rig.host.live_threads(), 0u);
  CHECK_EQ(rig.host.live_guest_mappings(), 0u);
  CHECK(!rig.clock.take_unshown(DisplayClock::heads, key));
  CHECK(rig.clock.invariants_hold()); }

TEST(display_clock, a_stopped_clock_ticks_no_more_and_one_destroyed_running_stops_its_thread)
{ MockHost host;
  host.add_guest_ram(ram, 0x10000u);
  FenceInterrupts interrupts(host);
  Shows           shows{};
  {
      DisplayClock clock(host, interrupts, &show, &shows);
      shows.clock = &clock;
      CHECK_STATUS(clock.initialize(), Status::ok);
      std::uint64_t next = 0u;
      CHECK(clock.tick(start_at, next));   /* no page: counts nothing, and says when */
      CHECK_EQ(next, 16666666u);
      CHECK_STATUS(clock.attach(page), Status::ok);
      clock.detach();
      CHECK(!clock.tick(start_at, next));
      CHECK_STATUS(clock.attach(page), Status::ok);
      CHECK_EQ(host.live_threads(), 1u);
  }
  CHECK_EQ(host.live_threads(), 0u);
  CHECK_EQ(host.live_events(), 0u);
  CHECK_EQ(host.live_mutexes(), 0u); }

/* The host's display, through its export socket: every head takes its rate
 * and its phase from the next blank on, and a clock not yet made, or a rate
 * the protocol does not have, is refused. */
TEST(display_clock, aligned_every_head_blanks_in_phase_with_the_host_s_display)
{ Rig rig;
  constexpr std::uint64_t period_144 = 6944444u;
  CHECK_STATUS(rig.clock.align(144000u, 5u), Status::invalid_state);
  CHECK_STATUS(rig.clock.initialize(), Status::ok);
  CHECK_STATUS(rig.clock.align(VblankRecord::least_millihz - 1u, 5u), Status::invalid_parameter);
  CHECK_STATUS(rig.clock.align(VblankRecord::most_millihz + 1u, 5u), Status::invalid_parameter);
  CHECK_STATUS(rig.clock.attach(page), Status::ok);
  VblankRecord record;
  rig.host.set_clock(start_at + period_60);
  CHECK(rig.reaches(page, 0u, 1u, record));
  const std::uint64_t last = record.vblank_ns();
  CHECK_STATUS(rig.clock.align(144000u, last + 10u * period_144 + 77u), Status::ok);
  CHECK(rig.host.wait_for_sleepers(1u, 2000u));
  rig.host.set_clock(last + period_144 + 76u);
  CHECK(rig.read(page, 0u, record));
  CHECK_EQ(record.sequence(), 1u);
  rig.host.set_clock(last + period_144 + 77u);
  for (std::uint32_t head = 0u; head < DisplayClock::heads; head++) {
      CHECK(rig.reaches(page, head, 2u, record));
      CHECK_EQ(record.vblank_ns(), last + period_144 + 77u);
      CHECK_EQ(record.refresh_millihz(), 144000u);
      CHECK_EQ(record.period_ns(), period_144);
  }
  CHECK(rig.clock.invariants_hold()); }

/* A rate set live reaches the page from the next blank. */
TEST(display_clock, a_live_retime_of_every_head_reaches_the_page)
{ Rig rig;
  CHECK_STATUS(rig.clock.retime_all(999u), Status::invalid_parameter);
  CHECK_STATUS(rig.clock.initialize(), Status::ok);
  CHECK_STATUS(rig.clock.attach(page), Status::ok);
  VblankRecord record;
  rig.host.set_clock(start_at + period_60);
  CHECK(rig.reaches(page, 0u, 1u, record));
  CHECK_STATUS(rig.clock.retime_all(359980u), Status::ok);
  CHECK(rig.host.wait_for_sleepers(1u, 2000u));
  rig.host.set_clock(record.vblank_ns() + 2u * period_60);
  CHECK(rig.reaches(page, 5u, 3u, record));
  CHECK_EQ(record.refresh_millihz(), 359980u);
  CHECK(rig.clock.invariants_hold()); }
