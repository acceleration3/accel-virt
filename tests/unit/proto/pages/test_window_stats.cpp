#include "core/support/status.hpp"
#include "harness/framework/byte_layout.hpp"
#include "harness/framework/hostile_sweep.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/pages/window_stats.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

using av::ByteLayout;
using av::Status;
using av::WindowStats;

namespace {

WindowStats sample() { return WindowStats(1000u, 400u, 900u, 3u, 0x200000u, 7u); }

bool rejects(const std::uint8_t *bytes, std::size_t length)
{
    WindowStats stats;
    const Status status = stats.decode(bytes, length);
    if (status != Status::ok) return true;
    std::uint8_t again[WindowStats::encoded_size] = {};
    if (length < WindowStats::encoded_size || stats.encode(again, sizeof again) != Status::ok)
        av::TestRegistry::instance().record_failure(__FILE__, __LINE__, "accepted input did not re-encode");
    return false;
}

}

TEST(window_stats, the_page_layout_keeps_the_sequence_between_the_header_and_the_fields)
{ CHECK_EQ(WindowStats::sequence_at, 8u);
  CHECK_EQ(WindowStats::fields_at, 16u);
  CHECK_EQ(WindowStats::encoded_size, WindowStats::fields_at + WindowStats::field_count * 8u);
  CHECK(WindowStats::encoded_size <= WindowStats::page_size); }

TEST(window_stats, a_default_report_is_all_zero)
{ const WindowStats stats;
  for (std::uint32_t index = 0u; index < WindowStats::field_count; index++) CHECK_EQ(stats.field(index), 0u); }

TEST(window_stats, the_named_accessors_and_the_indexed_one_agree)
{ const WindowStats stats = sample();
  CHECK_EQ(stats.total_pages(), 1000u);
  CHECK_EQ(stats.pages_in_use(), 400u);
  CHECK_EQ(stats.peak_pages_in_use(), 900u);
  CHECK_EQ(stats.refusals(), 3u);
  CHECK_EQ(stats.refused_bytes(), 0x200000u);
  CHECK_EQ(stats.largest_free_run_at_refusal(), 7u);
  CHECK(stats.field(0u) == stats.total_pages());
  CHECK(stats.field(1u) == stats.pages_in_use());
  CHECK(stats.field(2u) == stats.peak_pages_in_use());
  CHECK(stats.field(3u) == stats.refusals());
  CHECK(stats.field(4u) == stats.refused_bytes());
  CHECK(stats.field(5u) == stats.largest_free_run_at_refusal()); }

TEST(window_stats, a_field_past_the_last_reads_as_zero)
{ const WindowStats stats = sample();
  CHECK_EQ(stats.field(WindowStats::field_count), 0u);
  CHECK_EQ(stats.field(0xFFFFFFFFu), 0u); }

TEST(window_stats_encode, places_the_magic_the_version_and_every_field)
{ std::uint8_t bytes[WindowStats::encoded_size];
  std::memset(bytes, 0xEE, sizeof bytes);
  CHECK_STATUS(sample().encode(bytes, sizeof bytes), Status::ok);
  const ByteLayout layout(bytes, sizeof bytes);
  CHECK_EQ(layout.at(0u, 4u), 0x00535057u);
  CHECK_EQ(layout.at(4u, 4u), 1u);
  CHECK_EQ(layout.at(8u, 4u), 0u);
  CHECK_EQ(layout.at(12u, 4u), 0u);
  CHECK_EQ(layout.at(16u, 8u), 1000u);
  CHECK_EQ(layout.at(24u, 8u), 400u);
  CHECK_EQ(layout.at(32u, 8u), 900u);
  CHECK_EQ(layout.at(40u, 8u), 3u);
  CHECK_EQ(layout.at(48u, 8u), 0x200000u);
  CHECK_EQ(layout.at(56u, 8u), 7u); }

TEST(window_stats_encode, a_short_buffer_is_refused_and_not_written)
{ std::uint8_t bytes[WindowStats::encoded_size] = {};
  CHECK_STATUS(sample().encode(bytes, WindowStats::encoded_size - 1u), Status::too_large);
  for (const std::uint8_t byte : bytes) CHECK_EQ(byte, 0u); }

