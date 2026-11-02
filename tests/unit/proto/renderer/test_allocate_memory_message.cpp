#include "core/support/status.hpp"
#include "harness/framework/byte_layout.hpp"
#include "harness/framework/guarded_buffer.hpp"
#include "harness/framework/hostile_sweep.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/renderer/allocate_memory_message.hpp"
#include "proto/resources/host_memory_request.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

using av::AllocateMemoryMessage;
using av::ByteLayout;
using av::GuardedBuffer;
using av::HostMemoryRequest;
using av::Status;

namespace {

constexpr std::uint32_t context = 7u;
constexpr std::uint64_t key = 0x01000003u;
constexpr std::uint64_t size = 0x200000u;
const HostMemoryRequest readback(HostMemoryRequest::Kind::readback, 0u);

bool rejects(const std::uint8_t *bytes, std::size_t length)
{
    AllocateMemoryMessage message;
    const Status status = message.decode(bytes, length, 0u);
    if (status == Status::ok) {
        std::uint8_t again[AllocateMemoryMessage::encoded_size] = {};
        if (message.encode(again, sizeof again) != Status::ok || std::memcmp(again, bytes, sizeof again) != 0)
            av::TestRegistry::instance().record_failure(__FILE__, __LINE__, "accepted input did not round trip");
    }
    return status != Status::ok;
}

}

TEST(allocate_memory_message_encode, is_the_context_the_request_the_key_and_the_size)
{ GuardedBuffer bytes(AllocateMemoryMessage::encoded_size);
  CHECK_STATUS(AllocateMemoryMessage(context, readback.encoded(), key, size).encode(bytes.data(), bytes.size()),
               Status::ok);
  const ByteLayout layout(bytes.data(), bytes.size());
  CHECK(layout.at(0u, 4u) == context);
  CHECK(layout.at(4u, 4u) == readback.encoded());
  CHECK(layout.at(8u, 8u) == key);
  CHECK(layout.at(16u, 8u) == size);
  CHECK_EQ(AllocateMemoryMessage::encoded_size, 24u);
  CHECK(bytes.guards_intact()); }

TEST(allocate_memory_message, round_trips_through_its_encoding)
{ std::uint8_t bytes[AllocateMemoryMessage::encoded_size] = {};
  const std::uint32_t vulkan = HostMemoryRequest(HostMemoryRequest::Kind::vulkan, 31u).encoded();
  CHECK_STATUS(AllocateMemoryMessage(0xFFFFFFFFu, vulkan, ~std::uint64_t{0}, ~std::uint64_t{0})
                   .encode(bytes, sizeof bytes),
               Status::ok);
  AllocateMemoryMessage decoded;
  CHECK_EQ(decoded.context(), 0u);
  CHECK_EQ(decoded.key(), 0u);
  CHECK_STATUS(decoded.decode(bytes, sizeof bytes, 0u), Status::ok);
  CHECK_EQ(decoded.context(), 0xFFFFFFFFu);
  CHECK_EQ(decoded.request(), vulkan);
  CHECK_EQ(decoded.key(), ~std::uint64_t{0});
  CHECK_EQ(decoded.size(), ~std::uint64_t{0}); }

TEST(allocate_memory_message, carries_any_request_for_the_library_to_judge)
{ std::uint8_t bytes[AllocateMemoryMessage::encoded_size] = {};
  CHECK_STATUS(AllocateMemoryMessage(context, 0u, key, size).encode(bytes, sizeof bytes), Status::ok);
  AllocateMemoryMessage decoded;
  CHECK_STATUS(decoded.decode(bytes, sizeof bytes, 0u), Status::ok);
  CHECK_EQ(decoded.request(), 0u); }

TEST(allocate_memory_message_encode, refuses_what_decode_would)
{ std::uint8_t bytes[AllocateMemoryMessage::encoded_size] = {};
  CHECK_STATUS(AllocateMemoryMessage(0u, readback.encoded(), key, size).encode(bytes, sizeof bytes),
               Status::invalid_parameter);
  CHECK_STATUS(AllocateMemoryMessage(context, readback.encoded(), 0u, size).encode(bytes, sizeof bytes),
               Status::invalid_parameter);
  CHECK_STATUS(AllocateMemoryMessage(context, readback.encoded(), key, 0u).encode(bytes, sizeof bytes),
               Status::invalid_parameter);
  CHECK(ByteLayout(bytes, sizeof bytes).at(0u, 8u) == 0u); }

TEST(allocate_memory_message_encode, a_short_or_missing_destination_is_too_large)
{ GuardedBuffer buffer(AllocateMemoryMessage::encoded_size - 1u);
  CHECK_STATUS(AllocateMemoryMessage(context, readback.encoded(), key, size).encode(buffer.data(), buffer.size()),
               Status::too_large);
  CHECK(buffer.guards_intact());
  CHECK_STATUS(AllocateMemoryMessage(context, readback.encoded(), key, size)
                   .encode(nullptr, AllocateMemoryMessage::encoded_size),
               Status::too_large); }

TEST(allocate_memory_message_decode, a_zero_field_is_malformed_and_leaves_it_unchanged)
{ const std::size_t zeroed[][2] = {{0u, 4u}, {8u, 8u}, {16u, 8u}};
  for (const auto &field : zeroed) {
      std::uint8_t bytes[AllocateMemoryMessage::encoded_size] = {};
      CHECK_STATUS(AllocateMemoryMessage(context, readback.encoded(), key, size).encode(bytes, sizeof bytes),
                   Status::ok);
      std::memset(bytes + field[0], 0, field[1]);
      AllocateMemoryMessage decoded(9u, 1u, 9u, 9u);
      CHECK_STATUS(decoded.decode(bytes, sizeof bytes, 0u), Status::malformed);
      CHECK_EQ(decoded.context(), 9u);
      CHECK_EQ(decoded.key(), 9u);
      CHECK_EQ(decoded.size(), 9u);
  } }

TEST(allocate_memory_message_decode, any_descriptor_other_length_or_no_bytes_is_malformed)
{ std::uint8_t bytes[AllocateMemoryMessage::encoded_size + 1u] = {};
  CHECK_STATUS(AllocateMemoryMessage(context, readback.encoded(), key, size).encode(bytes, sizeof bytes), Status::ok);
  AllocateMemoryMessage decoded;
  CHECK_STATUS(decoded.decode(bytes, AllocateMemoryMessage::encoded_size, 1u), Status::malformed);
  CHECK_STATUS(decoded.decode(bytes, AllocateMemoryMessage::encoded_size - 1u, 0u), Status::malformed);
  CHECK_STATUS(decoded.decode(bytes, AllocateMemoryMessage::encoded_size + 1u, 0u), Status::malformed);
  CHECK_STATUS(decoded.decode(nullptr, AllocateMemoryMessage::encoded_size, 0u), Status::malformed);
  CHECK_EQ(decoded.context(), 0u); }

TEST(allocate_memory_message_decode, survives_the_hostile_sweep)
{ std::uint8_t bytes[AllocateMemoryMessage::encoded_size] = {};
  CHECK_STATUS(AllocateMemoryMessage(context, readback.encoded(), key, size).encode(bytes, sizeof bytes), Status::ok);
  const av::HostileSweep sweep(rejects, bytes, sizeof bytes);
  sweep.run();
  CHECK(sweep.cases_run() > 192u);
  const std::uint32_t acceptable = 191u + 1u + 1u;
  CHECK(sweep.rejections() >= sweep.cases_run() - acceptable); }
