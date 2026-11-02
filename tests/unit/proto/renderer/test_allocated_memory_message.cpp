#include "core/support/status.hpp"
#include "device/abi/av_executor.h"
#include "harness/framework/byte_layout.hpp"
#include "harness/framework/guarded_buffer.hpp"
#include "harness/framework/hostile_sweep.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/renderer/allocated_memory_message.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

using av::AllocatedMemoryMessage;
using av::ByteLayout;
using av::GuardedBuffer;
using av::Status;

namespace {

constexpr std::uint64_t size = 0x10000u;

bool rejects(const std::uint8_t *bytes, std::size_t length)
{
    AllocatedMemoryMessage message;
    const Status status = message.decode(bytes, length, 1u);
    if (status == Status::ok) {
        std::uint8_t again[AllocatedMemoryMessage::encoded_size] = {};
        if (message.encode(again, sizeof again) != Status::ok || std::memcmp(again, bytes, sizeof again) != 0)
            av::TestRegistry::instance().record_failure(__FILE__, __LINE__, "accepted input did not round trip");
    }
    return status != Status::ok;
}

}

TEST(allocated_memory_message_encode, is_the_caching_a_reserved_word_and_the_size)
{ GuardedBuffer bytes(AllocatedMemoryMessage::encoded_size);
  CHECK_STATUS(AllocatedMemoryMessage(AllocatedMemoryMessage::write_combined, size).encode(bytes.data(), bytes.size()),
               Status::ok);
  const ByteLayout layout(bytes.data(), bytes.size());
  CHECK(layout.at(0u, 4u) == AllocatedMemoryMessage::write_combined);
  CHECK(layout.at(4u, 4u) == 0u);
  CHECK(layout.at(8u, 8u) == size);
  CHECK_EQ(AllocatedMemoryMessage::encoded_size, 16u);
  CHECK(bytes.guards_intact()); }

TEST(allocated_memory_message, its_cachings_are_the_executor_abis)
{ CHECK_EQ(AllocatedMemoryMessage::write_back, static_cast<std::uint32_t>(AV_MEMORY_WRITE_BACK));
  CHECK_EQ(AllocatedMemoryMessage::write_combined, static_cast<std::uint32_t>(AV_MEMORY_WRITE_COMBINED)); }

TEST(allocated_memory_message, an_allocation_round_trips_with_its_one_descriptor)
{ const std::uint32_t cachings[] = {AllocatedMemoryMessage::write_back, AllocatedMemoryMessage::write_combined};
  for (const std::uint32_t caching : cachings) {
      std::uint8_t bytes[AllocatedMemoryMessage::encoded_size] = {};
      const AllocatedMemoryMessage made(caching, ~std::uint64_t{0});
      CHECK(made.allocated());
      CHECK_EQ(made.descriptors(), 1u);
      CHECK_STATUS(made.encode(bytes, sizeof bytes), Status::ok);
      AllocatedMemoryMessage decoded;
      CHECK(!decoded.allocated());
      CHECK_STATUS(decoded.decode(bytes, sizeof bytes, 1u), Status::ok);
      CHECK(decoded.allocated());
      CHECK_EQ(decoded.caching(), caching);
      CHECK_EQ(decoded.size(), ~std::uint64_t{0});
      CHECK_STATUS(decoded.decode(bytes, sizeof bytes, 0u), Status::malformed);
      CHECK_STATUS(decoded.decode(bytes, sizeof bytes, 2u), Status::malformed);
  } }

TEST(allocated_memory_message, nothing_allocated_carries_nothing)
{ const AllocatedMemoryMessage none;
  CHECK(!none.allocated());
  CHECK_EQ(none.descriptors(), 0u);
  CHECK_EQ(none.size(), 0u);
  std::uint8_t bytes[AllocatedMemoryMessage::encoded_size] = {};
  std::memset(bytes, 0xEE, sizeof bytes);
  CHECK_STATUS(none.encode(bytes, sizeof bytes), Status::ok);
  for (const std::uint8_t byte : bytes) CHECK_EQ(byte, 0u);
  AllocatedMemoryMessage decoded(AllocatedMemoryMessage::write_back, size);
  CHECK_STATUS(decoded.decode(bytes, sizeof bytes, 1u), Status::malformed);
  CHECK(decoded.allocated());
  CHECK_STATUS(decoded.decode(bytes, sizeof bytes, 0u), Status::ok);
  CHECK(!decoded.allocated()); }

