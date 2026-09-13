/* The private-data record a process puts on D3DKMTCreateContextVirtual to ask
 * for an rvk context. The kernel side decodes it from memory a process wrote,
 * so every field is judged as hostile input, the way ContextSetup judges the
 * wire (feature 043). */
#include "core/support/status.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/context/rvk_context_record.hpp"
#include "proto/context/sub_api.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <initializer_list>

using av::RvkContextRecord;
using av::Status;
using av::SubApi;

namespace {

/* Ring 2, fence 3, reply 4 - three distinct non-zero allocation indices, an
 * order the wire allows, one sub-API. */
RvkContextRecord sample()
{
    return RvkContextRecord(SubApi::vulkan, 16u, 2u, 3u, 4u);
}

class Encoded {
public:
    explicit Encoded(const RvkContextRecord &record) { status_ = record.encode(bytes_, sizeof bytes_); }
    std::uint8_t *bytes() { return bytes_; }
    Status        status() const { return status_; }

private:
    std::uint8_t bytes_[RvkContextRecord::encoded_size] = {};
    Status       status_ = Status::ok;
};

bool rejects(const std::uint8_t *bytes, std::size_t length)
{
    RvkContextRecord record;
    return record.decode(bytes, length) != Status::ok;
}

/* Encode a record the constructor accepts but the wire rules forbid, then check
 * decode refuses it - the caller side does not validate, the kernel side does. */
bool rejects_encoded(const RvkContextRecord &record)
{
    std::uint8_t bytes[RvkContextRecord::encoded_size] = {};
    if (record.encode(bytes, sizeof bytes) != Status::ok) return true;
    return rejects(bytes, sizeof bytes);
}

}  // namespace

TEST(rvk_context_record, a_well_formed_record_round_trips)
{ Encoded encoded(sample());
  CHECK_STATUS(encoded.status(), Status::ok);
  RvkContextRecord decoded;
  CHECK_STATUS(decoded.decode(encoded.bytes(), RvkContextRecord::encoded_size), Status::ok);
  CHECK_EQ(decoded.sub_api(), SubApi::vulkan);
  CHECK_EQ(decoded.ring_order(), std::uint8_t{16u});
  CHECK_EQ(decoded.ring(), 2u);
  CHECK_EQ(decoded.fence(), 3u);
  CHECK_EQ(decoded.reply(), 4u);
  std::uint8_t again[RvkContextRecord::encoded_size] = {};
  CHECK_STATUS(decoded.encode(again, sizeof again), Status::ok);
  CHECK(std::memcmp(again, encoded.bytes(), sizeof again) == 0); }

/* A reply of zero is a context that reads no answer back, which is legal. */
TEST(rvk_context_record, a_reply_of_zero_is_no_reply_slot)
{ RvkContextRecord record(SubApi::opencl, 12u, 2u, 3u, 0u);
  Encoded encoded(record);
  CHECK_STATUS(encoded.status(), Status::ok);
  RvkContextRecord decoded;
  CHECK_STATUS(decoded.decode(encoded.bytes(), RvkContextRecord::encoded_size), Status::ok);
  CHECK_EQ(decoded.reply(), 0u); }

TEST(rvk_context_record, a_short_record_is_refused)
{ Encoded encoded(sample());
  for (std::size_t length = 0u; length < RvkContextRecord::encoded_size; length++)
      CHECK(rejects(encoded.bytes(), length)); }

TEST(rvk_context_record, a_bad_magic_is_refused)
{ Encoded encoded(sample());
  encoded.bytes()[0] ^= 0xFFu;
  CHECK(rejects(encoded.bytes(), RvkContextRecord::encoded_size)); }

/* Exactly one sub-API bit; zero, two, or an undefined value is refused. */
TEST(rvk_context_record, a_sub_api_that_is_not_one_known_bit_is_refused)
{ for (std::uint32_t api : {0u, 3u, 8u, 7u}) {
      std::uint8_t bytes[RvkContextRecord::encoded_size] = {};
      CHECK_STATUS(sample().encode(bytes, sizeof bytes), Status::ok);
      std::memcpy(bytes + 8u, &api, sizeof api);
      CHECK(rejects(bytes, sizeof bytes));
  } }

/* The wire's ring order range, 12..24. */
TEST(rvk_context_record, an_order_outside_the_wire_range_is_refused)
{ for (std::uint8_t order : {std::uint8_t{0u}, std::uint8_t{11u}, std::uint8_t{25u}, std::uint8_t{255u}}) {
      std::uint8_t bytes[RvkContextRecord::encoded_size] = {};
      CHECK_STATUS(RvkContextRecord(SubApi::vulkan, 16u, 2u, 3u, 4u).encode(bytes, sizeof bytes),
                   Status::ok);
      bytes[8] = order;
      CHECK(rejects(bytes, sizeof bytes));
  } }

/* Ring and fence must be non-zero and distinct; a reply that names either is
 * refused, because one allocation cannot be two roles. */
TEST(rvk_context_record, colliding_or_zero_allocation_ids_are_refused)
{ CHECK(rejects_encoded(RvkContextRecord(SubApi::vulkan, 16u, 0u, 3u, 4u)));  /* ring zero */
  CHECK(rejects_encoded(RvkContextRecord(SubApi::vulkan, 16u, 2u, 0u, 4u)));  /* fence zero */
  CHECK(rejects_encoded(RvkContextRecord(SubApi::vulkan, 16u, 2u, 2u, 4u)));  /* ring == fence */
  CHECK(rejects_encoded(RvkContextRecord(SubApi::vulkan, 16u, 2u, 3u, 2u)));  /* reply == ring */
  CHECK(rejects_encoded(RvkContextRecord(SubApi::vulkan, 16u, 2u, 3u, 3u))); }/* reply == fence */

TEST(rvk_context_record, a_reserved_byte_that_is_not_zero_is_refused)
{ std::uint8_t bytes[RvkContextRecord::encoded_size] = {};
  CHECK_STATUS(sample().encode(bytes, sizeof bytes), Status::ok);
  bytes[9] = 1u;
  CHECK(rejects(bytes, sizeof bytes)); }
