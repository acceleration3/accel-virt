#include "core/support/status.hpp"
#include "harness/framework/byte_layout.hpp"
#include "harness/framework/guarded_buffer.hpp"
#include "harness/framework/hostile_sweep.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/context/device_uuid.hpp"
#include "proto/renderer/hello_message.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

using av::ByteLayout;
using av::GuardedBuffer;
using av::HelloMessage;
using av::Status;

namespace {

const std::uint8_t uuid[HelloMessage::uuid_bytes] = {0x10u, 0x11u, 0x12u, 0x13u, 0x14u, 0x15u, 0x16u, 0x17u,
                                                     0x18u, 0x19u, 0x1Au, 0x1Bu, 0x1Cu, 0x1Du, 0x1Eu, 0x1Fu};
const char         path[] = "/x.so";
const std::uint8_t capset[] = {0xC0u, 0xC1u, 0xC2u, 0xC3u};
constexpr std::uint32_t path_length = sizeof path - 1u;
constexpr std::size_t   hello_length = HelloMessage::fixed_size + path_length + sizeof capset;

HelloMessage sample()
{
    return HelloMessage(3u, 7u, av::DeviceUuid(uuid), path, path_length, capset, sizeof capset);
}

bool rejects(const std::uint8_t *bytes, std::size_t length)
{
    HelloMessage message;
    const Status status = message.decode(bytes, length, HelloMessage::descriptors);
    if (status == Status::ok) {
        std::uint8_t again[hello_length + 64u] = {};
        if (message.encoded_length() != length || message.encode(again, sizeof again) != Status::ok ||
            std::memcmp(again, bytes, length) != 0)
            av::TestRegistry::instance().record_failure(__FILE__, __LINE__, "accepted input did not round trip");
    }
    return status != Status::ok;
}

}  // namespace

TEST(hello_message_encode, is_the_fixed_fields_then_the_path_then_the_capset)
{ GuardedBuffer bytes(hello_length);
  CHECK_EQ(sample().encoded_length(), hello_length);
  CHECK_STATUS(sample().encode(bytes.data(), bytes.size()), Status::ok);
  const ByteLayout layout(bytes.data(), bytes.size());
  CHECK(layout.at(0u, 4u) == 3u);
  CHECK(layout.at(4u, 4u) == 7u);
  CHECK(layout.at(8u, 8u) == 0x1716151413121110ull);
  CHECK(layout.at(16u, 8u) == 0x1F1E1D1C1B1A1918ull);
  CHECK(layout.at(24u, 4u) == path_length);
  CHECK(layout.at(28u, 4u) == sizeof capset);
  CHECK(std::memcmp(bytes.data() + 32u, path, path_length) == 0);
  CHECK(layout.at(32u + path_length, 4u) == 0xC3C2C1C0u);
  CHECK_EQ(HelloMessage::descriptors, 0u);
  CHECK(bytes.guards_intact()); }

TEST(hello_message, round_trips_and_views_the_bytes_it_decoded)
{ std::uint8_t bytes[hello_length] = {};
  CHECK_STATUS(sample().encode(bytes, sizeof bytes), Status::ok);
  HelloMessage decoded;
  CHECK(decoded.executor_path() == nullptr);
  CHECK(decoded.capset() == nullptr);
  CHECK_EQ(decoded.encoded_length(), HelloMessage::fixed_size);
  CHECK_STATUS(decoded.decode(bytes, sizeof bytes, 0u), Status::ok);
  CHECK_EQ(decoded.version(), 3u);
  CHECK_EQ(decoded.epoch(), 7u);
  CHECK(std::memcmp(decoded.gpu_uuid(), uuid, sizeof uuid) == 0);
  CHECK_EQ(decoded.executor_path_length(), path_length);
  CHECK(decoded.executor_path() == reinterpret_cast<const char *>(bytes + HelloMessage::fixed_size));
  CHECK_EQ(decoded.capset_length(), static_cast<std::uint32_t>(sizeof capset));
  CHECK(decoded.capset() == bytes + HelloMessage::fixed_size + path_length); }