TEST(allocated_memory_message_encode, refuses_what_decode_would)
{ std::uint8_t bytes[AllocatedMemoryMessage::encoded_size] = {};
  CHECK_STATUS(AllocatedMemoryMessage(3u, size).encode(bytes, sizeof bytes), Status::invalid_parameter);
  CHECK_STATUS(AllocatedMemoryMessage(AllocatedMemoryMessage::write_back, 0u).encode(bytes, sizeof bytes),
               Status::invalid_parameter);
  CHECK_STATUS(AllocatedMemoryMessage(0u, size).encode(bytes, sizeof bytes), Status::invalid_parameter); }

TEST(allocated_memory_message_encode, a_short_or_missing_destination_is_too_large)
{ GuardedBuffer buffer(AllocatedMemoryMessage::encoded_size - 1u);
  CHECK_STATUS(AllocatedMemoryMessage(AllocatedMemoryMessage::write_back, size).encode(buffer.data(), buffer.size()),
               Status::too_large);
  CHECK(buffer.guards_intact());
  CHECK_STATUS(AllocatedMemoryMessage(AllocatedMemoryMessage::write_back, size)
                   .encode(nullptr, AllocatedMemoryMessage::encoded_size),
               Status::too_large); }

TEST(allocated_memory_message_decode, refuses_every_field_out_of_bounds_and_stays_unchanged)
{ struct Case { std::uint32_t caching; std::uint32_t reserved; std::uint64_t bytes; };
  const Case cases[] = {
      {3u, 0u, size}, {0xFFFFFFFFu, 0u, size}, {1u, 1u, size}, {1u, 0x80000000u, size},
      {1u, 0u, 0u},   {0u, 0u, size},          {0u, 1u, 0u},
  };
  for (const Case &one : cases) {
      std::uint8_t bytes[AllocatedMemoryMessage::encoded_size] = {};
      std::memcpy(bytes, &one.caching, 4u);
      std::memcpy(bytes + 4u, &one.reserved, 4u);
      std::memcpy(bytes + 8u, &one.bytes, 8u);
      AllocatedMemoryMessage decoded(AllocatedMemoryMessage::write_combined, 0x3000u);
      CHECK_STATUS(decoded.decode(bytes, sizeof bytes, one.caching != 0u ? 1u : 0u), Status::malformed);
      CHECK_EQ(decoded.caching(), AllocatedMemoryMessage::write_combined);
      CHECK_EQ(decoded.size(), 0x3000u);
  } }

TEST(allocated_memory_message_decode, another_length_or_no_bytes_is_malformed)
{ std::uint8_t bytes[AllocatedMemoryMessage::encoded_size + 1u] = {};
  CHECK_STATUS(AllocatedMemoryMessage(AllocatedMemoryMessage::write_back, size).encode(bytes, sizeof bytes),
               Status::ok);
  AllocatedMemoryMessage decoded;
  CHECK_STATUS(decoded.decode(bytes, AllocatedMemoryMessage::encoded_size - 1u, 1u), Status::malformed);
  CHECK_STATUS(decoded.decode(bytes, AllocatedMemoryMessage::encoded_size + 1u, 1u), Status::malformed);
  CHECK_STATUS(decoded.decode(nullptr, AllocatedMemoryMessage::encoded_size, 1u), Status::malformed);
  CHECK(!decoded.allocated()); }

TEST(allocated_memory_message_decode, survives_the_hostile_sweep)
{ std::uint8_t bytes[AllocatedMemoryMessage::encoded_size] = {};
  CHECK_STATUS(AllocatedMemoryMessage(AllocatedMemoryMessage::write_back, size).encode(bytes, sizeof bytes),
               Status::ok);
  const av::HostileSweep sweep(rejects, bytes, sizeof bytes);
  sweep.run();
  CHECK(sweep.cases_run() > 128u);
  const std::uint32_t acceptable = 63u + 1u;
  CHECK(sweep.rejections() >= sweep.cases_run() - acceptable); }