TEST(window_stats, round_trips_through_its_encoding)
{ std::uint8_t bytes[WindowStats::encoded_size] = {};
  CHECK_STATUS(sample().encode(bytes, sizeof bytes), Status::ok);
  WindowStats decoded;
  CHECK_STATUS(decoded.decode(bytes, sizeof bytes), Status::ok);
  for (std::uint32_t index = 0u; index < WindowStats::field_count; index++)
      CHECK(decoded.field(index) == sample().field(index)); }

TEST(window_stats_decode, ignores_the_sequence_word_a_writer_is_flipping)
{ std::uint8_t bytes[WindowStats::encoded_size] = {};
  CHECK_STATUS(sample().encode(bytes, sizeof bytes), Status::ok);
  std::memset(bytes + WindowStats::sequence_at, 0xFF, WindowStats::fields_at - WindowStats::sequence_at);
  WindowStats decoded;
  CHECK_STATUS(decoded.decode(bytes, sizeof bytes), Status::ok);
  CHECK_EQ(decoded.total_pages(), 1000u); }

TEST(window_stats_decode, a_wrong_magic_is_malformed_in_every_byte)
{ for (std::size_t byte = 0u; byte < 4u; byte++) {
      std::uint8_t bytes[WindowStats::encoded_size] = {};
      CHECK_STATUS(sample().encode(bytes, sizeof bytes), Status::ok);
      bytes[byte] ^= 0x01u;
      WindowStats decoded;
      CHECK_STATUS(decoded.decode(bytes, sizeof bytes), Status::malformed);
  } }

TEST(window_stats_decode, a_version_other_than_one_is_malformed)
{ const std::uint32_t versions[] = {0u, 2u, 0x100u, 0x80000000u, 0xFFFFFFFFu};
  for (const std::uint32_t version : versions) {
      std::uint8_t bytes[WindowStats::encoded_size] = {};
      CHECK_STATUS(sample().encode(bytes, sizeof bytes), Status::ok);
      std::memcpy(bytes + 4, &version, sizeof version);
      WindowStats decoded;
      CHECK_STATUS(decoded.decode(bytes, sizeof bytes), Status::malformed);
  } }

TEST(window_stats_decode, a_truncated_page_is_refused_at_every_length)
{ std::uint8_t bytes[WindowStats::encoded_size] = {};
  CHECK_STATUS(sample().encode(bytes, sizeof bytes), Status::ok);
  for (std::size_t length = 0u; length < WindowStats::encoded_size; length++) {
      WindowStats decoded;
      CHECK(decoded.decode(bytes, length) != Status::ok);
  } }

TEST(window_stats_decode, a_longer_page_decodes_from_its_front)
{ std::uint8_t bytes[WindowStats::page_size] = {};
  std::memset(bytes, 0xA5, sizeof bytes);
  CHECK_STATUS(sample().encode(bytes, WindowStats::encoded_size), Status::ok);
  WindowStats decoded;
  CHECK_STATUS(decoded.decode(bytes, sizeof bytes), Status::ok);
  CHECK_EQ(decoded.refusals(), 3u); }

TEST(window_stats_decode, a_rejected_page_leaves_the_destination_unchanged)
{ WindowStats decoded = sample();
  std::uint8_t bytes[WindowStats::encoded_size] = {};
  CHECK_STATUS(WindowStats().encode(bytes, sizeof bytes), Status::ok);
  bytes[0] ^= 0xFFu;
  CHECK_STATUS(decoded.decode(bytes, sizeof bytes), Status::malformed);
  CHECK_EQ(decoded.total_pages(), 1000u);
  CHECK(decoded.decode(bytes, 10u) != Status::ok);
  CHECK_EQ(decoded.largest_free_run_at_refusal(), 7u); }

TEST(window_stats_decode, survives_the_hostile_sweep)
{ std::uint8_t bytes[WindowStats::encoded_size] = {};
  CHECK_STATUS(sample().encode(bytes, sizeof bytes), Status::ok);
  const av::HostileSweep sweep(rejects, bytes, sizeof bytes);
  sweep.run();
  CHECK(sweep.rejections() > 0u); }
