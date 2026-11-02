#include "core/support/status.hpp"
#include "harness/framework/byte_layout.hpp"
#include "harness/framework/guarded_buffer.hpp"
#include "harness/framework/hostile_sweep.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/context/context_setup.hpp"
#include "proto/renderer/bind_context_message.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

using av::BindContextMessage;
using av::ByteLayout;
using av::ContextSetup;
using av::GuardedBuffer;
using av::Status;

namespace {

const std::uint8_t    setup[] = {0xA0u, 0xA1u, 0xA2u, 0xA3u};
constexpr std::size_t bind_length = BindContextMessage::fixed_size + sizeof setup;

void put(std::uint8_t *bytes, std::size_t offset, std::uint32_t value)
{
    for (std::size_t i = 0u; i < 4u; i++) bytes[offset + i] = static_cast<std::uint8_t>(value >> (8u * i));
}

bool rejects(const std::uint8_t *bytes, std::size_t length)
{
    BindContextMessage message;
    const Status       status = message.decode(bytes, length, 1u);
    if (status == Status::ok) {
        std::uint8_t again[bind_length] = {};
        if (message.encoded_length() != length || message.encode(again, sizeof again) != Status::ok ||
            std::memcmp(again, bytes, length) != 0)
            av::TestRegistry::instance().record_failure(__FILE__, __LINE__, "accepted input did not round trip");
    }
    return status != Status::ok;
}

}

TEST(bind_context_message_encode, is_the_context_flags_doorbell_setup_length_then_the_setup)
{ GuardedBuffer bytes(bind_length);
  const BindContextMessage bind(0x11223344u, setup, sizeof setup, 0x55u);
  CHECK_EQ(bind.encoded_length(), bind_length);
  CHECK_STATUS(bind.encode(bytes.data(), bytes.size()), Status::ok);
  const ByteLayout layout(bytes.data(), bytes.size());
  CHECK(layout.at(0u, 4u) == 0x11223344u);
  CHECK(layout.at(4u, 4u) == BindContextMessage::has_doorbell_flag);
  CHECK(layout.at(8u, 4u) == 0x55u);
  CHECK(layout.at(12u, 4u) == sizeof setup);
  CHECK(layout.at(16u, 4u) == 0xA3A2A1A0u);
  CHECK(bytes.guards_intact()); }

TEST(bind_context_message, a_doorbell_round_trips_with_its_eventfd)
{ std::uint8_t bytes[bind_length] = {};
  CHECK_STATUS(BindContextMessage(7u, setup, sizeof setup, 0u).encode(bytes, sizeof bytes), Status::ok);
  BindContextMessage decoded;
  CHECK(!decoded.has_doorbell());
  CHECK(decoded.setup() == nullptr);
  CHECK_STATUS(decoded.decode(bytes, sizeof bytes, 1u), Status::ok);
  CHECK_EQ(decoded.context(), 7u);
  CHECK(decoded.has_doorbell());
  CHECK_EQ(decoded.doorbell(), 0u);
  CHECK_EQ(decoded.descriptors(), 1u);
  CHECK_EQ(decoded.setup_length(), static_cast<std::uint32_t>(sizeof setup));
  CHECK(decoded.setup() == bytes + BindContextMessage::fixed_size); }

TEST(bind_context_message, no_doorbell_is_the_flag_clear_and_no_eventfd)
{ std::uint8_t bytes[bind_length] = {};
  const BindContextMessage bind(7u, setup, sizeof setup);
  CHECK_EQ(bind.descriptors(), 0u);
  CHECK_STATUS(bind.encode(bytes, sizeof bytes), Status::ok);
  CHECK(ByteLayout(bytes, sizeof bytes).at(4u, 4u) == 0u);
  BindContextMessage decoded;
  CHECK_STATUS(decoded.decode(bytes, sizeof bytes, 1u), Status::malformed);
  CHECK_STATUS(decoded.decode(bytes, sizeof bytes, 0u), Status::ok);
  CHECK(!decoded.has_doorbell());
  CHECK_EQ(decoded.doorbell(), 0u); }

TEST(bind_context_message, the_largest_setup_round_trips)
{ std::uint8_t large[ContextSetup::encoded_size] = {};
  std::uint8_t bytes[BindContextMessage::fixed_size + sizeof large] = {};
  CHECK_STATUS(BindContextMessage(7u, large, sizeof large, 3u).encode(bytes, sizeof bytes), Status::ok);
  BindContextMessage decoded;
  CHECK_STATUS(decoded.decode(bytes, sizeof bytes, 1u), Status::ok);
  CHECK_EQ(decoded.setup_length(), static_cast<std::uint32_t>(ContextSetup::encoded_size)); }

