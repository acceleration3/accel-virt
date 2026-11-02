#include "core/support/status.hpp"
#include "harness/framework/byte_layout.hpp"
#include "harness/framework/guarded_buffer.hpp"
#include "harness/framework/hostile_sweep.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/renderer/interrupt_lines_message.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

using av::ByteLayout;
using av::GuardedBuffer;
using av::InterruptLinesMessage;
using av::Status;

namespace {

bool rejects(const std::uint8_t *bytes, std::size_t length)
{
    InterruptLinesMessage message;
    const Status          status = message.decode(bytes, length, 2u);
    if (status == Status::ok) {
        std::uint8_t again[InterruptLinesMessage::encoded_size] = {};
        if (message.encode(again, sizeof again) != Status::ok || std::memcmp(again, bytes, sizeof again) != 0)
            av::TestRegistry::instance().record_failure(__FILE__, __LINE__, "accepted input did not round trip");
    }
    return status != Status::ok;
}

}

TEST(interrupt_lines_message_encode, is_the_line_count_a_reserved_word_and_the_present_mask)
{ GuardedBuffer bytes(InterruptLinesMessage::encoded_size);
  CHECK_STATUS(InterruptLinesMessage(64u, 0x8000000000000001ull).encode(bytes.data(), bytes.size()), Status::ok);
  const ByteLayout layout(bytes.data(), bytes.size());
  CHECK(layout.at(0u, 4u) == 64u);
  CHECK(layout.at(4u, 4u) == 0u);
  CHECK(layout.at(8u, 4u) == 1u);
  CHECK(layout.at(12u, 4u) == 0x80000000u);
  CHECK_EQ(InterruptLinesMessage::encoded_size, 16u);
  CHECK(bytes.guards_intact()); }

TEST(interrupt_lines_message, round_trips_with_one_descriptor_per_present_line)
{ std::uint8_t bytes[InterruptLinesMessage::encoded_size] = {};
  CHECK_STATUS(InterruptLinesMessage(4u, 0x5u).encode(bytes, sizeof bytes), Status::ok);
  InterruptLinesMessage decoded;
  CHECK_EQ(decoded.lines(), 0u);
  CHECK_EQ(decoded.descriptors(), 0u);
  CHECK_STATUS(decoded.decode(bytes, sizeof bytes, 2u), Status::ok);
  CHECK_EQ(decoded.lines(), 4u);
  CHECK_EQ(decoded.descriptors(), 2u);
  CHECK(decoded.present(0u));
  CHECK(!decoded.present(1u));
  CHECK(decoded.present(2u));
  CHECK(!decoded.present(3u));
  CHECK(!decoded.present(4u)); }

TEST(interrupt_lines_message, no_line_present_is_a_message_with_no_descriptors)
{ std::uint8_t bytes[InterruptLinesMessage::encoded_size] = {};
  CHECK_STATUS(InterruptLinesMessage(2u, 0u).encode(bytes, sizeof bytes), Status::ok);
  InterruptLinesMessage decoded;
  CHECK_STATUS(decoded.decode(bytes, sizeof bytes, 0u), Status::ok);
  CHECK_EQ(decoded.descriptors(), 0u); }

TEST(interrupt_lines_message, every_line_present_needs_max_lines_descriptors)
{ std::uint8_t bytes[InterruptLinesMessage::encoded_size] = {};
  CHECK_STATUS(InterruptLinesMessage(InterruptLinesMessage::max_lines, ~0ull).encode(bytes, sizeof bytes), Status::ok);
  InterruptLinesMessage decoded;
  CHECK_STATUS(decoded.decode(bytes, sizeof bytes, InterruptLinesMessage::max_lines), Status::ok);
  CHECK(decoded.present(InterruptLinesMessage::max_lines - 1u)); }

