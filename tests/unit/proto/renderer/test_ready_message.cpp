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

/* Vulkan (bit 0) and OpenCL (bit 2), each with import kinds. */
constexpr std::uint32_t offered = static_cast<std::uint32_t>(SubApi::vulkan) | static_cast<std::uint32_t>(SubApi::opencl);
const std::uint32_t     kinds[ReadyMessage::lanes] = {3u, 0u, 1u, 0u, 0u, 0u, 0u, 0u};

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

TEST(ready_message_encode, is_the_versions_the_sub_apis_and_eight_lanes_of_import_kinds)
{ GuardedBuffer bytes(ReadyMessage::encoded_size);
  const std::uint32_t every[ReadyMessage::lanes] = {0x10u, 0x11u, 0x12u, 0x13u, 0x14u, 0x15u, 0x16u, 0x17u};
  CHECK_STATUS(ReadyMessage(0x00010002u, 5u, 0xFFu, every).encode(bytes.data(), bytes.size()), Status::ok);
  const ByteLayout layout(bytes.data(), bytes.size());
  CHECK(layout.at(0u, 4u) == 0x00010002u);
  CHECK(layout.at(4u, 4u) == 5u);
  CHECK(layout.at(8u, 4u) == 0xFFu);
  for (std::uint32_t lane = 0u; lane < ReadyMessage::lanes; lane++)
      CHECK(layout.at(12u + 4u * lane, 4u) == 0x10u + lane);
  CHECK_EQ(ReadyMessage::encoded_size, 44u);
  CHECK_EQ(ReadyMessage::descriptors, 0u);
  CHECK(bytes.guards_intact()); }

TEST(ready_message, round_trips_through_its_encoding)
{ std::uint8_t bytes[ReadyMessage::encoded_size] = {};
  CHECK_STATUS(ReadyMessage(2u, 9u, offered, kinds).encode(bytes, sizeof bytes), Status::ok);
  ReadyMessage decoded;
  CHECK_EQ(decoded.sub_apis(), 0u);
  CHECK_EQ(decoded.import_kinds(0u), 0u);
  CHECK_STATUS(decoded.decode(bytes, sizeof bytes, 0u), Status::ok);
  CHECK_EQ(decoded.version(), 2u);
  CHECK_EQ(decoded.executor_abi(), 9u);
  CHECK_EQ(decoded.sub_apis(), offered);
  CHECK_EQ(decoded.import_kinds(0u), 3u);
  CHECK_EQ(decoded.import_kinds(1u), 0u);
  CHECK_EQ(decoded.import_kinds(2u), 1u);
  CHECK_EQ(decoded.import_kinds(ReadyMessage::lanes), 0u); }

/* An import kind for a sub-API the renderer cannot run would put a kind in
 * the capset no context of that API can use. */
TEST(ready_message, a_renderer_offering_nothing_has_no_import_kinds)
{ const std::uint32_t none[ReadyMessage::lanes] = {};
  std::uint8_t bytes[ReadyMessage::encoded_size] = {};
  CHECK_STATUS(ReadyMessage(1u, 1u, 0u, none).encode(bytes, sizeof bytes), Status::ok);
  CHECK_STATUS(ReadyMessage(1u, 1u, 0u, kinds).encode(bytes, sizeof bytes), Status::invalid_parameter); }

TEST(ready_message_encode, refuses_what_decode_would)
{ std::uint8_t bytes[ReadyMessage::encoded_size] = {};
  CHECK_STATUS(ReadyMessage(1u, 1u, offered | (1u << ReadyMessage::lanes), kinds).encode(bytes, sizeof bytes),
               Status::invalid_parameter);
  const std::uint32_t stray[ReadyMessage::lanes] = {3u, 0u, 1u, 0u, 0u, 0u, 0u, 4u};
  CHECK_STATUS(ReadyMessage(1u, 1u, offered, stray).encode(bytes, sizeof bytes), Status::invalid_parameter); }

TEST(ready_message_encode, a_short_or_missing_destination_is_too_large)
{ GuardedBuffer buffer(ReadyMessage::encoded_size - 1u);
  CHECK_STATUS(ReadyMessage(1u, 1u, offered, kinds).encode(buffer.data(), buffer.size()), Status::too_large);
  CHECK(buffer.guards_intact());
  CHECK_STATUS(ReadyMessage(1u, 1u, offered, kinds).encode(nullptr, ReadyMessage::encoded_size), Status::too_large); }

TEST(ready_message_decode, refuses_bad_masks_and_stays_unchanged)
{ struct Case { std::size_t offset; std::uint32_t value; };
  const Case cases[] = {{8u, offered | 0x100u}, {8u, 0x80000000u}, {12u + 4u * 1u, 1u}, {12u + 4u * 7u, 1u}};
  for (const Case &c : cases) {
      std::uint8_t bytes[ReadyMessage::encoded_size] = {};
      CHECK_STATUS(ReadyMessage(1u, 1u, offered, kinds).encode(bytes, sizeof bytes), Status::ok);
      for (std::size_t i = 0u; i < 4u; i++) bytes[c.offset + i] = static_cast<std::uint8_t>(c.value >> (8u * i));
      ReadyMessage decoded(7u, 7u, offered, kinds);
      CHECK_STATUS(decoded.decode(bytes, sizeof bytes, 0u), Status::malformed);
      CHECK_EQ(decoded.version(), 7u);
  } }

TEST(ready_message_decode, any_descriptor_other_length_or_no_bytes_is_malformed)
{ std::uint8_t bytes[ReadyMessage::encoded_size + 1u] = {};
  CHECK_STATUS(ReadyMessage(1u, 1u, offered, kinds).encode(bytes, sizeof bytes), Status::ok);
  ReadyMessage decoded;
  CHECK_STATUS(decoded.decode(bytes, ReadyMessage::encoded_size, 1u), Status::malformed);
  CHECK_STATUS(decoded.decode(bytes, ReadyMessage::encoded_size - 1u, 0u), Status::malformed);
  CHECK_STATUS(decoded.decode(bytes, ReadyMessage::encoded_size + 1u, 0u), Status::malformed);
  CHECK_STATUS(decoded.decode(nullptr, ReadyMessage::encoded_size, 0u), Status::malformed);
  CHECK_EQ(decoded.version(), 0u); }

TEST(ready_message_decode, survives_the_hostile_sweep)
{ std::uint8_t bytes[ReadyMessage::encoded_size] = {};
  CHECK_STATUS(ReadyMessage(2u, 9u, offered, kinds).encode(bytes, sizeof bytes), Status::ok);
  const av::HostileSweep sweep(rejects, bytes, sizeof bytes);
  sweep.run();
  CHECK(sweep.cases_run() > 256u);
  /* Both versions are free: 64 flips. Setting one of the six clear sub-API
   * bits below bit 8 offers a sub-API with no import kinds, which is allowed;
   * clearing bit 0 or 2 strands that lane's kinds, and bits 8 and up name no
   * lane. The kinds of lanes 0 and 2 are free, 64 flips; any bit in another
   * lane is a kind for a sub-API not offered. All zero is a renderer that
   * offers nothing, which is a message. With the untouched message,
   * 64 + 6 + 64 + 1 + 1. */
  const std::uint32_t acceptable = 64u + 6u + 64u + 1u + 1u;
  CHECK(sweep.rejections() >= sweep.cases_run() - acceptable); }
