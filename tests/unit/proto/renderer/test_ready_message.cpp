#include "core/support/status.hpp"
#include "harness/framework/byte_layout.hpp"
#include "harness/framework/guarded_buffer.hpp"
#include "harness/framework/hostile_sweep.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/context/sub_api.hpp"
#include "proto/renderer/ready_message.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

using av::ByteLayout;
using av::GuardedBuffer;
using av::ReadyMessage;
using av::Status;
using av::SubApi;

namespace {

/* Vulkan (bit 0) and OpenCL (bit 2). */
constexpr std::uint32_t offered = static_cast<std::uint32_t>(SubApi::vulkan) | static_cast<std::uint32_t>(SubApi::opencl);

bool rejects(const std::uint8_t *bytes, std::size_t length)
{
    ReadyMessage message;
    const Status status = message.decode(bytes, length, ReadyMessage::descriptors);
    if (status == Status::ok) {
        std::uint8_t again[ReadyMessage::encoded_size] = {};
        if (message.encode(again, sizeof again) != Status::ok || std::memcmp(again, bytes, sizeof again) != 0)
            av::TestRegistry::instance().record_failure(__FILE__, __LINE__, "accepted input did not round trip");
    }
    return status != Status::ok;
}

}  // namespace

TEST(ready_message_encode, is_the_versions_and_the_sub_apis)
{ GuardedBuffer bytes(ReadyMessage::encoded_size);
  CHECK_STATUS(ReadyMessage(0x00010002u, 5u, 0xFFu).encode(bytes.data(), bytes.size()), Status::ok);
  const ByteLayout layout(bytes.data(), bytes.size());
  CHECK(layout.at(0u, 4u) == 0x00010002u);
  CHECK(layout.at(4u, 4u) == 5u);
  CHECK(layout.at(8u, 4u) == 0xFFu);
  CHECK_EQ(ReadyMessage::encoded_size, 12u);
  CHECK_EQ(ReadyMessage::descriptors, 0u);
  CHECK(bytes.guards_intact()); }

TEST(ready_message, round_trips_through_its_encoding)
{ std::uint8_t bytes[ReadyMessage::encoded_size] = {};
  CHECK_STATUS(ReadyMessage(2u, 9u, offered).encode(bytes, sizeof bytes), Status::ok);
  ReadyMessage decoded;
  CHECK_EQ(decoded.version(), 0u);
  CHECK_EQ(decoded.sub_apis(), 0u);
  CHECK_STATUS(decoded.decode(bytes, sizeof bytes, 0u), Status::ok);
  CHECK_EQ(decoded.version(), 2u);
  CHECK_EQ(decoded.executor_abi(), 9u);
  CHECK_EQ(decoded.sub_apis(), offered); }

/* A renderer that can run nothing still answers: the capset then offers
 * nothing, which is the guest's to see. */
TEST(ready_message, a_renderer_offering_nothing_is_a_message)
{ std::uint8_t bytes[ReadyMessage::encoded_size] = {};
  CHECK_STATUS(ReadyMessage(1u, 1u, 0u).encode(bytes, sizeof bytes), Status::ok);
  ReadyMessage decoded(7u, 7u, offered);
  CHECK_STATUS(decoded.decode(bytes, sizeof bytes, 0u), Status::ok);
  CHECK_EQ(decoded.sub_apis(), 0u); }

/* The last lane is bit 7: every lane up to it is a sub-API, the next bit is
 * none. */
TEST(ready_message_encode, refuses_a_sub_api_past_the_last_lane)
{ std::uint8_t bytes[ReadyMessage::encoded_size] = {};
  CHECK_STATUS(ReadyMessage(1u, 1u, 1u << (ReadyMessage::lanes - 1u)).encode(bytes, sizeof bytes), Status::ok);
  CHECK_STATUS(ReadyMessage(1u, 1u, offered | (1u << ReadyMessage::lanes)).encode(bytes, sizeof bytes),
               Status::invalid_parameter);
  CHECK_STATUS(ReadyMessage(1u, 1u, 0x80000000u).encode(bytes, sizeof bytes), Status::invalid_parameter); }

TEST(ready_message_encode, a_short_or_missing_destination_is_too_large)
{ GuardedBuffer buffer(ReadyMessage::encoded_size - 1u);
  CHECK_STATUS(ReadyMessage(1u, 1u, offered).encode(buffer.data(), buffer.size()), Status::too_large);
  CHECK(buffer.guards_intact());
  CHECK_STATUS(ReadyMessage(1u, 1u, offered).encode(nullptr, ReadyMessage::encoded_size), Status::too_large); }

TEST(ready_message_decode, refuses_a_sub_api_past_the_last_lane_and_stays_unchanged)
{ const std::uint32_t masks[] = {offered | 0x100u, 0x80000000u, 0xFFFFFFFFu};
  for (const std::uint32_t mask : masks) {
      std::uint8_t bytes[ReadyMessage::encoded_size] = {};
      CHECK_STATUS(ReadyMessage(1u, 1u, offered).encode(bytes, sizeof bytes), Status::ok);
      for (std::size_t i = 0u; i < 4u; i++) bytes[8u + i] = static_cast<std::uint8_t>(mask >> (8u * i));
      ReadyMessage decoded(7u, 7u, offered);
      CHECK_STATUS(decoded.decode(bytes, sizeof bytes, 0u), Status::malformed);
      CHECK_EQ(decoded.version(), 7u);
      CHECK_EQ(decoded.sub_apis(), offered);
  } }

TEST(ready_message_decode, any_descriptor_other_length_or_no_bytes_is_malformed)
{ std::uint8_t bytes[ReadyMessage::encoded_size + 1u] = {};
  CHECK_STATUS(ReadyMessage(1u, 1u, offered).encode(bytes, sizeof bytes), Status::ok);
  ReadyMessage decoded;
  CHECK_STATUS(decoded.decode(bytes, ReadyMessage::encoded_size, 1u), Status::malformed);
  CHECK_STATUS(decoded.decode(bytes, ReadyMessage::encoded_size - 1u, 0u), Status::malformed);
  CHECK_STATUS(decoded.decode(bytes, ReadyMessage::encoded_size + 1u, 0u), Status::malformed);
  CHECK_STATUS(decoded.decode(nullptr, ReadyMessage::encoded_size, 0u), Status::malformed);
  CHECK_EQ(decoded.version(), 0u); }

TEST(ready_message_decode, survives_the_hostile_sweep)
{ std::uint8_t bytes[ReadyMessage::encoded_size] = {};
  CHECK_STATUS(ReadyMessage(2u, 9u, offered).encode(bytes, sizeof bytes), Status::ok);
  const av::HostileSweep sweep(rejects, bytes, sizeof bytes);
  sweep.run();
  CHECK(sweep.cases_run() > 96u);
  /* Both versions are free: 64 flips. Any of the eight lane bits of the
   * sub-API mask may be flipped; the 24 above them name no lane. All zero is
   * a renderer that offers nothing, which is a message. With the untouched
   * message, 64 + 8 + 1 + 1. */
  const std::uint32_t acceptable = 64u + 8u + 1u + 1u;
  CHECK(sweep.rejections() >= sweep.cases_run() - acceptable); }
