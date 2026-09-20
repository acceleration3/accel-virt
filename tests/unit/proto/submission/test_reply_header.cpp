#include "core/support/status.hpp"
#include "harness/framework/byte_layout.hpp"
#include "harness/framework/guarded_buffer.hpp"
#include "harness/framework/hostile_sweep.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/submission/reply_header.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

using av::ByteLayout;
using av::GuardedBuffer;
using av::ReplyHeader;
using av::Status;

namespace {

bool rejects(const std::uint8_t *bytes, std::size_t length)
{
    ReplyHeader header;
    const Status status = header.decode(bytes, length);
    if (status == Status::ok) {
        std::uint8_t again[ReplyHeader::encoded_size] = {};
        if (header.encode(again, sizeof again) != Status::ok || std::memcmp(again, bytes, sizeof again) != 0)
            av::TestRegistry::instance().record_failure(__FILE__, __LINE__, "accepted input did not round trip");
    }
    return status != Status::ok;
}

}  // namespace

TEST(reply_header_encode, places_every_field_at_its_offset)
{ std::uint8_t bytes[ReplyHeader::encoded_size] = {};
  CHECK_STATUS(ReplyHeader(0x1122334455667788u, 300u).encode(bytes, sizeof bytes), Status::ok);
  const ByteLayout layout(bytes, sizeof bytes);
  CHECK(layout.at(0u, 4u) == 0x594B5652u);
  CHECK(layout.at(4u, 4u) == ReplyHeader::status_ok);
  CHECK(layout.at(8u, 8u) == 0x1122334455667788u);
  CHECK(layout.at(16u, 4u) == 300u);
  CHECK(layout.at(20u, 4u) == 300u);
  CHECK(layout.at(24u, 8u) == 0u); }

TEST(reply_header, an_answer_that_fits_or_one_too_large_round_trips)
{ std::uint8_t bytes[ReplyHeader::encoded_size] = {};
  CHECK_STATUS(ReplyHeader(4u, 12u).encode(bytes, sizeof bytes), Status::ok);
  ReplyHeader decoded;
  CHECK_STATUS(decoded.decode(bytes, sizeof bytes), Status::ok);
  CHECK(!decoded.too_large());
  CHECK_EQ(decoded.fence_id(), 4u);
  CHECK_EQ(decoded.size(), 12u);
  CHECK_EQ(decoded.required(), 12u);
  CHECK_STATUS(ReplyHeader(5u, 12u).within(12u).encode(bytes, sizeof bytes), Status::ok);
  CHECK_STATUS(decoded.decode(bytes, sizeof bytes), Status::ok);
  CHECK(!decoded.too_large());
  CHECK_STATUS(ReplyHeader(5u, 70000u).within(69999u).encode(bytes, sizeof bytes), Status::ok);
  CHECK_STATUS(decoded.decode(bytes, sizeof bytes), Status::ok);
  CHECK(decoded.too_large());
  CHECK_EQ(decoded.size(), 0u);
  CHECK_EQ(decoded.required(), 70000u);
  CHECK_EQ(ReplyHeader().fence_id(), 0u); }

TEST(reply_header_decode, rejects_each_inconsistent_field)
{ struct Case { std::size_t offset; std::uint8_t value; };
  const Case cases[] = {
      {0u, 0u},     /* magic */
      {4u, 2u},     /* unknown status */
      {8u, 0u},     /* fence 0 (low byte of 3) */
      {16u, 11u},   /* ok, but size and required disagree */
      {26u, 1u},    /* reserved */
  };
  for (const Case &c : cases) {
      std::uint8_t bytes[ReplyHeader::encoded_size] = {};
      CHECK_STATUS(ReplyHeader(3u, 12u).encode(bytes, sizeof bytes), Status::ok);
      bytes[c.offset] = c.value;
      ReplyHeader decoded(9u, 1u);
      CHECK_STATUS(decoded.decode(bytes, sizeof bytes), Status::malformed);
      CHECK_EQ(decoded.fence_id(), 9u);
  }
  std::uint8_t too_large[ReplyHeader::encoded_size] = {};
  CHECK_STATUS(ReplyHeader(3u, 12u).within(4u).encode(too_large, sizeof too_large), Status::ok);
  too_large[16] = 1u;   /* too large, yet bytes follow */
  ReplyHeader decoded;
  CHECK_STATUS(decoded.decode(too_large, sizeof too_large), Status::malformed);
  too_large[16] = 0u;
  std::memset(too_large + 20, 0, 4u);   /* too large, needing nothing */
  CHECK_STATUS(decoded.decode(too_large, sizeof too_large), Status::malformed); }

TEST(reply_header_encode, a_short_destination_is_too_large_and_untouched)
{ GuardedBuffer buffer(ReplyHeader::encoded_size - 1u);
  CHECK_STATUS(ReplyHeader(1u, 0u).encode(buffer.data(), buffer.size()), Status::too_large);
  CHECK(buffer.guards_intact()); }

TEST(reply_header_decode, survives_the_hostile_sweep)
{ std::uint8_t bytes[ReplyHeader::encoded_size] = {};
  CHECK_STATUS(ReplyHeader(77u, 900u).encode(bytes, sizeof bytes), Status::ok);
  const av::HostileSweep sweep(rejects, bytes, sizeof bytes);
  sweep.run();
  CHECK(sweep.cases_run() > 256u);
  /* Only the fence is free: 77 has four bits set, so no single flip can zero
   * it, and all 64 are acceptable. Every other field is pinned by the answer's
   * own consistency - a flipped status leaves an answer of 900 bytes claiming
   * to be too large, and a flipped size or required makes the two disagree.
   * Every shorter input is short of 32 bytes, all-zero and all-ones carry no
   * magic, and the untouched input is the last acceptance. */
  const std::uint32_t acceptable = 64u + 1u;
  CHECK(sweep.rejections() >= sweep.cases_run() - acceptable); }
