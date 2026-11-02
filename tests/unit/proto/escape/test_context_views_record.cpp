#include "core/support/status.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/escape/context_views_record.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

using av::ContextViewsRecord;
using av::Status;

namespace {

constexpr std::uint64_t ring = 0x00007FF000010000ull;
constexpr std::uint64_t reply = 0x00007FF000200000ull;
constexpr std::uint64_t fence = 0x00007FF000300000ull;

class Bytes {
public:
    explicit Bytes(const ContextViewsRecord &record) { status_ = record.encode(bytes_, sizeof bytes_); }
    std::uint8_t *data() { return bytes_; }
    Status        status() const { return status_; }
    void          put(std::size_t at, std::uint8_t width, std::uint64_t value)
    { std::memcpy(bytes_ + at, &value, width); }

private:
    std::uint8_t bytes_[ContextViewsRecord::encoded_size] = {};
    Status       status_ = Status::ok;
};

}

TEST(context_views_record, a_request_round_trips_and_names_no_address)
{ Bytes bytes{ContextViewsRecord()};
  CHECK_STATUS(bytes.status(), Status::ok);
  const std::uint8_t expected[8] = {'A', 'V', 'V', 0, 2u, 0u, 0u, 0u};
  CHECK(std::memcmp(bytes.data(), expected, sizeof expected) == 0);
  for (std::size_t at = 8u; at < ContextViewsRecord::encoded_size; at++) CHECK_EQ(bytes.data()[at], 0u);
  ContextViewsRecord decoded{ContextViewsRecord::without_fence};
  CHECK_STATUS(decoded.decode(bytes.data(), ContextViewsRecord::encoded_size), Status::ok);
  CHECK_EQ(decoded.version(), ContextViewsRecord::with_fence);
  CHECK_EQ(decoded.ring(), 0u);
  CHECK_EQ(decoded.reply(), 0u);
  CHECK_EQ(decoded.fence(), 0u);
  Bytes first{ContextViewsRecord(ContextViewsRecord::without_fence)};
  CHECK_EQ(first.data()[4], 1u);
  CHECK_STATUS(decoded.decode(first.data(), ContextViewsRecord::encoded_size), Status::ok);
  CHECK_EQ(decoded.version(), ContextViewsRecord::without_fence); }

TEST(context_views_record, an_answer_carries_every_address_and_is_no_request)
{ Bytes bytes(ContextViewsRecord().answered(ring, reply, fence));
  std::uint64_t at_ring = 0u, at_reply = 0u, at_fence = 0u;
  std::memcpy(&at_ring, bytes.data() + 8u, sizeof at_ring);
  std::memcpy(&at_reply, bytes.data() + 16u, sizeof at_reply);
  std::memcpy(&at_fence, bytes.data() + 24u, sizeof at_fence);
  CHECK_EQ(at_ring, ring);
  CHECK_EQ(at_reply, reply);
  CHECK_EQ(at_fence, fence);
  ContextViewsRecord decoded;
  CHECK_STATUS(decoded.decode(bytes.data(), ContextViewsRecord::encoded_size), Status::malformed);
  Bytes half(ContextViewsRecord().answered(0u, reply, 0u));
  CHECK_STATUS(decoded.decode(half.data(), ContextViewsRecord::encoded_size), Status::malformed);
  Bytes fence_only(ContextViewsRecord().answered(0u, 0u, fence));
  CHECK_STATUS(decoded.decode(fence_only.data(), ContextViewsRecord::encoded_size), Status::malformed); }

TEST(context_views_record, a_version_1_answer_carries_no_fence)
{ const ContextViewsRecord request(ContextViewsRecord::without_fence);
  Bytes bytes(request.answered(ring, reply, fence));
  for (std::size_t at = 24u; at < ContextViewsRecord::encoded_size; at++) CHECK_EQ(bytes.data()[at], 0u);
  ContextViewsRecord accepted(ContextViewsRecord::without_fence);
  CHECK_STATUS(accepted.accept_answer(bytes.data(), ContextViewsRecord::encoded_size), Status::ok);
  CHECK_EQ(accepted.ring(), ring);
  CHECK_EQ(accepted.fence(), 0u); }

TEST(context_views_record, every_field_is_judged)
{ struct Case { const char *what; std::uint16_t version; std::size_t at; std::uint8_t width; std::uint64_t value; };
  const Case cases[] = {
      {"magic", 2u, 0u, 4u, 0x00575641u},
      {"version 0", 2u, 4u, 2u, 0u},
      {"version 3", 2u, 4u, 2u, 3u},
      {"reserved half", 2u, 6u, 2u, 1u},
      {"ring already", 2u, 8u, 8u, ring},
      {"reply already", 2u, 16u, 8u, reply},
      {"fence already", 2u, 24u, 8u, fence},
      {"fence high bit", 2u, 24u, 8u, 0x8000000000000000ull},
      {"version 1 reserved tail", 1u, 24u, 8u, 1u},
      {"version 1 reserved tail high bit", 1u, 24u, 8u, 0x8000000000000000ull},
  };
  for (const Case &one : cases) {
      Bytes bytes{ContextViewsRecord(one.version)};
      bytes.put(one.at, one.width, one.value);
      ContextViewsRecord decoded = ContextViewsRecord().answered(1u, 2u, 3u);
      CHECK_STATUS(decoded.decode(bytes.data(), ContextViewsRecord::encoded_size), Status::malformed);
      CHECK_EQ(decoded.ring(), 1u);
      CHECK_EQ(decoded.reply(), 2u);
      CHECK_EQ(decoded.fence(), 3u);
  } }

