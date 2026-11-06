#include "core/support/status.hpp"
#include "harness/framework/byte_layout.hpp"
#include "harness/framework/guarded_buffer.hpp"
#include "harness/framework/hostile_sweep.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/renderer/context_message.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

using av::ContextMessage;
using av::ByteLayout;
using av::GuardedBuffer;
using av::Status;

namespace {

bool rejects(const std::uint8_t *bytes, std::size_t length)
{
    ContextMessage message;
    const Status status = message.decode(bytes, length, ContextMessage::descriptors);
    if (status == Status::ok) {
        std::uint8_t again[ContextMessage::encoded_size] = {};
        if (message.encode(again, sizeof again) != Status::ok || std::memcmp(again, bytes, sizeof again) != 0)
            av::TestRegistry::instance().record_failure(__FILE__, __LINE__, "accepted input did not round trip");
    }
    return status != Status::ok;
}

}

TEST(context_message_encode, is_the_context_and_its_name)
{ GuardedBuffer bytes(ContextMessage::encoded_size);
  CHECK_STATUS(ContextMessage(0x11223344u, "dwm").encode(bytes.data(), bytes.size()), Status::ok);
  const ByteLayout layout(bytes.data(), bytes.size());
  CHECK(layout.at(0u, 4u) == 0x11223344u);
  CHECK_EQ(std::memcmp(bytes.data() + 4u, "dwm", 4u), 0);
  CHECK_EQ(ContextMessage::encoded_size, 4u + ContextMessage::name_bytes);
  CHECK_EQ(ContextMessage::descriptors, 0u);
  CHECK(bytes.guards_intact()); }

TEST(context_message, round_trips_through_its_encoding)
{ std::uint8_t bytes[ContextMessage::encoded_size] = {};
  CHECK_STATUS(ContextMessage(0xFFFFFFFFu, "explorer.exe").encode(bytes, sizeof bytes), Status::ok);
  ContextMessage decoded;
  CHECK_EQ(decoded.context(), 0u);
  CHECK_STATUS(decoded.decode(bytes, sizeof bytes, 0u), Status::ok);
  CHECK_EQ(decoded.context(), 0xFFFFFFFFu);
  CHECK_EQ(std::strcmp(decoded.name(), "explorer.exe"), 0); }

TEST(context_message, a_name_that_fills_its_field_is_cut_and_one_with_no_end_is_refused)
{ char long_name[ContextMessage::name_bytes + 8u];
  std::memset(long_name, 'n', sizeof long_name);
  long_name[sizeof long_name - 1u] = '\0';
  const ContextMessage cut(5u, long_name);
  CHECK_EQ(std::strlen(cut.name()), ContextMessage::name_bytes - 1u);
  std::uint8_t bytes[ContextMessage::encoded_size] = {};
  CHECK_STATUS(cut.encode(bytes, sizeof bytes), Status::ok);
  ContextMessage decoded(7u);
  bytes[ContextMessage::encoded_size - 1u] = 'x';
  CHECK_STATUS(decoded.decode(bytes, sizeof bytes, 0u), Status::malformed);
  CHECK_EQ(decoded.context(), 7u); }

TEST(context_message, context_zero_is_refused_both_ways_and_leaves_the_message_unchanged)
{ std::uint8_t bytes[ContextMessage::encoded_size] = {};
  CHECK_STATUS(ContextMessage(0u).encode(bytes, sizeof bytes), Status::invalid_parameter);
  ContextMessage decoded(7u);
  CHECK_STATUS(decoded.decode(bytes, sizeof bytes, 0u), Status::malformed);
  CHECK_EQ(decoded.context(), 7u); }

TEST(context_message_encode, a_short_or_missing_destination_is_too_large)
{ GuardedBuffer buffer(ContextMessage::encoded_size - 1u);
  CHECK_STATUS(ContextMessage(1u).encode(buffer.data(), buffer.size()), Status::too_large);
  CHECK(buffer.guards_intact());
  CHECK_STATUS(ContextMessage(1u).encode(nullptr, ContextMessage::encoded_size), Status::too_large); }

TEST(context_message_decode, any_descriptor_other_length_or_no_bytes_is_malformed)
{ std::uint8_t bytes[ContextMessage::encoded_size + 1u] = {};
  CHECK_STATUS(ContextMessage(9u).encode(bytes, sizeof bytes), Status::ok);
  ContextMessage decoded;
  CHECK_STATUS(decoded.decode(bytes, ContextMessage::encoded_size, 1u), Status::malformed);
  CHECK_STATUS(decoded.decode(bytes, ContextMessage::encoded_size - 1u, 0u), Status::malformed);
  CHECK_STATUS(decoded.decode(bytes, ContextMessage::encoded_size + 1u, 0u), Status::malformed);
  CHECK_STATUS(decoded.decode(nullptr, ContextMessage::encoded_size, 0u), Status::malformed);
  CHECK_EQ(decoded.context(), 0u); }

TEST(context_message_decode, survives_the_hostile_sweep)
{ std::uint8_t bytes[ContextMessage::encoded_size] = {};
  CHECK_STATUS(ContextMessage(3u).encode(bytes, sizeof bytes), Status::ok);
  const av::HostileSweep sweep(rejects, bytes, sizeof bytes);
  sweep.run();
  CHECK(sweep.cases_run() > 32u);
  const std::uint32_t acceptable = 32u + (ContextMessage::name_bytes - 1u) * 8u + 2u;
  CHECK(sweep.rejections() >= sweep.cases_run() - acceptable); }

TEST(context_message, a_missing_name_is_an_empty_one)
{ const ContextMessage unnamed(5u, nullptr);
  CHECK_EQ(unnamed.context(), 5u);
  CHECK_EQ(std::strlen(unnamed.name()), 0u);
  std::uint8_t bytes[ContextMessage::encoded_size] = {};
  CHECK_STATUS(unnamed.encode(bytes, sizeof bytes), Status::ok); }
