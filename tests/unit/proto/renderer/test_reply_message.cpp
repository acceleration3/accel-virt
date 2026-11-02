#include "core/support/status.hpp"
#include "harness/framework/byte_layout.hpp"
#include "harness/framework/guarded_buffer.hpp"
#include "harness/framework/hostile_sweep.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/renderer/reply_message.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

using av::ByteLayout;
using av::GuardedBuffer;
using av::ReplyMessage;
using av::Status;

namespace {

const std::uint8_t    payload[] = {0xB0u, 0xB1u, 0xB2u, 0xB3u};
constexpr std::size_t reply_length = ReplyMessage::fixed_size + sizeof payload;

void put(std::uint8_t *bytes, std::size_t offset, std::uint32_t value)
{
    for (std::size_t i = 0u; i < 4u; i++) bytes[offset + i] = static_cast<std::uint8_t>(value >> (8u * i));
}

bool rejects(const std::uint8_t *bytes, std::size_t length)
{
    ReplyMessage message;
    const Status status = message.decode(bytes, length, ReplyMessage::descriptors);
    if (status == Status::ok) {
        std::uint8_t again[reply_length] = {};
        if (message.encoded_length() != length || message.encode(again, sizeof again) != Status::ok ||
            std::memcmp(again, bytes, length) != 0)
            av::TestRegistry::instance().record_failure(__FILE__, __LINE__, "accepted input did not round trip");
    }
    return status != Status::ok;
}

}

TEST(reply_message_encode, is_the_sequence_the_status_the_payload_length_then_the_payload)
{ GuardedBuffer bytes(reply_length);
  const ReplyMessage reply(0x1122334455667788ull, Status::not_supported, payload, sizeof payload);
  CHECK_EQ(reply.encoded_length(), reply_length);
  CHECK_STATUS(reply.encode(bytes.data(), bytes.size()), Status::ok);
  const ByteLayout layout(bytes.data(), bytes.size());
  CHECK(layout.at(0u, 8u) == 0x1122334455667788ull);
  CHECK(layout.at(8u, 4u) == static_cast<std::uint32_t>(Status::not_supported));
  CHECK(layout.at(12u, 4u) == sizeof payload);
  CHECK(layout.at(16u, 4u) == 0xB3B2B1B0u);
  CHECK_EQ(ReplyMessage::descriptors, 0u);
  CHECK(bytes.guards_intact()); }

TEST(reply_message, round_trips_and_views_the_payload_it_decoded)
{ std::uint8_t bytes[reply_length] = {};
  CHECK_STATUS(ReplyMessage(~std::uint64_t{0}, Status::malformed, payload, sizeof payload).encode(bytes, sizeof bytes),
               Status::ok);
  ReplyMessage decoded;
  CHECK(decoded.payload() == nullptr);
  CHECK_STATUS(decoded.decode(bytes, sizeof bytes, 0u), Status::ok);
  CHECK(decoded.sequence() == ~std::uint64_t{0});
  CHECK_STATUS(decoded.status(), Status::malformed);
  CHECK_EQ(decoded.payload_length(), static_cast<std::uint32_t>(sizeof payload));
  CHECK(decoded.payload() == bytes + ReplyMessage::fixed_size); }

TEST(reply_message, an_empty_reply_is_the_fixed_part_alone)
{ std::uint8_t bytes[ReplyMessage::fixed_size] = {};
  CHECK_STATUS(ReplyMessage(4u, Status::ok, nullptr, 9u).encode(bytes, sizeof bytes), Status::ok);
  ReplyMessage decoded;
  CHECK_STATUS(decoded.decode(bytes, sizeof bytes, 0u), Status::ok);
  CHECK(decoded.sequence() == 4u);
  CHECK_STATUS(decoded.status(), Status::ok);
  CHECK_EQ(decoded.payload_length(), 0u); }