TEST(hello_message, the_longest_path_and_capset_round_trip)
{ static char           long_path[HelloMessage::max_path_length];
  static std::uint8_t   long_capset[HelloMessage::max_capset_length];
  static std::uint8_t   bytes[HelloMessage::fixed_size + sizeof long_path + sizeof long_capset];
  std::memset(long_path, 'a', sizeof long_path);
  const HelloMessage hello(1u, 1u, av::DeviceUuid(uuid), long_path, sizeof long_path, long_capset, sizeof long_capset);
  CHECK_STATUS(hello.encode(bytes, sizeof bytes), Status::ok);
  HelloMessage decoded;
  CHECK_STATUS(decoded.decode(bytes, sizeof bytes, 0u), Status::ok);
  CHECK_EQ(decoded.executor_path_length(), HelloMessage::max_path_length);
  CHECK_EQ(decoded.capset_length(), HelloMessage::max_capset_length); }

/* dlopen reads the path up to its NUL: one inside would load another file
 * than the one QEMU named. */
TEST(hello_message_encode, refuses_what_decode_would)
{ static char         long_path[HelloMessage::max_path_length + 1u];
  static std::uint8_t long_capset[HelloMessage::max_capset_length + 1u];
  static std::uint8_t bytes[HelloMessage::fixed_size + sizeof long_path + sizeof long_capset];
  std::memset(long_path, 'a', sizeof long_path);
  const char with_nul[] = {'/', '\0', 'x'};
  CHECK_STATUS(HelloMessage(1u, 1u, av::DeviceUuid(uuid), path, 0u, capset, 4u).encode(bytes, sizeof bytes), Status::invalid_parameter);
  CHECK_STATUS(HelloMessage(1u, 1u, av::DeviceUuid(uuid), nullptr, 5u, capset, 4u).encode(bytes, sizeof bytes),
               Status::invalid_parameter);
  CHECK_STATUS(HelloMessage(1u, 1u, av::DeviceUuid(uuid), long_path, sizeof long_path, capset, 4u).encode(bytes, sizeof bytes),
               Status::invalid_parameter);
  CHECK_STATUS(HelloMessage(1u, 1u, av::DeviceUuid(uuid), path, path_length, capset, 0u).encode(bytes, sizeof bytes),
               Status::invalid_parameter);
  CHECK_STATUS(HelloMessage(1u, 1u, av::DeviceUuid(uuid), path, path_length, nullptr, 4u).encode(bytes, sizeof bytes),
               Status::invalid_parameter);
  CHECK_STATUS(HelloMessage(1u, 1u, av::DeviceUuid(uuid), path, path_length, long_capset, sizeof long_capset).encode(bytes, sizeof bytes),
               Status::invalid_parameter);
  CHECK_STATUS(HelloMessage(1u, 1u, av::DeviceUuid(uuid), with_nul, sizeof with_nul, capset, 4u).encode(bytes, sizeof bytes),
               Status::invalid_parameter); }

TEST(hello_message, a_null_path_or_capset_is_empty)
{ const HelloMessage hello(1u, 1u, av::DeviceUuid(uuid), nullptr, 9u, nullptr, 9u);
  CHECK_EQ(hello.executor_path_length(), 0u);
  CHECK_EQ(hello.capset_length(), 0u); }

TEST(hello_message_encode, a_short_or_missing_destination_is_too_large)
{ GuardedBuffer buffer(hello_length - 1u);
  CHECK_STATUS(sample().encode(buffer.data(), buffer.size()), Status::too_large);
  CHECK(buffer.guards_intact());
  CHECK_STATUS(sample().encode(nullptr, hello_length), Status::too_large); }

TEST(hello_message_decode, a_descriptor_is_malformed)
{ std::uint8_t bytes[hello_length] = {};
  CHECK_STATUS(sample().encode(bytes, sizeof bytes), Status::ok);
  HelloMessage decoded;
  CHECK_STATUS(decoded.decode(bytes, sizeof bytes, 1u), Status::malformed);
  CHECK_EQ(decoded.version(), 0u); }

