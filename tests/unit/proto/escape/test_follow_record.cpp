#include "core/support/status.hpp"
#include "harness/framework/byte_layout.hpp"
#include "harness/framework/guarded_buffer.hpp"
#include "harness/framework/hostile_sweep.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/escape/follow_record.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

using av::ByteLayout;
using av::FollowRecord;
using av::GuardedBuffer;
using av::Status;

namespace {

bool rejects(const std::uint8_t *bytes, std::size_t length)
{
    FollowRecord record;
    const Status status = record.decode(bytes, length);
    if (status == Status::ok) {
        std::uint8_t again[FollowRecord::encoded_size] = {};
        if (record.encode(again, sizeof again) != Status::ok || std::memcmp(again, bytes, sizeof again) != 0)
            av::TestRegistry::instance().record_failure(__FILE__, __LINE__, "accepted input did not round trip");
    }
    return status != Status::ok;
}

}  // namespace

TEST(follow_record_encode, places_every_field_at_its_offset)
{ std::uint8_t bytes[FollowRecord::encoded_size] = {};
  CHECK_STATUS(FollowRecord(0x11223344u, 0x5566778899AABBCCull).encode(bytes, sizeof bytes), Status::ok);
  const ByteLayout layout(bytes, sizeof bytes);
  CHECK(layout.at(0u, 4u) == 0x00465641u);   /* "AVF" */
  CHECK(layout.at(4u, 4u) == 0x11223344u);
  CHECK(layout.at(8u, 8u) == 0x5566778899AABBCCull); }

TEST(follow_record, round_trips_through_its_encoding)
{ std::uint8_t bytes[FollowRecord::encoded_size] = {};
  CHECK_STATUS(FollowRecord(0xFFFFFFFFu, ~std::uint64_t{0}).encode(bytes, sizeof bytes), Status::ok);
  FollowRecord decoded;
  CHECK_STATUS(decoded.decode(bytes, sizeof bytes), Status::ok);
  CHECK_EQ(decoded.ring(), 0xFFFFFFFFu);
  CHECK(decoded.fence() == ~std::uint64_t{0}); }

TEST(follow_record, a_default_record_follows_nothing)
{ const FollowRecord record;
  CHECK_EQ(record.ring(), 0u);
  CHECK(record.fence() == 0u); }

/* A packet follows a ring by its handle and a fence the host completes: a
 * handle of 0 names no allocation, and fence 0 is never submitted, so either
 * is a record of nothing - and a packet with private data that does not decode
 * is an ordinary packet. */
TEST(follow_record_decode, rejects_a_bad_magic_ring_zero_and_fence_zero_and_stays_unchanged)
{ struct Case { std::size_t offset; std::size_t width; std::uint64_t value; };
  const Case cases[] = {{0u, 4u, 0x464B5653u}, {4u, 4u, 0u}, {8u, 8u, 0u}};
  for (const Case &c : cases) {
      std::uint8_t bytes[FollowRecord::encoded_size] = {};
      CHECK_STATUS(FollowRecord(7u, 9u).encode(bytes, sizeof bytes), Status::ok);
      for (std::size_t i = 0u; i < c.width; i++) bytes[c.offset + i] = static_cast<std::uint8_t>(c.value >> (8u * i));
      FollowRecord decoded(3u, 4u);
      CHECK_STATUS(decoded.decode(bytes, sizeof bytes), Status::malformed);
      CHECK_EQ(decoded.ring(), 3u);
      CHECK(decoded.fence() == 4u);
  } }

/* dxgkrnl hands every packet on an ordinary context private data of the size
 * the context reported; only exactly a record's length is one. */
TEST(follow_record_decode, any_other_length_is_malformed)
{ std::uint8_t bytes[FollowRecord::encoded_size + 1u] = {};
  CHECK_STATUS(FollowRecord(7u, 9u).encode(bytes, sizeof bytes), Status::ok);
  FollowRecord decoded;
  CHECK_STATUS(decoded.decode(bytes, FollowRecord::encoded_size - 1u), Status::malformed);
  CHECK_STATUS(decoded.decode(bytes, FollowRecord::encoded_size + 1u), Status::malformed);
  CHECK_STATUS(decoded.decode(bytes, 0u), Status::malformed);
  CHECK_STATUS(decoded.decode(nullptr, FollowRecord::encoded_size), Status::malformed);
  CHECK_EQ(decoded.ring(), 0u); }

TEST(follow_record_encode, a_short_destination_is_too_large_and_untouched)
{ GuardedBuffer buffer(FollowRecord::encoded_size - 1u);
  CHECK_STATUS(FollowRecord(7u, 9u).encode(buffer.data(), buffer.size()), Status::too_large);
  CHECK(buffer.guards_intact()); }

TEST(follow_record_decode, survives_the_hostile_sweep)
{ std::uint8_t bytes[FollowRecord::encoded_size] = {};
  CHECK_STATUS(FollowRecord(3u, 3u).encode(bytes, sizeof bytes), Status::ok);
  const av::HostileSweep sweep(rejects, bytes, sizeof bytes);
  sweep.run();
  CHECK(sweep.cases_run() > 128u);
  /* The format constrains the magic and forbids a zero ring or fence. Ring and
   * fence are 3, with two bits set, so no single flip clears either: all 32
   * flips of the ring and 64 of the fence are acceptable. Every shorter input
   * is not 16 bytes, all-zero and all-ones carry no magic, and the untouched
   * record is the last acceptance. */
  const std::uint32_t acceptable = 32u + 64u + 1u;
  CHECK(sweep.rejections() >= sweep.cases_run() - acceptable); }