TEST(reply_message, the_largest_payload_round_trips)
{ static std::uint8_t large[ReplyMessage::max_payload_length];
  static std::uint8_t bytes[ReplyMessage::fixed_size + sizeof large];
  CHECK_STATUS(ReplyMessage(1u, Status::ok, large, sizeof large).encode(bytes, sizeof bytes), Status::ok);
  ReplyMessage decoded;
  CHECK_STATUS(decoded.decode(bytes, sizeof bytes, 0u), Status::ok);
  CHECK_EQ(decoded.payload_length(), ReplyMessage::max_payload_length); }

TEST(reply_message_encode, refuses_what_decode_would)
{ static std::uint8_t large[ReplyMessage::max_payload_length + 1u];
  static std::uint8_t bytes[ReplyMessage::fixed_size + sizeof large];
  CHECK_STATUS(ReplyMessage(1u, Status::ok, large, sizeof large).encode(bytes, sizeof bytes), Status::invalid_parameter);
  CHECK_STATUS(ReplyMessage(1u, static_cast<Status>(10u), nullptr, 0u).encode(bytes, sizeof bytes),
               Status::invalid_parameter); }

TEST(reply_message_encode, a_short_or_missing_destination_is_too_large)
{ GuardedBuffer buffer(reply_length - 1u);
  CHECK_STATUS(ReplyMessage(1u, Status::ok, payload, sizeof payload).encode(buffer.data(), buffer.size()),
               Status::too_large);
  CHECK(buffer.guards_intact());
  CHECK_STATUS(ReplyMessage(1u, Status::ok, payload, sizeof payload).encode(nullptr, reply_length), Status::too_large); }

TEST(reply_message_decode, refuses_an_unknown_status_and_stays_unchanged)
{ std::uint8_t bytes[reply_length] = {};
  CHECK_STATUS(ReplyMessage(1u, Status::ok, payload, sizeof payload).encode(bytes, sizeof bytes), Status::ok);
  put(bytes, 8u, static_cast<std::uint32_t>(Status::malformed) + 1u);
  ReplyMessage decoded(6u, Status::conflict, nullptr, 0u);
  CHECK_STATUS(decoded.decode(bytes, sizeof bytes, 0u), Status::malformed);
  CHECK(decoded.sequence() == 6u);
  CHECK_STATUS(decoded.status(), Status::conflict); }

TEST(reply_message_decode, a_payload_past_the_bound_is_malformed_even_when_it_arrived)
{ static std::uint8_t bytes[ReplyMessage::fixed_size + ReplyMessage::max_payload_length + 1u];
  put(bytes, 12u, ReplyMessage::max_payload_length + 1u);
  ReplyMessage decoded;
  CHECK_STATUS(decoded.decode(bytes, sizeof bytes, 0u), Status::malformed);
  CHECK_EQ(decoded.payload_length(), 0u); }

TEST(reply_message_decode, any_descriptor_or_a_length_other_than_the_fields_give_is_malformed)
{ std::uint8_t bytes[reply_length + 1u] = {};
  CHECK_STATUS(ReplyMessage(1u, Status::ok, payload, sizeof payload).encode(bytes, sizeof bytes), Status::ok);
  ReplyMessage decoded;
  CHECK_STATUS(decoded.decode(bytes, reply_length, 1u), Status::malformed);
  CHECK_STATUS(decoded.decode(bytes, reply_length - 1u, 0u), Status::malformed);
  CHECK_STATUS(decoded.decode(bytes, reply_length + 1u, 0u), Status::malformed);
  CHECK_STATUS(decoded.decode(bytes, ReplyMessage::fixed_size - 1u, 0u), Status::malformed);
  CHECK_STATUS(decoded.decode(nullptr, reply_length, 0u), Status::malformed);
  CHECK(decoded.sequence() == 0u); }

TEST(reply_message_decode, survives_the_hostile_sweep)
{ std::uint8_t bytes[reply_length] = {};
  CHECK_STATUS(ReplyMessage(9u, Status::ok, payload, sizeof payload).encode(bytes, sizeof bytes), Status::ok);
  const av::HostileSweep sweep(rejects, bytes, sizeof bytes);
  sweep.run();
  CHECK(sweep.cases_run() > 128u);
  const std::uint32_t acceptable = 64u + 4u + 32u + 1u;
  CHECK(sweep.rejections() >= sweep.cases_run() - acceptable); }
