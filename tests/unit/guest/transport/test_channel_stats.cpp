#include "core/support/status.hpp"
#include "guest/transport/channel_stats.hpp"
#include "harness/framework/guarded_buffer.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/client/client_blob_arena.hpp"
#include "proto/submission/channel_counter.hpp"
#include "proto/submission/channel_stats_report.hpp"
#include "proto/submission/command_header.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

using av::ChannelCounter;
using av::ChannelStats;
using av::ChannelStatsReport;
using av::ClientBlobArena;
using av::CommandHeader;
using av::Status;

namespace {

constexpr std::uint64_t interval = ChannelStats::interval;

std::uint64_t value_of(const ChannelStats &stats, ChannelCounter counter) { return stats.snapshot().value(counter); }

}

TEST(channel_stats, a_fresh_collector_is_zero_and_not_due)
{ const ChannelStats stats;
  CHECK(stats.invariants_hold());
  CHECK(!stats.due(0u));
  CHECK(!stats.due(100u * interval));
  for (std::uint32_t index = 0u; index < ChannelStatsReport::counters; index++)
      CHECK_EQ(value_of(stats, static_cast<ChannelCounter>(index)), 0u); }

TEST(channel_stats_note, adds_to_the_named_counter_only)
{ ChannelStats stats;
  stats.note(ChannelCounter::room_waits, 3u);
  stats.note(ChannelCounter::room_waits, 4u);
  stats.note(ChannelCounter::room_wait_us, 250u);
  CHECK_EQ(value_of(stats, ChannelCounter::room_waits), 7u);
  CHECK_EQ(value_of(stats, ChannelCounter::room_wait_us), 250u);
  CHECK_EQ(value_of(stats, ChannelCounter::inline_bulk), 0u);
  CHECK(stats.invariants_hold()); }

TEST(channel_stats_note, every_event_counter_accepts_a_note)
{ const ChannelCounter events[] = {ChannelCounter::split_transfers, ChannelCounter::split_pieces,
                                   ChannelCounter::room_waits,      ChannelCounter::room_wait_us,
                                   ChannelCounter::inline_bulk,     ChannelCounter::copied_mappings,
                                   ChannelCounter::slice_waits,     ChannelCounter::refused_no_space,
                                   ChannelCounter::refused_no_memory, ChannelCounter::refused_other};
  ChannelStats stats;
  for (const ChannelCounter counter : events) stats.note(counter, 1u);
  for (const ChannelCounter counter : events) CHECK_EQ(value_of(stats, counter), 1u); }

TEST(channel_stats_note, a_zero_amount_changes_nothing_and_is_not_a_reason_to_report)
{ ChannelStats stats;
  stats.note(ChannelCounter::room_waits, 0u);
  CHECK_EQ(value_of(stats, ChannelCounter::room_waits), 0u);
  CHECK(!stats.due(interval * 10u)); }

TEST(channel_stats_note, the_gauges_and_out_of_range_counters_cannot_be_noted)
{ ChannelStats stats;
  stats.note(ChannelCounter::arena_bytes, 9u);
  stats.note(ChannelCounter::arena_peak_ring_bytes, 9u);
  stats.note(ChannelCounter::arena_set_aside_bytes, 9u);
  stats.note(ChannelCounter::count, 9u);
  stats.note(static_cast<ChannelCounter>(0xFFFFFFFFu), 9u);
  for (std::uint32_t index = 0u; index < ChannelStatsReport::counters; index++)
      CHECK_EQ(value_of(stats, static_cast<ChannelCounter>(index)), 0u);
  CHECK(!stats.due(interval * 10u)); }

TEST(channel_stats_note, wraps_at_sixty_four_bits_without_disturbing_its_neighbours)
{ ChannelStats stats;
  stats.note(ChannelCounter::room_wait_us, ~0ull);
  stats.note(ChannelCounter::room_wait_us, 2u);
  CHECK_EQ(value_of(stats, ChannelCounter::room_wait_us), 1u);
  CHECK_EQ(value_of(stats, ChannelCounter::room_waits), 0u);
  CHECK_EQ(value_of(stats, ChannelCounter::inline_bulk), 0u); }

