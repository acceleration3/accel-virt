#include "core/support/status.hpp"
#include "device/recording/record_header.hpp"
#include "device/recording/record_kind.hpp"
#include "harness/framework/byte_layout.hpp"
#include "harness/framework/guarded_buffer.hpp"
#include "harness/framework/hostile_sweep.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

using av::ByteLayout;
using av::GuardedBuffer;
using av::RecordHeader;
using av::RecordKind;
using av::Status;

namespace {

bool rejects(const std::uint8_t *bytes, std::size_t length)
{
    RecordHeader header;
    const Status status = header.decode(bytes, length);
    if (status == Status::ok) {
        std::uint8_t again[RecordHeader::encoded_size] = {};
        if (header.encode(again, sizeof again) != Status::ok
            || std::memcmp(again, bytes, sizeof again) != 0)
            av::TestRegistry::instance().record_failure(__FILE__, __LINE__,
                                                        "accepted input did not round trip");
    }
    return status != Status::ok;
}

RecordHeader a_batch(std::uint32_t length)
{
    return RecordHeader(RecordKind::batch, 7u, length, 0x1122334455667788u, 9u);
}

}  // namespace

TEST(record_header_encode, places_every_field_at_its_offset)
{ std::uint8_t bytes[RecordHeader::encoded_size] = {};
  CHECK_STATUS(a_batch(0x40u).encode(bytes, sizeof bytes), Status::ok);
  const ByteLayout layout(bytes, sizeof bytes);
  CHECK(layout.at(0u, 4u) == 0x444B5652u);
  CHECK(layout.at(4u, 2u) == 2u);
  CHECK(layout.at(6u, 2u) == 0u);
  CHECK(layout.at(8u, 4u) == 7u);
  CHECK(layout.at(12u, 4u) == 0x40u);
  CHECK(layout.at(16u, 8u) == 0x1122334455667788u);
  CHECK(layout.at(24u, 8u) == 9u); }

TEST(record_header, round_trips_every_kind)
{ const RecordKind kinds[] = {RecordKind::context_setup, RecordKind::batch, RecordKind::reply,
                              RecordKind::fence_completed, RecordKind::fault, RecordKind::blob,
                              RecordKind::blob_bytes};
  for (const RecordKind kind : kinds) {
      std::uint8_t bytes[RecordHeader::encoded_size] = {};
      CHECK_STATUS(RecordHeader(kind, 3u, RecordHeader::max_payload, ~std::uint64_t{0},
                                ~std::uint64_t{0}).encode(bytes, sizeof bytes), Status::ok);
      RecordHeader decoded;
      CHECK_STATUS(decoded.decode(bytes, sizeof bytes), Status::ok);
      CHECK(decoded.kind() == kind);
      CHECK_EQ(decoded.context(), 3u);
      CHECK_EQ(decoded.length(), RecordHeader::max_payload);
      CHECK(decoded.timestamp() == ~std::uint64_t{0});
      CHECK(decoded.fence() == ~std::uint64_t{0});
  } }

TEST(record_header_span, is_the_header_and_its_payload)
{ CHECK(a_batch(0u).span() == RecordHeader::encoded_size);
  CHECK(a_batch(1u).span() == RecordHeader::encoded_size + 1u);
  CHECK(a_batch(RecordHeader::max_payload).span()
        == RecordHeader::encoded_size + RecordHeader::max_payload); }

TEST(record_header_fits, accepts_a_span_up_to_what_is_left)
{ CHECK(a_batch(8u).fits(RecordHeader::encoded_size + 8u));
  CHECK(!a_batch(8u).fits(RecordHeader::encoded_size + 7u));
  CHECK(!a_batch(0u).fits(RecordHeader::encoded_size - 1u));
  CHECK(a_batch(0u).fits(~std::uint64_t{0})); }

TEST(record_header_encode, refuses_a_payload_no_recording_may_hold)
{ GuardedBuffer buffer(RecordHeader::encoded_size);
  CHECK_STATUS(a_batch(RecordHeader::max_payload + 1u).encode(buffer.data(), buffer.size()),
               Status::invalid_parameter);
  CHECK(buffer.guards_intact()); }

TEST(record_header_encode, a_short_destination_is_too_large_and_untouched)
{ GuardedBuffer buffer(RecordHeader::encoded_size - 1u);
  CHECK_STATUS(a_batch(0u).encode(buffer.data(), buffer.size()), Status::too_large);
  CHECK(buffer.guards_intact()); }

TEST(record_header_decode, rejects_a_bad_magic_a_kind_nobody_wrote_and_a_reserved_field)
{ struct Case { std::size_t offset; std::uint8_t value; };
  /* magic, kind 0, kind 8, and the reserved halfword. */
  const Case cases[] = {{0u, 0u}, {3u, 0x45u}, {4u, 0u}, {4u, 8u}, {5u, 1u}, {6u, 1u}, {7u, 1u}};
  for (const Case &c : cases) {
      std::uint8_t bytes[RecordHeader::encoded_size] = {};
      CHECK_STATUS(a_batch(16u).encode(bytes, sizeof bytes), Status::ok);
      bytes[c.offset] = c.value;
      RecordHeader decoded(RecordKind::reply, 1u, 4u, 5u, 6u);
      CHECK_STATUS(decoded.decode(bytes, sizeof bytes), Status::malformed);
      CHECK(decoded.kind() == RecordKind::reply);
      CHECK_EQ(decoded.length(), 4u);
  } }

TEST(record_header_decode, rejects_a_length_larger_than_any_record)
{ std::uint8_t bytes[RecordHeader::encoded_size] = {};
  CHECK_STATUS(a_batch(RecordHeader::max_payload).encode(bytes, sizeof bytes), Status::ok);
  RecordHeader decoded;
  CHECK_STATUS(decoded.decode(bytes, sizeof bytes), Status::ok);
  bytes[12] = 1u;  /* max_payload + 1 */
  CHECK_STATUS(decoded.decode(bytes, sizeof bytes), Status::malformed); }

TEST(record_header_decode, a_short_buffer_is_malformed)
{ std::uint8_t bytes[RecordHeader::encoded_size] = {};
  CHECK_STATUS(a_batch(0u).encode(bytes, sizeof bytes), Status::ok);
  RecordHeader decoded;
  for (std::size_t length = 0u; length < sizeof bytes; length++)
      CHECK_STATUS(decoded.decode(bytes, length), Status::malformed);
  CHECK_STATUS(decoded.decode(nullptr, sizeof bytes), Status::malformed); }

TEST(record_header_decode, survives_the_hostile_sweep)
{ std::uint8_t bytes[RecordHeader::encoded_size] = {};
  CHECK_STATUS(a_batch(40u).encode(bytes, sizeof bytes), Status::ok);
  const av::HostileSweep sweep(rejects, bytes, sizeof bytes);
  sweep.run();
  CHECK(sweep.cases_run() > 256u); }
