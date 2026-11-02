#include "core/support/status.hpp"
#include "harness/framework/byte_layout.hpp"
#include "harness/framework/guarded_buffer.hpp"
#include "harness/framework/hostile_sweep.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/renderer/free_memory_message.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

using av::ByteLayout;
using av::FreeMemoryMessage;
using av::GuardedBuffer;
using av::Status;

namespace {

constexpr std::uint64_t key = 0x0100000300000005ull;

bool rejects(const std::uint8_t *bytes, std::size_t length)
{
    FreeMemoryMessage message;
    const Status status = message.decode(bytes, length, 0u);
    if (status == Status::ok) {
        std::uint8_t again[FreeMemoryMessage::encoded_size] = {};
        if (message.encode(again, sizeof again) != Status::ok || std::memcmp(again, bytes, sizeof again) != 0)
            av::TestRegistry::instance().record_failure(__FILE__, __LINE__, "accepted input did not round trip");
    }
    return status != Status::ok;
}

}

TEST(free_memory_message, is_the_key_and_round_trips)
{ GuardedBuffer bytes(FreeMemoryMessage::encoded_size);
  CHECK_STATUS(FreeMemoryMessage(key).encode(bytes.data(), bytes.size()), Status::ok);
  CHECK(ByteLayout(bytes.data(), bytes.size()).at(0u, 8u) == key);
  CHECK_EQ(FreeMemoryMessage::encoded_size, 8u);
  CHECK(bytes.guards_intact());
  FreeMemoryMessage decoded;
  CHECK_EQ(decoded.key(), 0u);
  CHECK_STATUS(decoded.decode(bytes.data(), bytes.size(), 0u), Status::ok);
  CHECK_EQ(decoded.key(), key); }

TEST(free_memory_message, key_zero_is_refused_both_ways)
{ std::uint8_t bytes[FreeMemoryMessage::encoded_size] = {};
  CHECK_STATUS(FreeMemoryMessage(0u).encode(bytes, sizeof bytes), Status::invalid_parameter);
  FreeMemoryMessage decoded(key);
  CHECK_STATUS(decoded.decode(bytes, sizeof bytes, 0u), Status::malformed);
  CHECK_EQ(decoded.key(), key); }

TEST(free_memory_message_encode, a_short_or_missing_destination_is_too_large)
{ GuardedBuffer buffer(FreeMemoryMessage::encoded_size - 1u);
  CHECK_STATUS(FreeMemoryMessage(key).encode(buffer.data(), buffer.size()), Status::too_large);
  CHECK(buffer.guards_intact());
  CHECK_STATUS(FreeMemoryMessage(key).encode(nullptr, FreeMemoryMessage::encoded_size), Status::too_large); }

TEST(free_memory_message_decode, any_descriptor_other_length_or_no_bytes_is_malformed)
{ std::uint8_t bytes[FreeMemoryMessage::encoded_size + 1u] = {};
  CHECK_STATUS(FreeMemoryMessage(key).encode(bytes, sizeof bytes), Status::ok);
  FreeMemoryMessage decoded(7u);
  CHECK_STATUS(decoded.decode(bytes, FreeMemoryMessage::encoded_size, 1u), Status::malformed);
  CHECK_STATUS(decoded.decode(bytes, FreeMemoryMessage::encoded_size - 1u, 0u), Status::malformed);
  CHECK_STATUS(decoded.decode(bytes, FreeMemoryMessage::encoded_size + 1u, 0u), Status::malformed);
  CHECK_STATUS(decoded.decode(nullptr, FreeMemoryMessage::encoded_size, 0u), Status::malformed);
  CHECK_EQ(decoded.key(), 7u); }

TEST(free_memory_message_decode, survives_the_hostile_sweep)
{ std::uint8_t bytes[FreeMemoryMessage::encoded_size] = {};
  CHECK_STATUS(FreeMemoryMessage(key).encode(bytes, sizeof bytes), Status::ok);
  const av::HostileSweep sweep(rejects, bytes, sizeof bytes);
  sweep.run();
  CHECK(sweep.cases_run() > 64u);
  const std::uint32_t acceptable = 64u + 1u + 1u;
  CHECK(sweep.rejections() >= sweep.cases_run() - acceptable); }
