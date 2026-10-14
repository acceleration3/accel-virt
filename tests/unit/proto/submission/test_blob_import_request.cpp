/* IMPORT_BLOB's body as a guest driver writes it and the device reads it: a
 * length other than 8 bytes is framing, and a resource of 0 or a reserved
 * word that is not zero is refused here, so the device only ever looks up a
 * resource a guest could have opened. */
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

TEST(blob_import_request_encode, lays_out_the_resource_and_a_zero_reserved_word)
{ std::uint8_t bytes[BlobImportRequest::encoded_size];
  std::memset(bytes, 0xEE, sizeof bytes);
  encode(BlobImportRequest(0x01000146u), bytes);
  const ByteLayout layout(bytes, sizeof bytes);
  CHECK(layout.at(0u, 4u) == 0x01000146u);
  CHECK(layout.at(4u, 4u) == 0u); }

TEST(blob_import_request_encode, a_short_destination_is_too_large_and_untouched)
{ GuardedBuffer buffer(BlobImportRequest::encoded_size - 1u);
  CHECK_STATUS(BlobImportRequest(1u).encode(buffer.data(), buffer.size()), Status::too_large);
  CHECK(buffer.guards_intact()); }

TEST(blob_import_request_decode, round_trips_every_resource_but_none)
{ const std::uint32_t resources[] = {1u, 0x01000000u, 0xFFFFFFFFu};
  for (const std::uint32_t resource : resources) {
      std::uint8_t bytes[BlobImportRequest::encoded_size] = {};
      encode(BlobImportRequest(resource), bytes);
      BlobImportRequest decoded;
      CHECK_STATUS(decoded.decode(bytes, sizeof bytes), Status::ok);
      CHECK_EQ(decoded.resource_id(), resource);
  } }

TEST(blob_import_request_decode, any_length_but_its_own_is_framing)
{ std::uint8_t bytes[BlobImportRequest::encoded_size + 8u] = {};
  encode(BlobImportRequest(5u), bytes);
  const std::size_t lengths[] = {0u, 1u, 4u, 7u, 9u, 16u};
  for (const std::size_t length : lengths) {
      BlobImportRequest decoded(9u);
      CHECK_STATUS(decoded.decode(bytes, length), Status::malformed);
      CHECK_EQ(decoded.resource_id(), 9u);
  } }

TEST(blob_import_request_decode, no_resource_or_a_reserved_word_is_an_invalid_parameter_left_unread)
{ struct Case { std::size_t offset; std::uint32_t value; };
  const Case cases[] = {{0u, 0u}, {4u, 1u}, {4u, 0x80000000u}};
  for (const Case &c : cases) {
      std::uint8_t bytes[BlobImportRequest::encoded_size] = {};
      encode(BlobImportRequest(5u), bytes);
      std::memcpy(bytes + c.offset, &c.value, sizeof c.value);
      BlobImportRequest decoded(9u);
      CHECK_STATUS(decoded.decode(bytes, sizeof bytes), Status::invalid_parameter);
      CHECK_EQ(decoded.resource_id(), 9u);
  } }

TEST(blob_import_request_decode, survives_the_hostile_sweep)
{ std::uint8_t bytes[BlobImportRequest::encoded_size] = {};
  encode(BlobImportRequest(0x01000146u), bytes);
  const av::HostileSweep sweep(rejects, bytes, sizeof bytes);
  sweep.run();
  CHECK(sweep.cases_run() > 64u);
  /* Any resource but 0 is acceptable and the reserved word must be zero: of
   * the resource's 32 bits every single flip leaves it non-zero, and the
   * untouched input is the last acceptance. */
  const std::uint32_t acceptable = 32u + 1u;
  CHECK(sweep.rejections() >= sweep.cases_run() - acceptable); }