TEST(channel_stats_piece, the_second_piece_splits_a_transfer_and_every_later_one_adds_a_piece)
{ ChannelStats stats;
  stats.piece(0u);
  stats.piece(1u);
  CHECK_EQ(value_of(stats, ChannelCounter::split_transfers), 0u);
  CHECK_EQ(value_of(stats, ChannelCounter::split_pieces), 0u);
  stats.piece(2u);
  CHECK_EQ(value_of(stats, ChannelCounter::split_transfers), 1u);
  CHECK_EQ(value_of(stats, ChannelCounter::split_pieces), 1u);
  stats.piece(3u);
  stats.piece(4u);
  stats.piece(1000000u);
  CHECK_EQ(value_of(stats, ChannelCounter::split_transfers), 1u);
  CHECK_EQ(value_of(stats, ChannelCounter::split_pieces), 4u);
  CHECK(stats.invariants_hold()); }

TEST(channel_stats_observe, reads_the_arena_into_the_three_gauges)
{ std::uint8_t     block[4096];
  ClientBlobArena  arena(block, sizeof block);
  ChannelStats     stats;
  stats.observe(arena);
  CHECK_EQ(value_of(stats, ChannelCounter::arena_bytes), arena.lent());
  CHECK_EQ(value_of(stats, ChannelCounter::arena_peak_ring_bytes), arena.peak_ring());
  CHECK_EQ(value_of(stats, ChannelCounter::arena_set_aside_bytes), arena.set_aside_bytes());
  const av::ClientBlobLease lease = arena.take(1024u);
  CHECK(lease.taken());
  std::uint8_t region[512];
  CHECK(arena.add_region(std::uint64_t{1} << 26u, region, sizeof region));
  stats.observe(arena);
  CHECK(arena.peak_ring() >= 1024u);
  CHECK_EQ(value_of(stats, ChannelCounter::arena_peak_ring_bytes), arena.peak_ring());
  CHECK_EQ(value_of(stats, ChannelCounter::arena_set_aside_bytes), arena.set_aside_bytes());
  CHECK(stats.invariants_hold()); }

TEST(channel_stats_observe, an_unchanged_gauge_is_not_a_reason_to_report)
{ std::uint8_t    block[4096];
  ClientBlobArena arena(block, sizeof block);
  ChannelStats    stats;
  stats.observe(arena);
  stats.sent(1u);
  stats.observe(arena);
  stats.observe(arena);
  CHECK(!stats.due(1u + interval * 5u)); }

TEST(channel_stats_observe, a_changed_gauge_makes_the_report_due)
{ std::uint8_t    block[4096];
  ClientBlobArena arena(block, sizeof block);
  ChannelStats    stats;
  stats.observe(arena);
  stats.sent(1u);
  const av::ClientBlobLease lease = arena.take(256u);
  CHECK(lease.taken());
  stats.observe(arena);
  CHECK(stats.due(1u + interval)); }

TEST(channel_stats_observe, an_arena_with_no_block_reads_as_zero)
{ ClientBlobArena arena;
  ChannelStats    stats;
  stats.observe(arena);
  CHECK_EQ(value_of(stats, ChannelCounter::arena_bytes), 0u);
  CHECK(!stats.due(interval * 10u)); }

TEST(channel_stats_due, a_first_change_is_due_at_once_at_any_time)
{ ChannelStats stats;
  stats.note(ChannelCounter::slice_waits, 1u);
  CHECK(stats.due(0u));
  CHECK(stats.due(1u)); }

