/* The escape record a process sends to learn where the driver mapped its av
 * context's ring and reply slot (docs/protocol.md, "Context memory"). The
 * kernel decodes it from memory the process wrote, so every field is judged as
 * hostile input, and the kernel's answer is encoded back into the same 32
 * bytes. */
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

}  // namespace

/* A request carries only its magic and version; everything else is the
 * kernel's to write. */
TEST(context_views_record, a_request_round_trips_and_names_no_address)
{ Bytes bytes{ContextViewsRecord()};
  CHECK_STATUS(bytes.status(), Status::ok);
  const std::uint8_t expected[8] = {'A', 'V', 'V', 0, 1u, 0u, 0u, 0u};
  CHECK(std::memcmp(bytes.data(), expected, sizeof expected) == 0);
  for (std::size_t at = 8u; at < ContextViewsRecord::encoded_size; at++) CHECK_EQ(bytes.data()[at], 0u);
  ContextViewsRecord decoded;
  CHECK_STATUS(decoded.decode(bytes.data(), ContextViewsRecord::encoded_size), Status::ok);
  CHECK_EQ(decoded.ring(), 0u);
  CHECK_EQ(decoded.reply(), 0u); }

/* The answer lands where the contract puts it, and is no request: a process
 * cannot pre-fill the addresses. */
TEST(context_views_record, an_answer_carries_both_addresses_and_is_no_request)
{ Bytes bytes(ContextViewsRecord().answered(ring, reply));
  std::uint64_t at_ring = 0u, at_reply = 0u;
  std::memcpy(&at_ring, bytes.data() + 8u, sizeof at_ring);
  std::memcpy(&at_reply, bytes.data() + 16u, sizeof at_reply);
  CHECK_EQ(at_ring, ring);
  CHECK_EQ(at_reply, reply);
  ContextViewsRecord decoded;
  CHECK_STATUS(decoded.decode(bytes.data(), ContextViewsRecord::encoded_size), Status::malformed);
  Bytes half(ContextViewsRecord().answered(0u, reply));
  CHECK_STATUS(decoded.decode(half.data(), ContextViewsRecord::encoded_size), Status::malformed); }

TEST(context_views_record, every_field_is_judged)
{ struct Case { const char *what; std::size_t at; std::uint8_t width; std::uint64_t value; };
  const Case cases[] = {
      {"magic", 0u, 4u, 0x00575641u},   /* AVW, the window's */
      {"version 0", 4u, 2u, 0u},
      {"version 2", 4u, 2u, 2u},
      {"reserved half", 6u, 2u, 1u},
      {"ring already", 8u, 8u, ring},
      {"reply already", 16u, 8u, reply},
      {"reserved tail", 24u, 8u, 1u},
      {"reserved tail high bit", 24u, 8u, 0x8000000000000000ull},
  };
  for (const Case &one : cases) {
      Bytes bytes{ContextViewsRecord()};
      bytes.put(one.at, one.width, one.value);
      ContextViewsRecord decoded = ContextViewsRecord().answered(1u, 2u);
      CHECK_STATUS(decoded.decode(bytes.data(), ContextViewsRecord::encoded_size), Status::malformed);
      /* A refusal leaves the record as it was. */
      CHECK_EQ(decoded.ring(), 1u);
      CHECK_EQ(decoded.reply(), 2u);
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

/* One bit of a request flipped anywhere is either still the request itself or
 * refused: a request has no field a bit can move and stay one. */
TEST(context_views_record, a_byte_flip_sweep_of_a_request_is_always_refused)
{ for (std::size_t byte = 0u; byte < ContextViewsRecord::encoded_size; byte++)
      for (unsigned bit = 0u; bit < 8u; bit++) {
          Bytes bytes{ContextViewsRecord()};
          bytes.data()[byte] = static_cast<std::uint8_t>(bytes.data()[byte] ^ (1u << bit));
          ContextViewsRecord decoded;
          CHECK_STATUS(decoded.decode(bytes.data(), ContextViewsRecord::encoded_size), Status::malformed);
      } }

/* The process reads the kernel's answer back from the bytes it sent. */
TEST(context_views_record, accept_answer_takes_the_addresses_the_kernel_wrote)
{ ContextViewsRecord request;
  Bytes bytes(request.answered(ring, reply));
  CHECK_STATUS(request.accept_answer(bytes.data(), ContextViewsRecord::encoded_size), Status::ok);
  CHECK_EQ(request.ring(), ring);
  CHECK_EQ(request.reply(), reply);
  /* Any two distinct addresses, the lowest and highest included. */
  ContextViewsRecord edges;
  Bytes extremes(edges.answered(1u, UINT64_MAX));
  CHECK_STATUS(edges.accept_answer(extremes.data(), ContextViewsRecord::encoded_size), Status::ok);
  CHECK_EQ(edges.reply(), UINT64_MAX); }

/* No driver answers with an address missing, or with the ring and reply slot
 * at one place; refused, and the record left as it was. */
TEST(context_views_record, accept_answer_refuses_what_no_driver_answers)
{ struct Case { const char *what; std::uint64_t ring; std::uint64_t reply; std::size_t at; std::uint64_t value; };
  const Case cases[] = {
      {"no ring", 0u, reply, 0u, 0u},
      {"no reply", ring, 0u, 0u, 0u},
      {"one place for both", ring, ring, 0u, 0u},
      {"no answer at all", 0u, 0u, 0u, 0u},
      {"reserved tail", ring, reply, 24u, 1u},
  };
  for (const Case &one : cases) {
      ContextViewsRecord request;
      Bytes              bytes(request.answered(one.ring, one.reply));
      if (one.at != 0u) bytes.put(one.at, 8u, one.value);
      CHECK_STATUS(request.accept_answer(bytes.data(), ContextViewsRecord::encoded_size), Status::malformed);
      CHECK_EQ(request.ring(), 0u);
      CHECK_EQ(request.reply(), 0u);
  }
  ContextViewsRecord request;
  Bytes              wrong(request.answered(ring, reply));
  wrong.put(0u, 4u, 0x00575641u);
  CHECK_STATUS(request.accept_answer(wrong.data(), ContextViewsRecord::encoded_size), Status::malformed); }
