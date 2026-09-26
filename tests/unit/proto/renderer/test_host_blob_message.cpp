#include "core/support/status.hpp"
#include "harness/framework/byte_layout.hpp"
#include "harness/framework/guarded_buffer.hpp"
#include "harness/framework/hostile_sweep.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/renderer/host_blob_message.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

using av::ByteLayout;
using av::GuardedBuffer;
using av::HostBlobMessage;
using av::Status;

namespace {

bool rejects(const std::uint8_t *bytes, std::size_t length)
{
    HostBlobMessage message;
    const Status status = message.decode(bytes, length, HostBlobMessage::descriptors);
    if (status == Status::ok) {
        std::uint8_t again[HostBlobMessage::encoded_size] = {};
        if (message.encode(again, sizeof again) != Status::ok || std::memcmp(again, bytes, sizeof again) != 0)
            av::TestRegistry::instance().record_failure(__FILE__, __LINE__, "accepted input did not round trip");
    }
    return status != Status::ok;
}

}  // namespace

TEST(host_blob_message_encode, is_the_blob_four_reserved_bytes_and_the_size)
{ GuardedBuffer bytes(HostBlobMessage::encoded_size);
  CHECK_STATUS(HostBlobMessage(0x11223344u, 0x5566778899AABBCCull).encode(bytes.data(), bytes.size()), Status::ok);
  const ByteLayout layout(bytes.data(), bytes.size());
  CHECK(layout.at(0u, 4u) == 0x11223344u);
  CHECK(layout.at(4u, 4u) == 0u);
  CHECK(layout.at(8u, 8u) == 0x5566778899AABBCCull);
  CHECK_EQ(HostBlobMessage::descriptors, 1u);
  CHECK(bytes.guards_intact()); }

TEST(host_blob_message, round_trips_through_its_encoding)
{ std::uint8_t bytes[HostBlobMessage::encoded_size] = {};
  CHECK_STATUS(HostBlobMessage(0xFFFFFFFFu, ~std::uint64_t{0}).encode(bytes, sizeof bytes), Status::ok);
  HostBlobMessage decoded;
  CHECK_EQ(decoded.blob(), 0u);
  CHECK(decoded.size() == 0u);
  CHECK_STATUS(decoded.decode(bytes, sizeof bytes, 1u), Status::ok);
  CHECK_EQ(decoded.blob(), 0xFFFFFFFFu);
  CHECK(decoded.size() == ~std::uint64_t{0}); }

TEST(host_blob_message_encode, refuses_blob_zero_and_an_empty_blob)
{ std::uint8_t bytes[HostBlobMessage::encoded_size] = {};
  CHECK_STATUS(HostBlobMessage(0u, 1u).encode(bytes, sizeof bytes), Status::invalid_parameter);
  CHECK_STATUS(HostBlobMessage(1u, 0u).encode(bytes, sizeof bytes), Status::invalid_parameter); }

TEST(host_blob_message_encode, a_short_or_missing_destination_is_too_large)
{ GuardedBuffer buffer(HostBlobMessage::encoded_size - 1u);
  CHECK_STATUS(HostBlobMessage(1u, 1u).encode(buffer.data(), buffer.size()), Status::too_large);
  CHECK(buffer.guards_intact());
  CHECK_STATUS(HostBlobMessage(1u, 1u).encode(nullptr, HostBlobMessage::encoded_size), Status::too_large); }

/* The hostmem memfd is the blob; the renderer cannot import one it was not
 * given, and a second would leak. */
TEST(host_blob_message_decode, needs_exactly_one_descriptor)
{ std::uint8_t bytes[HostBlobMessage::encoded_size] = {};
  CHECK_STATUS(HostBlobMessage(2u, 0x1000u).encode(bytes, sizeof bytes), Status::ok);
  HostBlobMessage decoded;
  CHECK_STATUS(decoded.decode(bytes, sizeof bytes, 0u), Status::malformed);
  CHECK_STATUS(decoded.decode(bytes, sizeof bytes, 2u), Status::malformed);
  CHECK_EQ(decoded.blob(), 0u); }

TEST(host_blob_message_decode, refuses_bad_fields_and_stays_unchanged)
{ struct Case { std::size_t offset; std::size_t width; std::uint64_t value; };
  const Case cases[] = {{0u, 4u, 0u}, {4u, 4u, 0x80000000u}, {8u, 8u, 0u}};
  for (const Case &c : cases) {
      std::uint8_t bytes[HostBlobMessage::encoded_size] = {};
      CHECK_STATUS(HostBlobMessage(2u, 0x1000u).encode(bytes, sizeof bytes), Status::ok);
      for (std::size_t i = 0u; i < c.width; i++) bytes[c.offset + i] = static_cast<std::uint8_t>(c.value >> (8u * i));
      HostBlobMessage decoded(3u, 4u);
      CHECK_STATUS(decoded.decode(bytes, sizeof bytes, 1u), Status::malformed);
      CHECK_EQ(decoded.blob(), 3u);
      CHECK(decoded.size() == 4u);
  } }

TEST(host_blob_message_decode, any_other_length_or_no_bytes_is_malformed)
{ std::uint8_t bytes[HostBlobMessage::encoded_size + 1u] = {};
  CHECK_STATUS(HostBlobMessage(2u, 0x1000u).encode(bytes, sizeof bytes), Status::ok);
  HostBlobMessage decoded;
  CHECK_STATUS(decoded.decode(bytes, HostBlobMessage::encoded_size - 1u, 1u), Status::malformed);
  CHECK_STATUS(decoded.decode(bytes, HostBlobMessage::encoded_size + 1u, 1u), Status::malformed);
  CHECK_STATUS(decoded.decode(nullptr, HostBlobMessage::encoded_size, 1u), Status::malformed); }

TEST(host_blob_message_decode, survives_the_hostile_sweep)
{ std::uint8_t bytes[HostBlobMessage::encoded_size] = {};
  CHECK_STATUS(HostBlobMessage(3u, 3u).encode(bytes, sizeof bytes), Status::ok);
  const av::HostileSweep sweep(rejects, bytes, sizeof bytes);
  sweep.run();
  CHECK(sweep.cases_run() > 128u);
  /* Blob and size are 3, two bits set, so no single flip zeroes either: all
   * 32 flips of the blob and 64 of the size are acceptable. The reserved
   * word must stay zero. With the untouched message, 32 + 64 + 1. */
  const std::uint32_t acceptable = 32u + 64u + 1u;
  CHECK(sweep.rejections() >= sweep.cases_run() - acceptable); }