TEST(interrupt_lines_message_encode, refuses_what_decode_would)
{ std::uint8_t bytes[InterruptLinesMessage::encoded_size] = {};
  CHECK_STATUS(InterruptLinesMessage(0u, 0u).encode(bytes, sizeof bytes), Status::invalid_parameter);
  CHECK_STATUS(InterruptLinesMessage(InterruptLinesMessage::max_lines + 1u, 1u).encode(bytes, sizeof bytes),
               Status::invalid_parameter);
  CHECK_STATUS(InterruptLinesMessage(2u, 0x4u).encode(bytes, sizeof bytes), Status::invalid_parameter); }

TEST(interrupt_lines_message_encode, a_short_or_missing_destination_is_too_large)
{ GuardedBuffer buffer(InterruptLinesMessage::encoded_size - 1u);
  CHECK_STATUS(InterruptLinesMessage(1u, 1u).encode(buffer.data(), buffer.size()), Status::too_large);
  CHECK(buffer.guards_intact());
  CHECK_STATUS(InterruptLinesMessage(1u, 1u).encode(nullptr, InterruptLinesMessage::encoded_size), Status::too_large); }

TEST(interrupt_lines_message_decode, refuses_bad_fields_and_wrong_descriptor_counts_and_stays_unchanged)
{ struct Case { std::uint32_t lines; std::uint64_t present; std::uint32_t descriptors; };
  const Case cases[] = {
      {0u, 0u, 0u},
      {InterruptLinesMessage::max_lines + 1u, 1u, 1u},
      {2u, 0x4u, 1u},
      {4u, 0x5u, 1u},
      {4u, 0x5u, 3u},
  };
  for (const Case &c : cases) {
      std::uint8_t bytes[InterruptLinesMessage::encoded_size] = {};
      for (std::size_t i = 0u; i < 4u; i++) bytes[i] = static_cast<std::uint8_t>(c.lines >> (8u * i));
      for (std::size_t i = 0u; i < 8u; i++) bytes[8u + i] = static_cast<std::uint8_t>(c.present >> (8u * i));
      InterruptLinesMessage decoded(3u, 1u);
      CHECK_STATUS(decoded.decode(bytes, sizeof bytes, c.descriptors), Status::malformed);
      CHECK_EQ(decoded.lines(), 3u);
      CHECK_EQ(decoded.descriptors(), 1u);
  } }

TEST(interrupt_lines_message_decode, a_reserved_word_that_is_not_zero_is_malformed)
{ std::uint8_t bytes[InterruptLinesMessage::encoded_size] = {};
  CHECK_STATUS(InterruptLinesMessage(4u, 0x5u).encode(bytes, sizeof bytes), Status::ok);
  bytes[4] = 1u;
  InterruptLinesMessage decoded;
  CHECK_STATUS(decoded.decode(bytes, sizeof bytes, 2u), Status::malformed);
  CHECK_EQ(decoded.lines(), 0u); }

TEST(interrupt_lines_message_decode, any_other_length_or_no_bytes_is_malformed)
{ std::uint8_t bytes[InterruptLinesMessage::encoded_size + 1u] = {};
  CHECK_STATUS(InterruptLinesMessage(1u, 1u).encode(bytes, sizeof bytes), Status::ok);
  InterruptLinesMessage decoded;
  CHECK_STATUS(decoded.decode(bytes, InterruptLinesMessage::encoded_size - 1u, 1u), Status::malformed);
  CHECK_STATUS(decoded.decode(bytes, InterruptLinesMessage::encoded_size + 1u, 1u), Status::malformed);
  CHECK_STATUS(decoded.decode(nullptr, InterruptLinesMessage::encoded_size, 1u), Status::malformed);
  CHECK_EQ(decoded.lines(), 0u); }

TEST(interrupt_lines_message_decode, survives_the_hostile_sweep)
{ std::uint8_t bytes[InterruptLinesMessage::encoded_size] = {};
  CHECK_STATUS(InterruptLinesMessage(4u, 0x5u).encode(bytes, sizeof bytes), Status::ok);
  const av::HostileSweep sweep(rejects, bytes, sizeof bytes);
  sweep.run();
  CHECK(sweep.cases_run() > 64u);
  const std::uint32_t acceptable = 5u + 1u;
  CHECK(sweep.rejections() >= sweep.cases_run() - acceptable); }
