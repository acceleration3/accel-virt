#include "core/support/status.hpp"
#include "harness/framework/byte_layout.hpp"
#include "harness/framework/guarded_buffer.hpp"
#include "harness/framework/hostile_sweep.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/submission/batch_header.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

using av::BatchHeader;
using av::ByteLayout;
using av::GuardedBuffer;
using av::Status;

namespace {

bool rejects(const std::uint8_t *bytes, std::size_t length)
{
    BatchHeader header;
    const Status status = header.decode(bytes, length);
    if (status == Status::ok) {
        std::uint8_t again[BatchHeader::encoded_size] = {};
        if (header.encode(again, sizeof again) != Status::ok || std::memcmp(again, bytes, sizeof again) != 0)
            av::TestRegistry::instance().record_failure(__FILE__, __LINE__, "accepted input did not round trip");
    }
    return status != Status::ok;
}

}  // namespace

TEST(batch_header_encode, places_every_field_at_its_offset)
{ std::uint8_t bytes[BatchHeader::encoded_size] = {};
  CHECK_STATUS(BatchHeader(0x123u, 0x1122334455667788u).encode(bytes, sizeof bytes), Status::ok);
  const ByteLayout layout(bytes, sizeof bytes);
  CHECK(layout.at(0u, 4u) == 0x424B5652u);
  CHECK(layout.at(4u, 4u) == 0x123u);
  CHECK(layout.at(8u, 8u) == 0x1122334455667788u); }

TEST(batch_header, round_trips_through_its_encoding)
{ std::uint8_t bytes[BatchHeader::encoded_size] = {};
  CHECK_STATUS(BatchHeader(0xFFFFFFFFu, ~std::uint64_t{0}).encode(bytes, sizeof bytes), Status::ok);
  BatchHeader decoded;
  CHECK_STATUS(decoded.decode(bytes, sizeof bytes), Status::ok);
  CHECK_EQ(decoded.length(), 0xFFFFFFFFu);
  CHECK(decoded.fence_id() == ~std::uint64_t{0}); }

TEST(batch_header_span, rounds_the_header_and_payload_up_to_eight_bytes)
{ struct Case { std::uint32_t length; std::uint64_t span; };
  const Case cases[] = {{0u, 16u}, {1u, 24u}, {7u, 24u}, {8u, 24u}, {9u, 32u},
                        {0xFFFFFFFFu, 0x100000010ull}};
  for (const Case &c : cases) CHECK(BatchHeader(c.length, 1u).span() == c.span); }

TEST(batch_header_fits, accepts_a_span_up_to_the_capacity)
{ CHECK(BatchHeader(4080u, 1u).fits(4096u));
  CHECK(!BatchHeader(4081u, 1u).fits(4096u));
  CHECK(!BatchHeader(0xFFFFFFFFu, 1u).fits(1ull << 24));
  CHECK(BatchHeader(0u, 1u).fits(16u));
  CHECK(!BatchHeader(0u, 1u).fits(15u)); }

TEST(batch_header_decode, rejects_a_bad_magic_and_fence_zero)
{ struct Case { std::size_t offset; std::uint8_t value; };
  const Case cases[] = {{0u, 0u}, {3u, 0x51u}, {8u, 0u}};
  for (const Case &c : cases) {
      std::uint8_t bytes[BatchHeader::encoded_size] = {};
      CHECK_STATUS(BatchHeader(16u, 1u).encode(bytes, sizeof bytes), Status::ok);
      bytes[c.offset] = c.value;
      BatchHeader decoded(4u, 9u);
      CHECK_STATUS(decoded.decode(bytes, sizeof bytes), Status::malformed);
      CHECK_EQ(decoded.length(), 4u);
      CHECK(decoded.fence_id() == 9u);
  } }

TEST(batch_header_decode, a_short_buffer_is_malformed)
{ std::uint8_t bytes[BatchHeader::encoded_size] = {};
  CHECK_STATUS(BatchHeader(0u, 1u).encode(bytes, sizeof bytes), Status::ok);
  BatchHeader decoded;
  CHECK_STATUS(decoded.decode(bytes, sizeof bytes - 1u), Status::malformed);
  CHECK_STATUS(decoded.decode(nullptr, sizeof bytes), Status::malformed); }

TEST(batch_header_encode, a_short_destination_is_too_large_and_untouched)
{ GuardedBuffer buffer(BatchHeader::encoded_size - 1u);
  CHECK_STATUS(BatchHeader(0u, 1u).encode(buffer.data(), buffer.size()), Status::too_large);
  CHECK(buffer.guards_intact()); }

TEST(batch_header_decode, survives_the_hostile_sweep)
{ std::uint8_t bytes[BatchHeader::encoded_size] = {};
  CHECK_STATUS(BatchHeader(40u, 3u).encode(bytes, sizeof bytes), Status::ok);
  const av::HostileSweep sweep(rejects, bytes, sizeof bytes);
  sweep.run();
  CHECK(sweep.cases_run() > 128u);
  /* The format constrains the magic and forbids fence zero; the payload length
   * is 32 free bits, and the fence's 64 bits are free too, because 3 has two
   * bits set and no single flip can clear both. Every shorter input is short of
   * 16 bytes, all-zero and all-ones carry no magic, and the untouched input is
   * the last acceptance. */
  const std::uint32_t acceptable = 32u + 64u + 1u;
  CHECK(sweep.rejections() >= sweep.cases_run() - acceptable); }
