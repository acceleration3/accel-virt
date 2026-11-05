#include "core/support/status.hpp"
#include "harness/framework/byte_layout.hpp"
#include "harness/framework/hostile_sweep.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/submission/channel_counter.hpp"
#include "proto/submission/channel_stats_report.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

using av::ByteLayout;
using av::ChannelCounter;
using av::ChannelStatsReport;
using av::Status;

namespace {

constexpr std::size_t value_offset(std::uint32_t index) { return 8u + std::size_t{index} * 8u; }

ChannelStatsReport filled()
{
    ChannelStatsReport report;
    for (std::uint32_t index = 0u; index < ChannelStatsReport::counters; index++)
        report.set(static_cast<ChannelCounter>(index), 0x0101010101010101ull * (index + 1u));
    return report;
}

bool rejects(const std::uint8_t *bytes, std::size_t length)
{
    ChannelStatsReport report;
    const Status       status = report.decode(bytes, length);
    if (status == Status::ok) {
        std::uint8_t again[ChannelStatsReport::encoded_size] = {};
        if (length != ChannelStatsReport::encoded_size || report.encode(again, sizeof again) != Status::ok ||
            std::memcmp(again, bytes, length) != 0)
            av::TestRegistry::instance().record_failure(__FILE__, __LINE__, "accepted input did not round trip");
    }
    return status != Status::ok;
}

}

TEST(channel_stats_report, the_encoded_size_is_a_header_and_one_word_per_counter)
{ CHECK_EQ(ChannelStatsReport::encoded_size, 8u + ChannelStatsReport::counters * 8u);
  CHECK_EQ(ChannelStatsReport::counters, static_cast<std::uint32_t>(ChannelCounter::count)); }

TEST(channel_stats_report_encode, places_the_version_the_count_and_every_counter)
{ const ChannelStatsReport report = filled();
  std::uint8_t             bytes[ChannelStatsReport::encoded_size] = {};
  CHECK_STATUS(report.encode(bytes, sizeof bytes), Status::ok);
  const ByteLayout layout(bytes, sizeof bytes);
  CHECK_EQ(layout.at(0u, 4u), 2u);
  CHECK_EQ(layout.at(4u, 4u), ChannelStatsReport::counters);
  for (std::uint32_t index = 0u; index < ChannelStatsReport::counters; index++)
      CHECK(layout.at(value_offset(index), 8u) == 0x0101010101010101ull * (index + 1u)); }

TEST(channel_stats_report_encode, a_short_buffer_is_refused_and_not_written)
{ const ChannelStatsReport report = filled();
  std::uint8_t             bytes[ChannelStatsReport::encoded_size] = {};
  CHECK_STATUS(report.encode(bytes, ChannelStatsReport::encoded_size - 1u), Status::too_large);
  CHECK_STATUS(report.encode(bytes, 0u), Status::too_large);
  for (const std::uint8_t byte : bytes) CHECK_EQ(byte, 0u); }

TEST(channel_stats_report_encode, a_longer_buffer_is_written_only_to_its_size)
{ const ChannelStatsReport report = filled();
  std::uint8_t             bytes[ChannelStatsReport::encoded_size + 8u];
  std::memset(bytes, 0xCD, sizeof bytes);
  CHECK_STATUS(report.encode(bytes, sizeof bytes), Status::ok);
  for (std::size_t index = ChannelStatsReport::encoded_size; index < sizeof bytes; index++)
      CHECK_EQ(bytes[index], 0xCDu); }

TEST(channel_stats_report, round_trips_every_counter)
{ const ChannelStatsReport report = filled();
  std::uint8_t             bytes[ChannelStatsReport::encoded_size] = {};
  CHECK_STATUS(report.encode(bytes, sizeof bytes), Status::ok);
  ChannelStatsReport decoded;
  CHECK_STATUS(decoded.decode(bytes, sizeof bytes), Status::ok);
  for (std::uint32_t index = 0u; index < ChannelStatsReport::counters; index++)
      CHECK(decoded.value(static_cast<ChannelCounter>(index)) == report.value(static_cast<ChannelCounter>(index))); }

TEST(channel_stats_report, a_counter_holds_the_full_sixty_four_bits)
{ ChannelStatsReport report;
  report.set(ChannelCounter::arena_bytes, ~0ull);
  std::uint8_t bytes[ChannelStatsReport::encoded_size] = {};
  CHECK_STATUS(report.encode(bytes, sizeof bytes), Status::ok);
  ChannelStatsReport decoded;
  CHECK_STATUS(decoded.decode(bytes, sizeof bytes), Status::ok);
  CHECK(decoded.value(ChannelCounter::arena_bytes) == ~0ull);
  CHECK(decoded.value(ChannelCounter::arena_peak_ring_bytes) == 0u); }

