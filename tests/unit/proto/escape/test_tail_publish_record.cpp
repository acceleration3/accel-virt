#include "core/support/status.hpp"
#include "harness/framework/byte_layout.hpp"
#include "harness/framework/guarded_buffer.hpp"
#include "harness/framework/hostile_sweep.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/escape/tail_publish_record.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

using av::ByteLayout;
using av::GuardedBuffer;
using av::Status;
using av::TailPublishRecord;

namespace {

bool rejects(const std::uint8_t *bytes, std::size_t length)
{
    TailPublishRecord record;
    const Status status = record.decode(bytes, length);
    if (status == Status::ok) {
        std::uint8_t again[TailPublishRecord::encoded_size] = {};
        if (record.encode(again, sizeof again) != Status::ok || std::memcmp(again, bytes, sizeof again) != 0)
            av::TestRegistry::instance().record_failure(__FILE__, __LINE__, "accepted input did not round trip");
    }
    return status != Status::ok;
}

}

TEST(tail_publish_record_encode, is_the_magic_the_ring_and_zeros)
{ GuardedBuffer bytes(TailPublishRecord::encoded_size);
  CHECK_STATUS(TailPublishRecord(0x12345678u).encode(bytes.data(), bytes.size()), Status::ok);
  const ByteLayout layout(bytes.data(), bytes.size());
  CHECK(layout.at(0u, 4u) == 0x50564B52u);
  CHECK(layout.at(4u, 4u) == 0x12345678u);
  for (std::size_t offset = 8u; offset < TailPublishRecord::encoded_size; offset += 4u)
      CHECK(layout.at(offset, 4u) == 0u);
  CHECK_EQ(TailPublishRecord::encoded_size, 32u);
  CHECK(bytes.guards_intact()); }

TEST(tail_publish_record, round_trips_through_its_encoding)
{ std::uint8_t bytes[TailPublishRecord::encoded_size] = {};
  CHECK_STATUS(TailPublishRecord(0xFFFFFFFFu).encode(bytes, sizeof bytes), Status::ok);
  TailPublishRecord decoded;
  CHECK_EQ(decoded.ring(), 0u);
  CHECK_STATUS(decoded.decode(bytes, sizeof bytes), Status::ok);
  CHECK_EQ(decoded.ring(), 0xFFFFFFFFu); }

TEST(tail_publish_record_decode, a_refused_record_keeps_the_ring_it_had)
{ std::uint8_t bytes[TailPublishRecord::encoded_size] = {};
  CHECK_STATUS(TailPublishRecord(7u).encode(bytes, sizeof bytes), Status::ok);
  TailPublishRecord decoded;
  CHECK_STATUS(decoded.decode(bytes, sizeof bytes), Status::ok);
  CHECK_STATUS(TailPublishRecord(9u).encode(bytes, sizeof bytes), Status::ok);
  bytes[8] = 1u;
  CHECK_STATUS(decoded.decode(bytes, sizeof bytes), Status::malformed);
  CHECK_EQ(decoded.ring(), 7u); }

TEST(tail_publish_record_encode, a_short_or_missing_destination_is_too_large)
{ GuardedBuffer buffer(TailPublishRecord::encoded_size - 1u);
  CHECK_STATUS(TailPublishRecord().encode(buffer.data(), buffer.size()), Status::too_large);
  CHECK(buffer.guards_intact());
  CHECK_STATUS(TailPublishRecord().encode(nullptr, TailPublishRecord::encoded_size), Status::too_large); }

TEST(tail_publish_record_decode, refuses_another_magic_a_reserved_byte_and_any_other_length)
{ std::uint8_t bytes[TailPublishRecord::encoded_size + 1u] = {};
  CHECK_STATUS(TailPublishRecord().encode(bytes, sizeof bytes), Status::ok);
  TailPublishRecord decoded;
  CHECK_STATUS(decoded.decode(bytes, TailPublishRecord::encoded_size - 1u), Status::malformed);
  CHECK_STATUS(decoded.decode(bytes, TailPublishRecord::encoded_size + 1u), Status::malformed);
  CHECK_STATUS(decoded.decode(nullptr, TailPublishRecord::encoded_size), Status::malformed);
  bytes[31] = 1u;
  CHECK_STATUS(decoded.decode(bytes, TailPublishRecord::encoded_size), Status::malformed);
  bytes[31] = 0u;
  bytes[8] = 0x80u;
  CHECK_STATUS(decoded.decode(bytes, TailPublishRecord::encoded_size), Status::malformed);
  bytes[8] = 0u;
  bytes[0] ^= 1u;
  CHECK_STATUS(decoded.decode(bytes, TailPublishRecord::encoded_size), Status::malformed); }

TEST(tail_publish_record_decode, survives_the_hostile_sweep)
{ std::uint8_t bytes[TailPublishRecord::encoded_size] = {};
  CHECK_STATUS(TailPublishRecord().encode(bytes, sizeof bytes), Status::ok);
  const av::HostileSweep sweep(rejects, bytes, sizeof bytes);
  sweep.run();
  CHECK(sweep.cases_run() > 256u);
  const std::uint32_t acceptable = 32u + 1u;
  CHECK(sweep.rejections() >= sweep.cases_run() - acceptable); }