TEST(hello_message_decode, a_length_other_than_the_fields_give_is_malformed)
{ std::uint8_t bytes[hello_length + 1u] = {};
  CHECK_STATUS(sample().encode(bytes, sizeof bytes), Status::ok);
  HelloMessage decoded;
  CHECK_STATUS(decoded.decode(bytes, hello_length - 1u, 0u), Status::malformed);
  CHECK_STATUS(decoded.decode(bytes, hello_length + 1u, 0u), Status::malformed);
  CHECK_STATUS(decoded.decode(bytes, HelloMessage::fixed_size - 1u, 0u), Status::malformed);
  CHECK_STATUS(decoded.decode(nullptr, hello_length, 0u), Status::malformed);
  CHECK_EQ(decoded.version(), 0u); }

/* A length past the bound is refused before it is added to anything, whatever
 * arrived after it. */
TEST(hello_message_decode, lengths_past_their_bounds_are_malformed)
{ static std::uint8_t bytes[HelloMessage::fixed_size + HelloMessage::max_path_length + 1u + 4u];
  std::memset(bytes, 'a', sizeof bytes);
  const std::uint32_t lengths[][2] = {{HelloMessage::max_path_length + 1u, 4u},
                                      {4u, HelloMessage::max_capset_length + 1u},
                                      {0xFFFFFFFFu, 0xFFFFFFFFu}};
  for (const auto &pair : lengths) {
      for (std::size_t i = 0u; i < 4u; i++) {
          bytes[24u + i] = static_cast<std::uint8_t>(pair[0] >> (8u * i));
          bytes[28u + i] = static_cast<std::uint8_t>(pair[1] >> (8u * i));
      }
      HelloMessage decoded;
      CHECK_STATUS(decoded.decode(bytes, sizeof bytes, 0u), Status::malformed);
  } }

TEST(hello_message_decode, refuses_an_empty_path_or_capset_and_a_nul_in_the_path_and_stays_unchanged)
{ struct Case { std::uint32_t path_length; std::uint32_t capset_length; std::uint8_t first_path_byte; };
  const Case cases[] = {{0u, 4u, 'x'}, {4u, 0u, 'x'}, {4u, 4u, 0u}};
  for (const Case &c : cases) {
      std::uint8_t bytes[HelloMessage::fixed_size + 8u] = {};
      for (std::size_t i = 0u; i < 4u; i++) {
          bytes[24u + i] = static_cast<std::uint8_t>(c.path_length >> (8u * i));
          bytes[28u + i] = static_cast<std::uint8_t>(c.capset_length >> (8u * i));
      }
      std::memset(bytes + HelloMessage::fixed_size, 'x', 8u);
      bytes[HelloMessage::fixed_size] = c.first_path_byte;
      const std::size_t length = HelloMessage::fixed_size + c.path_length + c.capset_length;
      HelloMessage decoded(9u, 9u, av::DeviceUuid(uuid), path, path_length, capset, sizeof capset);
      CHECK_STATUS(decoded.decode(bytes, length, 0u), Status::malformed);
      CHECK_EQ(decoded.version(), 9u);
      CHECK(decoded.executor_path() == path);
  } }

TEST(hello_message_decode, survives_the_hostile_sweep)
{ std::uint8_t bytes[hello_length] = {};
  CHECK_STATUS(sample().encode(bytes, sizeof bytes), Status::ok);
  const av::HostileSweep sweep(rejects, bytes, sizeof bytes);
  sweep.run();
  CHECK(sweep.cases_run() > 256u);
  /* Version, epoch and the uuid are free: 32 + 32 + 128 flips. A flip of
   * either length claims bytes that did not arrive. "/x.so" has no byte with a
   * single bit set, so none of its 40 flips makes a NUL; the capset's 32 are
   * free. With the untouched message, 265 acceptances - the sweep is mostly
   * payload, and what it proves is the lengths. */
  const std::uint32_t acceptable = 32u + 32u + 128u + 40u + 32u + 1u;
  CHECK(sweep.rejections() >= sweep.cases_run() - acceptable); }