TEST(channel_stats_due, after_a_send_it_waits_a_full_interval_for_the_next_change)
{ ChannelStats stats;
  stats.note(ChannelCounter::slice_waits, 1u);
  stats.sent(1000u);
  CHECK(!stats.due(1000u + interval * 10u));
  stats.note(ChannelCounter::slice_waits, 1u);
  CHECK(!stats.due(1000u));
  CHECK(!stats.due(1000u + interval - 1u));
  CHECK(stats.due(1000u + interval));
  CHECK(stats.due(1000u + interval * 3u));
  CHECK(stats.invariants_hold()); }

TEST(channel_stats_due, a_send_clears_only_what_it_reported)
{ ChannelStats stats;
  stats.note(ChannelCounter::room_waits, 1u);
  stats.sent(5u);
  stats.note(ChannelCounter::room_waits, 1u);
  stats.sent(6u);
  CHECK(!stats.due(6u + interval * 4u));
  CHECK(stats.invariants_hold()); }

TEST(channel_stats_frame_report, writes_a_header_and_a_report_that_decode_to_the_snapshot)
{ ChannelStats stats;
  stats.note(ChannelCounter::room_waits, 11u);
  stats.note(ChannelCounter::refused_no_space, 22u);
  std::uint8_t frame[ChannelStats::command_bytes] = {};
  CHECK(stats.frame_report(frame, sizeof frame));
  CommandHeader header;
  CHECK_STATUS(header.decode(frame, CommandHeader::encoded_size), Status::ok);
  CHECK_EQ(header.size(), ChannelStats::command_bytes);
  CHECK_EQ(header.id(), CommandHeader::channel_stats_id);
  ChannelStatsReport report;
  CHECK_STATUS(report.decode(frame + CommandHeader::encoded_size, ChannelStatsReport::encoded_size), Status::ok);
  CHECK_EQ(report.value(ChannelCounter::room_waits), 11u);
  CHECK_EQ(report.value(ChannelCounter::refused_no_space), 22u);
  CHECK_EQ(report.value(ChannelCounter::inline_bulk), 0u); }

TEST(channel_stats_frame_report, the_frame_is_exactly_a_header_and_a_report)
{ CHECK_EQ(ChannelStats::command_bytes, CommandHeader::encoded_size + ChannelStatsReport::encoded_size);
  CHECK_EQ(ChannelStats::command_bytes % 8u, 0u); }

TEST(channel_stats_frame_report, a_short_buffer_is_refused_and_not_written)
{ const ChannelStats stats;
  av::GuardedBuffer  buffer(ChannelStats::command_bytes);
  for (std::size_t capacity = 0u; capacity < ChannelStats::command_bytes; capacity++) {
      std::memset(buffer.data(), 0x77, ChannelStats::command_bytes);
      CHECK(!stats.frame_report(buffer.data(), capacity));
      for (std::size_t at = 0u; at < ChannelStats::command_bytes; at++) CHECK_EQ(buffer.data()[at], 0x77u);
  }
  CHECK(buffer.guards_intact()); }

TEST(channel_stats_frame_report, a_longer_buffer_is_written_only_to_the_frame)
{ const ChannelStats stats;
  std::uint8_t       bytes[ChannelStats::command_bytes + 16u];
  std::memset(bytes, 0x77, sizeof bytes);
  CHECK(stats.frame_report(bytes, sizeof bytes));
  for (std::size_t at = ChannelStats::command_bytes; at < sizeof bytes; at++) CHECK_EQ(bytes[at], 0x77u); }

TEST(channel_stats_frame_report, reporting_changes_nothing_until_it_is_marked_sent)
{ ChannelStats stats;
  stats.note(ChannelCounter::room_waits, 1u);
  std::uint8_t frame[ChannelStats::command_bytes] = {};
  CHECK(stats.frame_report(frame, sizeof frame));
  CHECK(stats.due(0u));
  stats.sent(0u);
  stats.note(ChannelCounter::room_waits, 1u);
  CHECK(stats.due(interval)); }
