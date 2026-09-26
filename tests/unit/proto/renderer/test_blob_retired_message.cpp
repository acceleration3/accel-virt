#include "core/support/status.hpp"
#include "harness/framework/byte_layout.hpp"
#include "harness/framework/guarded_buffer.hpp"
#include "harness/framework/hostile_sweep.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/renderer/blob_retired_message.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

using av::BlobRetiredMessage;
using av::ByteLayout;
using av::GuardedBuffer;
using av::Status;

namespace {

bool rejects(const std::uint8_t *bytes, std::size_t length)
{
    BlobRetiredMessage message;
    const Status status = message.decode(bytes, length, BlobRetiredMessage::descriptors);
    if (status == Status::ok) {
        std::uint8_t again[BlobRetiredMessage::encoded_size] = {};
        if (message.encode(again, sizeof again) != Status::ok || std::memcmp(again, bytes, sizeof again) != 0)
            av::TestRegistry::instance().record_failure(__FILE__, __LINE__, "accepted input did not round trip");
    }
    return status != Status::ok;
}

}  // namespace

TEST(blob_retired_message_encode, is_the_blob)
{ GuardedBuffer bytes(BlobRetiredMessage::encoded_size);
  CHECK_STATUS(BlobRetiredMessage(0x11223344u).encode(bytes.data(), bytes.size()), Status::ok);
  const ByteLayout layout(bytes.data(), bytes.size());
  CHECK(layout.at(0u, 4u) == 0x11223344u);
  CHECK_EQ(BlobRetiredMessage::descriptors, 0u);
  CHECK(bytes.guards_intact()); }

TEST(blob_retired_message, round_trips_through_its_encoding)
{ std::uint8_t bytes[BlobRetiredMessage::encoded_size] = {};
  CHECK_STATUS(BlobRetiredMessage(0xFFFFFFFFu).encode(bytes, sizeof bytes), Status::ok);
  BlobRetiredMessage decoded;
  CHECK_EQ(decoded.blob(), 0u);
  CHECK_STATUS(decoded.decode(bytes, sizeof bytes, 0u), Status::ok);
  CHECK_EQ(decoded.blob(), 0xFFFFFFFFu); }

/* Blob 0 names no virtio-gpu resource, so retiring it is a sender's bug. */
TEST(blob_retired_message, blob_zero_is_refused_both_ways_and_leaves_the_message_unchanged)
{ std::uint8_t bytes[BlobRetiredMessage::encoded_size] = {};
  CHECK_STATUS(BlobRetiredMessage(0u).encode(bytes, sizeof bytes), Status::invalid_parameter);
  BlobRetiredMessage decoded(7u);
  CHECK_STATUS(decoded.decode(bytes, sizeof bytes, 0u), Status::malformed);
  CHECK_EQ(decoded.blob(), 7u); }

TEST(blob_retired_message_encode, a_short_or_missing_destination_is_too_large)
{ GuardedBuffer buffer(BlobRetiredMessage::encoded_size - 1u);
  CHECK_STATUS(BlobRetiredMessage(1u).encode(buffer.data(), buffer.size()), Status::too_large);
  CHECK(buffer.guards_intact());
  CHECK_STATUS(BlobRetiredMessage(1u).encode(nullptr, BlobRetiredMessage::encoded_size), Status::too_large); }

/* A descriptor on a message that takes none would be leaked by a receiver
 * that trusted the kind. */
TEST(blob_retired_message_decode, any_descriptor_other_length_or_no_bytes_is_malformed)
{ std::uint8_t bytes[BlobRetiredMessage::encoded_size + 1u] = {};
  CHECK_STATUS(BlobRetiredMessage(9u).encode(bytes, sizeof bytes), Status::ok);
  BlobRetiredMessage decoded;
  CHECK_STATUS(decoded.decode(bytes, BlobRetiredMessage::encoded_size, 1u), Status::malformed);
  CHECK_STATUS(decoded.decode(bytes, BlobRetiredMessage::encoded_size - 1u, 0u), Status::malformed);
  CHECK_STATUS(decoded.decode(bytes, BlobRetiredMessage::encoded_size + 1u, 0u), Status::malformed);
  CHECK_STATUS(decoded.decode(nullptr, BlobRetiredMessage::encoded_size, 0u), Status::malformed);
  CHECK_EQ(decoded.blob(), 0u); }

TEST(blob_retired_message_decode, survives_the_hostile_sweep)
{ std::uint8_t bytes[BlobRetiredMessage::encoded_size] = {};
  CHECK_STATUS(BlobRetiredMessage(3u).encode(bytes, sizeof bytes), Status::ok);
  const av::HostileSweep sweep(rejects, bytes, sizeof bytes);
  sweep.run();
  CHECK(sweep.cases_run() > 32u);
  /* Blob 3 has two bits set, so no single flip zeroes it: its 32 flips, the
   * all-ones word and the untouched message are acceptances. */
  const std::uint32_t acceptable = 32u + 1u + 1u;
  CHECK(sweep.rejections() >= sweep.cases_run() - acceptable); }
