#include "core/trace/state_writer.hpp"
#include "executor/channel_stats_latest.hpp"
#include "harness/framework/hostile_sweep.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/submission/channel_counter.hpp"
#include "proto/submission/channel_stats_report.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

using av::ChannelCounter;
using av::ChannelStatsLatest;
using av::ChannelStatsReport;
using av::StateWriter;
using av::Status;

namespace {

class Body {
public:
    Body(std::uint64_t base)
    {
        ChannelStatsReport report;
        for (std::uint32_t index = 0u; index < ChannelStatsReport::counters; index++)
            report.set(static_cast<ChannelCounter>(index), base + index);
        CHECK_STATUS(report.encode(bytes_, sizeof bytes_), Status::ok);
    }
    const std::uint8_t *bytes() const { return bytes_; }
    std::uint8_t       *mutable_bytes() { return bytes_; }

private:
    std::uint8_t bytes_[ChannelStatsReport::encoded_size] = {};
};

class Dump {
public:
    explicit Dump(const ChannelStatsLatest &latest, std::uint32_t parked = 0u)
        : writer_(text_, sizeof text_)
    {
        latest.report(writer_, parked);
    }
    bool has(const char *needle) const { return std::strstr(writer_.dump(), needle) != nullptr; }
    const char *text() const { return writer_.dump(); }

private:
    char        text_[2048] = {};
    StateWriter writer_;
};

}

TEST(channel_stats_latest, nothing_has_been_received_at_first)
{ ChannelStatsLatest latest;
  CHECK(!latest.received());
  CHECK(latest.invariants_hold());
  const Dump dump(latest);
  CHECK(dump.has("channel/0 reports=0 rejected=0 split_transfers=0")); }

TEST(channel_stats_latest_accept, keeps_the_newest_report_and_the_context_it_came_from)
{ ChannelStatsLatest latest;
  const Body        first(100u);
  const Body        second(5000u);
  latest.accept(7u, first.bytes(), ChannelStatsReport::encoded_size);
  CHECK(latest.received());
  CHECK(latest.invariants_hold());
  latest.accept(9u, second.bytes(), ChannelStatsReport::encoded_size);
  CHECK(latest.invariants_hold());
  const Dump dump(latest, 3u);
  CHECK(dump.has("channel/9 reports=2 rejected=0"));
  CHECK(dump.has("split_transfers=5000 split_pieces=5001 blob_room_waits=5002 blob_room_wait_us=5003 ring_waits=5004 "
                 "ring_wait_us=5005 fence_waits=5006 fence_wait_us=5007"));
  CHECK(dump.has("arena_grows=5014 arena_bytes=5015 arena_peak_ring_bytes=5016 arena_set_aside_bytes=5017 "
                 "arena_segments=5018 parked_blobs=3")); }

TEST(channel_stats_latest_accept, a_malformed_report_is_counted_and_changes_nothing_else)
{ ChannelStatsLatest latest;
  const Body        good(100u);
  latest.accept(3u, good.bytes(), ChannelStatsReport::encoded_size);
  Body bad(900u);
  bad.mutable_bytes()[0] = 9u;
  latest.accept(4u, bad.bytes(), ChannelStatsReport::encoded_size);
  latest.accept(4u, good.bytes(), ChannelStatsReport::encoded_size - 1u);
  latest.accept(4u, good.bytes(), ChannelStatsReport::encoded_size + 1u);
  latest.accept(4u, nullptr, ChannelStatsReport::encoded_size);
  latest.accept(4u, good.bytes(), 0u);
  CHECK(latest.invariants_hold());
  const Dump dump(latest);
  CHECK(dump.has("channel/3 reports=1 rejected=5"));
  CHECK(dump.has("split_transfers=100 ")); }

TEST(channel_stats_latest_accept, only_rejections_still_count_as_received)
{ ChannelStatsLatest latest;
  latest.accept(1u, nullptr, 0u);
  CHECK(latest.received());
  CHECK(latest.invariants_hold());
  const Dump dump(latest);
  CHECK(dump.has("channel/0 reports=0 rejected=1")); }

TEST(channel_stats_latest_accept, a_counter_keeps_all_sixty_four_bits)
{ ChannelStatsReport report;
  report.set(ChannelCounter::blob_room_wait_us, 0xFFFFFFFFFFFFFFFFull);
  std::uint8_t bytes[ChannelStatsReport::encoded_size] = {};
  CHECK_STATUS(report.encode(bytes, sizeof bytes), Status::ok);
  ChannelStatsLatest latest;
  latest.accept(1u, bytes, sizeof bytes);
  const Dump dump(latest);
  CHECK(dump.has("blob_room_wait_us=18446744073709551615")); }

TEST(channel_stats_latest_reset, forgets_every_report_and_every_value)
{ ChannelStatsLatest latest;
  const Body        body(100u);
  latest.accept(5u, body.bytes(), ChannelStatsReport::encoded_size);
  latest.accept(5u, nullptr, 0u);
  latest.reset();
  CHECK(!latest.received());
  CHECK(latest.invariants_hold());
  const Dump dump(latest);
  CHECK(dump.has("channel/0 reports=0 rejected=0 split_transfers=0"));
  CHECK(dump.has("arena_set_aside_bytes=0 arena_segments=0 parked_blobs=0")); }

TEST(channel_stats_latest_reset, a_reset_collector_accepts_again)
{ ChannelStatsLatest latest;
  const Body        body(7u);
  latest.accept(5u, body.bytes(), ChannelStatsReport::encoded_size);
  latest.reset();
  latest.accept(6u, body.bytes(), ChannelStatsReport::encoded_size);
  CHECK(latest.received());
  const Dump dump(latest);
  CHECK(dump.has("channel/6 reports=1 rejected=0 split_transfers=7")); }

TEST(channel_stats_latest, the_report_names_every_counter_once_in_order)
{ ChannelStatsLatest latest;
  const Dump           dump(latest);
  const char *const    names[] = {"split_transfers", "split_pieces", "blob_room_waits", "blob_room_wait_us", "ring_waits",
                                  "ring_wait_us", "fence_waits", "fence_wait_us", "inline_bulk",
                                  "copied_mappings", "slice_waits", "refused_no_space", "refused_no_memory",
                                  "refused_other", "arena_grows", "arena_bytes", "arena_peak_ring_bytes",
                                  "arena_set_aside_bytes", "arena_segments"};
  CHECK_EQ(sizeof names / sizeof names[0], ChannelStatsReport::counters);
  const char *last = dump.text();
  for (const char *name : names) {
      const char *const found = std::strstr(last, name);
      CHECK(found != nullptr);
      if (found != nullptr) last = found;
  } }

TEST(channel_stats_latest_accept, a_hostile_sweep_never_leaves_a_report_half_accepted)
{ static ChannelStatsLatest *held;
  const Body                  body(40u);
  ChannelStatsLatest          latest;
  held = &latest;
  const av::HostileSweep sweep(
      [](const std::uint8_t *bytes, std::size_t length) {
          held->accept(2u, bytes, length);
          return !held->invariants_hold();
      },
      body.bytes(), ChannelStatsReport::encoded_size);
  sweep.run();
  CHECK(sweep.cases_run() > 0u);
  CHECK_EQ(sweep.rejections(), 0u);
  CHECK(latest.invariants_hold()); }