TEST(context_views_record, a_short_or_long_buffer_is_refused_and_encode_refuses_one)
{ Bytes bytes{ContextViewsRecord()};
  for (std::size_t length = 0u; length < ContextViewsRecord::encoded_size; length++) {
      ContextViewsRecord decoded;
      CHECK(decoded.decode(bytes.data(), length) != Status::ok);
      CHECK(decoded.accept_answer(bytes.data(), length) != Status::ok);
  }
  ContextViewsRecord decoded;
  CHECK(decoded.decode(bytes.data(), ContextViewsRecord::encoded_size + 1u) != Status::ok);
  CHECK(decoded.decode(nullptr, ContextViewsRecord::encoded_size) != Status::ok);
  std::uint8_t small[ContextViewsRecord::encoded_size - 1u] = {};
  CHECK(ContextViewsRecord().encode(small, sizeof small) != Status::ok); }

TEST(context_views_record, a_byte_flip_sweep_of_a_request_is_always_refused)
{ for (std::size_t byte = 0u; byte < ContextViewsRecord::encoded_size; byte++)
      for (unsigned bit = 0u; bit < 8u; bit++) {
          Bytes bytes{ContextViewsRecord()};
          bytes.data()[byte] = static_cast<std::uint8_t>(bytes.data()[byte] ^ (1u << bit));
          ContextViewsRecord decoded;
          CHECK_STATUS(decoded.decode(bytes.data(), ContextViewsRecord::encoded_size), Status::malformed);
      } }

TEST(context_views_record, accept_answer_takes_the_addresses_the_kernel_wrote)
{ ContextViewsRecord request;
  Bytes bytes(request.answered(ring, reply, fence));
  CHECK_STATUS(request.accept_answer(bytes.data(), ContextViewsRecord::encoded_size), Status::ok);
  CHECK_EQ(request.ring(), ring);
  CHECK_EQ(request.reply(), reply);
  CHECK_EQ(request.fence(), fence);
  ContextViewsRecord edges;
  Bytes extremes(edges.answered(1u, UINT64_MAX, 2u));
  CHECK_STATUS(edges.accept_answer(extremes.data(), ContextViewsRecord::encoded_size), Status::ok);
  CHECK_EQ(edges.reply(), UINT64_MAX); }

TEST(context_views_record, accept_answer_refuses_what_no_driver_answers)
{ struct Case { const char *what; std::uint64_t ring; std::uint64_t reply; std::uint64_t fence; };
  const Case cases[] = {
      {"no ring", 0u, reply, fence},
      {"no reply", ring, 0u, fence},
      {"no fence", ring, reply, 0u},
      {"one place for ring and reply", ring, ring, fence},
      {"fence on the ring", ring, reply, ring},
      {"fence on the reply", ring, reply, reply},
      {"no answer at all", 0u, 0u, 0u},
  };
  for (const Case &one : cases) {
      ContextViewsRecord request;
      Bytes              bytes(request.answered(one.ring, one.reply, one.fence));
      CHECK_STATUS(request.accept_answer(bytes.data(), ContextViewsRecord::encoded_size), Status::malformed);
      CHECK_EQ(request.ring(), 0u);
      CHECK_EQ(request.reply(), 0u);
      CHECK_EQ(request.fence(), 0u);
  }
  ContextViewsRecord request;
  Bytes              wrong(request.answered(ring, reply, fence));
  wrong.put(0u, 4u, 0x00575641u);
  CHECK_STATUS(request.accept_answer(wrong.data(), ContextViewsRecord::encoded_size), Status::malformed);
  Bytes older(ContextViewsRecord(ContextViewsRecord::without_fence).answered(ring, reply, 0u));
  CHECK_STATUS(request.accept_answer(older.data(), ContextViewsRecord::encoded_size), Status::malformed);
  ContextViewsRecord first(ContextViewsRecord::without_fence);
  Bytes newer(ContextViewsRecord().answered(ring, reply, fence));
  CHECK_STATUS(first.accept_answer(newer.data(), ContextViewsRecord::encoded_size), Status::malformed);
  Bytes filled(ContextViewsRecord(ContextViewsRecord::without_fence).answered(ring, reply, 0u));
  filled.put(24u, 8u, 1u);
  CHECK_STATUS(first.accept_answer(filled.data(), ContextViewsRecord::encoded_size), Status::malformed); }
