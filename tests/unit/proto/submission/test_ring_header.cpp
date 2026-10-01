#include "core/support/status.hpp"
#include "harness/framework/byte_layout.hpp"
#include "harness/framework/guarded_buffer.hpp"
#include "harness/framework/hostile_sweep.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/submission/ring_header.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

using av::ByteLayout;
using av::GuardedBuffer;
using av::RingHeader;
using av::Status;

namespace {

/* Byte 5 is the device's awake flag and bytes 24..31 the fence the guest last
 * published: the header reads past both, so neither is part of a round trip. */
constexpr std::size_t awake_offset = 5u;
constexpr std::size_t published_fence_offset = 24u;

bool rejects(const std::uint8_t *bytes, std::size_t length)
{
    RingHeader header;
    const Status status = header.decode(bytes, length);
    if (status == Status::ok) {
        std::uint8_t again[RingHeader::encoded_size] = {};
        const bool encoded = header.encode(again, sizeof again) == Status::ok;
        again[awake_offset] = bytes[awake_offset];
        std::memcpy(again + published_fence_offset, bytes + published_fence_offset, 8u);
        if (!encoded || std::memcmp(again, bytes, sizeof again) != 0)
            av::TestRegistry::instance().record_failure(__FILE__, __LINE__, "accepted input did not round trip");
    }
    return status != Status::ok;
}

}  // namespace

TEST(ring_header_encode, places_every_field_at_its_offset)
{ std::uint8_t bytes[RingHeader::encoded_size] = {};
  CHECK_STATUS(RingHeader(16u, 0x1234u, 0x1000u).encode(bytes, sizeof bytes), Status::ok);
  const ByteLayout layout(bytes, sizeof bytes);
  CHECK(layout.at(0u, 4u) == 0x00525641u);
  CHECK(layout.at(4u, 1u) == 16u);
  CHECK(layout.at(5u, 3u) == 0u);
  CHECK(layout.at(8u, 8u) == 0x1234u);
  CHECK(layout.at(16u, 8u) == 0x1000u);
  CHECK(layout.at(24u, 8u) == 0u); }

TEST(ring_header, round_trips_and_reports_its_capacity)
{ std::uint8_t bytes[RingHeader::encoded_size] = {};
  CHECK_STATUS(RingHeader(24u, ~std::uint64_t{0}, 5u).encode(bytes, sizeof bytes), Status::ok);
  RingHeader decoded;
  CHECK_STATUS(decoded.decode(bytes, sizeof bytes), Status::ok);
  CHECK_EQ(decoded.order(), 24u);
  CHECK(decoded.capacity() == (1ull << 24));
  CHECK(decoded.tail() == ~std::uint64_t{0});
  CHECK(decoded.head() == 5u); }

TEST(ring_header_decode, accepts_orders_12_and_24)
{ const std::uint8_t orders[] = {12u, 24u};
  for (const std::uint8_t order : orders) {
      std::uint8_t bytes[RingHeader::encoded_size] = {};
      CHECK_STATUS(RingHeader(order, 0u, 0u).encode(bytes, sizeof bytes), Status::ok);
      RingHeader decoded;
      CHECK_STATUS(decoded.decode(bytes, sizeof bytes), Status::ok);
  } }

TEST(ring_header_decode, rejects_each_malformed_field)
{ struct Case { std::size_t offset; std::uint8_t value; };
  const Case cases[] = {{0u, 0u}, {4u, 11u}, {4u, 25u}, {6u, 1u}, {7u, 0x80u}};
  for (const Case &c : cases) {
      std::uint8_t bytes[RingHeader::encoded_size] = {};
      CHECK_STATUS(RingHeader(16u, 8u, 4u).encode(bytes, sizeof bytes), Status::ok);
      bytes[c.offset] = c.value;
      RingHeader decoded(20u, 1u, 1u);
      CHECK_STATUS(decoded.decode(bytes, sizeof bytes), Status::malformed);
      CHECK_EQ(decoded.order(), 20u);
  } }

TEST(ring_header_decode, reads_past_the_awake_flag_and_the_published_fence)
{ std::uint8_t bytes[RingHeader::encoded_size] = {};
  CHECK_STATUS(RingHeader(16u, 8u, 4u).encode(bytes, sizeof bytes), Status::ok);
  bytes[awake_offset] = 1u;
  const std::uint64_t published = 0x8877665544332211u;
  std::memcpy(bytes + published_fence_offset, &published, sizeof published);
  RingHeader decoded;
  CHECK_STATUS(decoded.decode(bytes, sizeof bytes), Status::ok);
  CHECK_EQ(decoded.order(), 16u);
  CHECK(decoded.tail() == 8u);
  CHECK(decoded.head() == 4u); }

TEST(ring_header_encode, a_short_destination_is_too_large_and_untouched)
{ GuardedBuffer buffer(RingHeader::encoded_size - 1u);
  CHECK_STATUS(RingHeader(12u, 0u, 0u).encode(buffer.data(), buffer.size()), Status::too_large);
  CHECK(buffer.guards_intact()); }

TEST(ring_header_decode, survives_the_hostile_sweep)
{ std::uint8_t bytes[RingHeader::encoded_size] = {};
  CHECK_STATUS(RingHeader(16u, 64u, 32u).encode(bytes, sizeof bytes), Status::ok);
  const av::HostileSweep sweep(rejects, bytes, sizeof bytes);
  sweep.run();
  CHECK(sweep.cases_run() > 256u);
  /* The format constrains the magic, the order and one reserved pair, and
   * leaves the counters to RingProgress and the awake flag and published fence
   * to their readers. So the acceptable flips are the 128 bits of tail and head,
   * the 8 of the awake flag and the 64 of the published fence, plus the four
   * flips of order 16 that land inside [12, 24] - 17, 18, 20 and 24. Every
   * shorter input is short of 32 bytes, all-zero and all-ones carry no magic,
   * and the untouched input is the last acceptance. */
  const std::uint32_t acceptable = 128u + 8u + 64u + 4u + 1u;
  CHECK(sweep.rejections() >= sweep.cases_run() - acceptable); }
