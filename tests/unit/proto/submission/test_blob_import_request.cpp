/* IMPORT_BLOB's body as the ICD writes it and the device reads it: a length
 * other than 32 bytes is framing, and every field the bytes alone can judge is
 * judged here, so the device never imports a range it cannot express. */
#include "core/support/status.hpp"
#include "harness/framework/byte_layout.hpp"
#include "harness/framework/guarded_buffer.hpp"
#include "harness/framework/hostile_sweep.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/submission/blob_import_request.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

using av::BlobImportRequest;
using av::ByteLayout;
using av::GuardedBuffer;
using av::Status;

namespace {

constexpr std::uint64_t page = BlobImportRequest::page_size;

bool rejects(const std::uint8_t *bytes, std::size_t length)
{
    BlobImportRequest request;
    const Status      status = request.decode(bytes, length);
    if (status == Status::ok) {
        std::uint8_t again[BlobImportRequest::encoded_size] = {};
        if (request.encode(again, sizeof again) != Status::ok || std::memcmp(again, bytes, sizeof again) != 0)
            av::TestRegistry::instance().record_failure(__FILE__, __LINE__, "accepted input did not round trip");
    }
    return status != Status::ok;
}

void encode(const BlobImportRequest &request, std::uint8_t *bytes)
{
    CHECK_STATUS(request.encode(bytes, BlobImportRequest::encoded_size), Status::ok);
}

}  // namespace

TEST(blob_import_request_encode, lays_out_the_fields_with_both_reserved_words_zero)
{ std::uint8_t bytes[BlobImportRequest::encoded_size];
  std::memset(bytes, 0xEE, sizeof bytes);
  encode(BlobImportRequest(0x00100146u, 3u * page, 0x4000000u, 7u), bytes);
  const ByteLayout layout(bytes, sizeof bytes);
  CHECK(layout.at(0u, 4u) == 0x00100146u);
  CHECK(layout.at(4u, 4u) == 0u);
  CHECK(layout.at(8u, 8u) == 3u * page);
  CHECK(layout.at(16u, 8u) == 0x4000000u);
  CHECK(layout.at(24u, 4u) == 7u);
  CHECK(layout.at(28u, 4u) == 0u); }

TEST(blob_import_request_encode, a_short_destination_is_too_large_and_untouched)
{ GuardedBuffer buffer(BlobImportRequest::encoded_size - 1u);
  CHECK_STATUS(BlobImportRequest(1u, 0u, page, 0u).encode(buffer.data(), buffer.size()), Status::too_large);
  CHECK(buffer.guards_intact()); }

TEST(blob_import_request_decode, round_trips_the_highest_range_whose_end_is_representable)
{ std::uint8_t bytes[BlobImportRequest::encoded_size] = {};
  const std::uint64_t offset = 0xFFFFFFFFFFFFF000u - 15u * page;
  encode(BlobImportRequest(0xFFFFFFFFu, offset, 15u * page, 0xFFFFFFFFu), bytes);
  BlobImportRequest decoded;
  CHECK_STATUS(decoded.decode(bytes, sizeof bytes), Status::ok);
  CHECK_EQ(decoded.resource_id(), 0xFFFFFFFFu);
  CHECK_EQ(decoded.offset(), offset);
  CHECK_EQ(decoded.size(), 15u * page);
  CHECK_EQ(decoded.memory_type(), 0xFFFFFFFFu); }

TEST(blob_import_request_decode, any_length_but_its_own_is_framing)
{ std::uint8_t bytes[BlobImportRequest::encoded_size + 8u] = {};
  encode(BlobImportRequest(5u, 0u, page, 1u), bytes);
  const std::size_t lengths[] = {0u, 1u, 4u, 8u, 16u, 24u, 28u, 31u, 33u, 40u};
  for (const std::size_t length : lengths) {
      BlobImportRequest decoded(9u, page, page, 2u);
      CHECK_STATUS(decoded.decode(bytes, length), Status::malformed);
      CHECK_EQ(decoded.resource_id(), 9u);
  } }

TEST(blob_import_request_decode, a_field_no_import_can_have_is_an_invalid_parameter_left_unread)
{ struct Case { std::size_t offset; std::size_t width; std::uint64_t value; };
  const Case cases[] = {
      {4u, 4u, 1u},                        /* reserved */
      {4u, 4u, 0x80000000u},
      {28u, 4u, 1u},                       /* trailing reserved */
      {8u, 8u, 1u},                        /* offset one byte past a page */
      {8u, 8u, page - 1u},
      {16u, 8u, 0u},                       /* no size */
      {16u, 8u, page + 1u},                /* size not whole pages */
      {16u, 8u, 1u},
      {8u, 8u, 0xFFFFFFFFFFFFF000u},       /* a page at the top ending past 2^64 */
  };
  for (const Case &c : cases) {
      std::uint8_t bytes[BlobImportRequest::encoded_size] = {};
      encode(BlobImportRequest(5u, 0u, 2u * page, 1u), bytes);
      std::memcpy(bytes + c.offset, &c.value, c.width);
      BlobImportRequest decoded(9u, page, page, 2u);
      CHECK_STATUS(decoded.decode(bytes, sizeof bytes), Status::invalid_parameter);
      CHECK_EQ(decoded.resource_id(), 9u);
      CHECK_EQ(decoded.size(), page);
  }
  std::uint8_t last[BlobImportRequest::encoded_size] = {};
  encode(BlobImportRequest(5u, 0xFFFFFFFFFFFFF000u, page, 1u), last);
  BlobImportRequest decoded;
  CHECK_STATUS(decoded.decode(last, sizeof last), Status::invalid_parameter); }

TEST(blob_import_request_decode, survives_the_hostile_sweep)
{ std::uint8_t bytes[BlobImportRequest::encoded_size] = {};
  encode(BlobImportRequest(0x00100146u, page, 16u * page, 2u), bytes);
  const av::HostileSweep sweep(rejects, bytes, sizeof bytes);
  sweep.run();
  CHECK(sweep.cases_run() > 256u); }