TEST(channel_stats_report, a_counter_past_the_last_reads_as_zero_and_sets_nothing)
{ ChannelStatsReport report;
  report.set(ChannelCounter::count, 7u);
  report.set(static_cast<ChannelCounter>(0xFFFFFFFFu), 7u);
  CHECK(report.value(ChannelCounter::count) == 0u);
  CHECK(report.value(static_cast<ChannelCounter>(0xFFFFFFFFu)) == 0u);
  for (std::uint32_t index = 0u; index < ChannelStatsReport::counters; index++)
      CHECK(report.value(static_cast<ChannelCounter>(index)) == 0u); }

TEST(channel_stats_report_decode, a_missing_buffer_is_malformed)
{ ChannelStatsReport report;
  CHECK_STATUS(report.decode(nullptr, ChannelStatsReport::encoded_size), Status::malformed); }

TEST(channel_stats_report_decode, only_the_exact_length_is_accepted)
{ const ChannelStatsReport report = filled();
  std::uint8_t             bytes[ChannelStatsReport::encoded_size + 8u] = {};
  CHECK_STATUS(report.encode(bytes, sizeof bytes), Status::ok);
  const std::size_t lengths[] = {0u, 1u, 4u, 7u, 8u, ChannelStatsReport::encoded_size - 1u,
                                 ChannelStatsReport::encoded_size + 1u, ChannelStatsReport::encoded_size + 8u};
  for (const std::size_t length : lengths) {
      ChannelStatsReport decoded;
      CHECK_STATUS(decoded.decode(bytes, length), Status::malformed);
  } }

TEST(channel_stats_report_decode, a_version_other_than_two_is_malformed)
{ const std::uint8_t versions[] = {0u, 1u, 3u, 0x80u, 0xFFu};
  for (const std::uint8_t version : versions) {
      std::uint8_t bytes[ChannelStatsReport::encoded_size] = {};
      CHECK_STATUS(filled().encode(bytes, sizeof bytes), Status::ok);
      bytes[0] = version;
      ChannelStatsReport decoded;
      CHECK_STATUS(decoded.decode(bytes, sizeof bytes), Status::malformed);
  }
  std::uint8_t bytes[ChannelStatsReport::encoded_size] = {};
  CHECK_STATUS(filled().encode(bytes, sizeof bytes), Status::ok);
  bytes[3] = 1u;
  ChannelStatsReport decoded;
  CHECK_STATUS(decoded.decode(bytes, sizeof bytes), Status::malformed); }

TEST(channel_stats_report_decode, a_count_other_than_the_counters_is_malformed)
{ const std::uint32_t counts[] = {0u, 1u, ChannelStatsReport::counters - 1u, ChannelStatsReport::counters + 1u,
                                  0xFFFFFFFFu};
  for (const std::uint32_t count : counts) {
      std::uint8_t bytes[ChannelStatsReport::encoded_size] = {};
      CHECK_STATUS(filled().encode(bytes, sizeof bytes), Status::ok);
      std::memcpy(bytes + 4, &count, sizeof count);
      ChannelStatsReport decoded;
      CHECK_STATUS(decoded.decode(bytes, sizeof bytes), Status::malformed);
  } }

TEST(channel_stats_report_decode, a_rejected_report_leaves_the_destination_unchanged)
{ ChannelStatsReport decoded = filled();
  std::uint8_t       bytes[ChannelStatsReport::encoded_size] = {};
  CHECK_STATUS(ChannelStatsReport().encode(bytes, sizeof bytes), Status::ok);
  bytes[0] = 9u;
  CHECK_STATUS(decoded.decode(bytes, sizeof bytes), Status::malformed);
  for (std::uint32_t index = 0u; index < ChannelStatsReport::counters; index++)
      CHECK(decoded.value(static_cast<ChannelCounter>(index)) == 0x0101010101010101ull * (index + 1u)); }

TEST(channel_stats_report_decode, survives_the_hostile_sweep)
{ std::uint8_t bytes[ChannelStatsReport::encoded_size] = {};
  CHECK_STATUS(filled().encode(bytes, sizeof bytes), Status::ok);
  const av::HostileSweep sweep(rejects, bytes, sizeof bytes);
  sweep.run();
  CHECK(sweep.cases_run() > 0u);
  CHECK(sweep.rejections() > 0u); }
