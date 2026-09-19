/* The body of SHARE_OBJECT, OPEN_SHARED_OBJECT and RELEASE_SHARED_OBJECT as a
 * guest frames it and the device reads it: a length other than 32 bytes is
 * framing, and every field the bytes alone can judge is judged here - a fence
 * carries no length and memory carries whole pages.
 * The wire handle is deliberately not judged: which command may carry one is
 * the command's rule, and the command's test is where that lives. */
#include "core/support/status.hpp"
#include "harness/framework/byte_layout.hpp"
#include "harness/framework/guarded_buffer.hpp"
#include "harness/framework/hostile_sweep.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/submission/shared_object_request.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

using av::ByteLayout;
using av::GuardedBuffer;
using av::SharedObjectKind;
using av::SharedObjectRequest;
using av::Status;

namespace {

constexpr std::uint64_t page = SharedObjectRequest::page_size;

SharedObjectRequest valid()
{
    return SharedObjectRequest(0x0123456789ABCDEFu, SharedObjectKind(SharedObjectKind::memory), 0xFEEDFACEu,
                               8u * page);
}

void encode(const SharedObjectRequest &request, std::uint8_t *bytes)
{
    CHECK_STATUS(request.encode(bytes, SharedObjectRequest::encoded_size), Status::ok);
}

bool rejects(const std::uint8_t *bytes, std::size_t length)
{
    SharedObjectRequest request;
    const Status        status = request.decode(bytes, length);
    if (status == Status::ok) {
        std::uint8_t again[SharedObjectRequest::encoded_size] = {};
        if (request.encode(again, sizeof again) != Status::ok || std::memcmp(again, bytes, sizeof again) != 0)
            av::TestRegistry::instance().record_failure(__FILE__, __LINE__, "accepted input did not round trip");
    }
    return status != Status::ok;
}

}  // namespace

TEST(shared_object_request_encode, lays_out_the_fields_with_the_reserved_word_zero)
{ std::uint8_t bytes[SharedObjectRequest::encoded_size];
  std::memset(bytes, 0xEE, sizeof bytes);
  encode(valid(), bytes);
  const ByteLayout layout(bytes, sizeof bytes);
  CHECK(layout.at(0u, 8u) == 0x0123456789ABCDEFu);
  CHECK(layout.at(8u, 4u) == SharedObjectKind::memory);
  CHECK(layout.at(12u, 4u) == 0u);
  CHECK(layout.at(16u, 8u) == 0xFEEDFACEu);
  CHECK(layout.at(24u, 8u) == 8u * page); }

TEST(shared_object_request_encode, a_short_destination_is_too_large_and_untouched)
{ GuardedBuffer buffer(SharedObjectRequest::encoded_size - 1u);
  CHECK_STATUS(valid().encode(buffer.data(), buffer.size()), Status::too_large);
  CHECK(buffer.guards_intact()); }

TEST(shared_object_request_decode, round_trips_every_field)
{ std::uint8_t bytes[SharedObjectRequest::encoded_size] = {};
  encode(valid(), bytes);
  SharedObjectRequest decoded;
  CHECK_STATUS(decoded.decode(bytes, sizeof bytes), Status::ok);
  CHECK_EQ(decoded.object_id(), 0x0123456789ABCDEFu);
  CHECK_EQ(decoded.kind().value(), SharedObjectKind::memory);
  CHECK_EQ(decoded.wire_handle(), 0xFEEDFACEu);
  CHECK_EQ(decoded.size(), 8u * page); }

TEST(shared_object_request_decode, a_zero_wire_handle_is_what_an_open_carries_and_is_accepted)
{ std::uint8_t bytes[SharedObjectRequest::encoded_size] = {};
  encode(SharedObjectRequest(4u, SharedObjectKind(SharedObjectKind::monitored_fence), 0u, 0u), bytes);
  SharedObjectRequest decoded;
  CHECK_STATUS(decoded.decode(bytes, sizeof bytes), Status::ok);
  CHECK_EQ(decoded.wire_handle(), 0u);
  CHECK_EQ(decoded.size(), 0u);
  CHECK_EQ(decoded.kind().value(), SharedObjectKind::monitored_fence); }

TEST(shared_object_request_decode, any_length_but_its_own_is_framing_and_leaves_the_request_alone)
{ std::uint8_t bytes[SharedObjectRequest::encoded_size + 8u] = {};
  encode(valid(), bytes);
  const std::size_t lengths[] = {0u, 1u, 8u, 12u, 16u, 20u, 24u, 28u, 31u, 33u, 40u};
  for (const std::size_t length : lengths) {
      SharedObjectRequest decoded(9u, SharedObjectKind(SharedObjectKind::native_fence), 3u, 0u);
      CHECK_STATUS(decoded.decode(bytes, length), Status::malformed);
      CHECK_EQ(decoded.object_id(), 9u);
      CHECK_EQ(decoded.wire_handle(), 3u);
  } }

TEST(shared_object_request_decode, a_field_no_request_can_have_is_an_invalid_parameter_left_unread)
{ struct Case { std::size_t offset; std::size_t width; std::uint64_t value; };
  const Case cases[] = {
      {0u, 8u, 0u},                        /* no object */
      {8u, 4u, 0u},                        /* no kind */
      {8u, 4u, 4u},                        /* the kind after the last one */
      {8u, 4u, 0xFFFFFFFFu},
      {12u, 4u, 1u},                       /* reserved */
      {12u, 4u, 0x80000000u},
      {24u, 8u, 0u},                       /* memory with no length */
      {24u, 8u, 1u},                       /* a length that is not whole pages */
      {24u, 8u, page - 1u},
      {24u, 8u, page + 1u},
      {8u, 4u, SharedObjectKind::monitored_fence},  /* a fence carrying a length */
      {8u, 4u, SharedObjectKind::native_fence},
  };
  for (const Case &c : cases) {
      std::uint8_t bytes[SharedObjectRequest::encoded_size] = {};
      encode(valid(), bytes);
      std::memcpy(bytes + c.offset, &c.value, c.width);
      SharedObjectRequest decoded(9u, SharedObjectKind(SharedObjectKind::native_fence), 3u, 0u);
      CHECK_STATUS(decoded.decode(bytes, sizeof bytes), Status::invalid_parameter);
      CHECK_EQ(decoded.object_id(), 9u);
      CHECK_EQ(decoded.kind().value(), SharedObjectKind::native_fence);
  } }

TEST(shared_object_request_decode, the_widest_representable_request_round_trips)
{ std::uint8_t bytes[SharedObjectRequest::encoded_size] = {};
  encode(SharedObjectRequest(0xFFFFFFFFFFFFFFFFu, SharedObjectKind(SharedObjectKind::memory),
                             0xFFFFFFFFFFFFFFFFu, 0xFFFFFFFFFFFFF000u),
         bytes);
  SharedObjectRequest decoded;
  CHECK_STATUS(decoded.decode(bytes, sizeof bytes), Status::ok);
  CHECK_EQ(decoded.object_id(), 0xFFFFFFFFFFFFFFFFu);
  CHECK_EQ(decoded.wire_handle(), 0xFFFFFFFFFFFFFFFFu);
  CHECK_EQ(decoded.size(), 0xFFFFFFFFFFFFF000u); }

TEST(shared_object_request_decode, survives_the_hostile_sweep)
{ std::uint8_t bytes[SharedObjectRequest::encoded_size] = {};
  encode(valid(), bytes);
  const av::HostileSweep sweep(rejects, bytes, sizeof bytes);
  sweep.run();
  CHECK(sweep.cases_run() > 256u); }