TEST(bind_context_message_encode, refuses_what_decode_would)
{ std::uint8_t too_long[ContextSetup::encoded_size + 1u] = {};
  std::uint8_t bytes[BindContextMessage::fixed_size + sizeof too_long] = {};
  CHECK_STATUS(BindContextMessage(0u, setup, sizeof setup, 1u).encode(bytes, sizeof bytes), Status::invalid_parameter);
  CHECK_STATUS(BindContextMessage(7u, setup, 0u, 1u).encode(bytes, sizeof bytes), Status::invalid_parameter);
  CHECK_STATUS(BindContextMessage(7u, nullptr, 4u).encode(bytes, sizeof bytes), Status::invalid_parameter);
  CHECK_STATUS(BindContextMessage(7u, too_long, sizeof too_long).encode(bytes, sizeof bytes),
               Status::invalid_parameter); }

TEST(bind_context_message, a_null_setup_is_empty)
{ CHECK_EQ(BindContextMessage(7u, nullptr, 4u).setup_length(), 0u);
  CHECK_EQ(BindContextMessage(7u, nullptr, 4u, 1u).setup_length(), 0u); }

TEST(bind_context_message_encode, a_short_or_missing_destination_is_too_large)
{ GuardedBuffer buffer(bind_length - 1u);
  CHECK_STATUS(BindContextMessage(7u, setup, sizeof setup).encode(buffer.data(), buffer.size()), Status::too_large);
  CHECK(buffer.guards_intact());
  CHECK_STATUS(BindContextMessage(7u, setup, sizeof setup).encode(nullptr, bind_length), Status::too_large); }

TEST(bind_context_message_decode, refuses_bad_fields_and_stays_unchanged)
{ struct Case { std::size_t offset; std::uint32_t value; std::uint32_t descriptors; };
  const Case cases[] = {
      {0u, 0u, 1u},
      {4u, 3u, 1u},
      {4u, 0u, 0u},
      {8u, 5u, 2u},
      {8u, 5u, 0u},
  };
  for (const Case &c : cases) {
      std::uint8_t bytes[bind_length] = {};
      CHECK_STATUS(BindContextMessage(7u, setup, sizeof setup, 5u).encode(bytes, sizeof bytes), Status::ok);
      put(bytes, c.offset, c.value);
      BindContextMessage decoded(9u, setup, 2u);
      CHECK_STATUS(decoded.decode(bytes, sizeof bytes, c.descriptors), Status::malformed);
      CHECK_EQ(decoded.context(), 9u);
      CHECK_EQ(decoded.setup_length(), 2u);
  } }

TEST(bind_context_message_decode, refuses_an_empty_or_overlong_setup_that_arrived)
{ std::uint8_t bytes[BindContextMessage::fixed_size + ContextSetup::encoded_size + 1u] = {};
  put(bytes, 0u, 7u);
  BindContextMessage decoded;
  CHECK_STATUS(decoded.decode(bytes, BindContextMessage::fixed_size, 0u), Status::malformed);
  put(bytes, 12u, ContextSetup::encoded_size + 1u);
  CHECK_STATUS(decoded.decode(bytes, sizeof bytes, 0u), Status::malformed);
  CHECK_EQ(decoded.context(), 0u); }

TEST(bind_context_message_decode, a_length_other_than_the_fields_give_is_malformed)
{ std::uint8_t bytes[bind_length + 1u] = {};
  CHECK_STATUS(BindContextMessage(7u, setup, sizeof setup).encode(bytes, sizeof bytes), Status::ok);
  BindContextMessage decoded;
  CHECK_STATUS(decoded.decode(bytes, bind_length - 1u, 0u), Status::malformed);
  CHECK_STATUS(decoded.decode(bytes, bind_length + 1u, 0u), Status::malformed);
  CHECK_STATUS(decoded.decode(bytes, BindContextMessage::fixed_size - 1u, 0u), Status::malformed);
  CHECK_STATUS(decoded.decode(nullptr, bind_length, 0u), Status::malformed);
  CHECK_EQ(decoded.context(), 0u); }

TEST(bind_context_message_decode, survives_the_hostile_sweep)
{ std::uint8_t bytes[bind_length] = {};
  CHECK_STATUS(BindContextMessage(3u, setup, sizeof setup, 5u).encode(bytes, sizeof bytes), Status::ok);
  const av::HostileSweep sweep(rejects, bytes, sizeof bytes);
  sweep.run();
  CHECK(sweep.cases_run() > 128u);
  const std::uint32_t acceptable = 32u + 32u + 32u + 1u;
  CHECK(sweep.rejections() >= sweep.cases_run() - acceptable); }
