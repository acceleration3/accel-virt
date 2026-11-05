#include "core/support/status.hpp"
#include "harness/framework/byte_layout.hpp"
#include "harness/framework/guarded_buffer.hpp"
#include "harness/framework/hostile_sweep.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/submission/blob_import_request.hpp"
#include "proto/wire/guest_bytes.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

using av::BlobImportRequest;
using av::BlobRole;
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

}

TEST(blob_import_request_encode, lays_out_the_resource_and_a_plain_role)
{ std::uint8_t bytes[BlobImportRequest::encoded_size];
  std::memset(bytes, 0xEE, sizeof bytes);
  encode(BlobImportRequest(0x01000146u), bytes);
  const ByteLayout layout(bytes, sizeof bytes);
  CHECK(layout.at(0u, 4u) == 0x01000146u);
  CHECK(layout.at(4u, 4u) == 0u);
  CHECK(layout.at(8u, 8u) == 0u); }

TEST(blob_import_request_encode, lays_out_an_arena_role_with_its_base)
{ std::uint8_t bytes[BlobImportRequest::encoded_size];
  std::memset(bytes, 0xEE, sizeof bytes);
  encode(BlobImportRequest(7u, BlobRole(BlobRole::Kind::arena_segment, av::GuestBytes::slot_bytes * 3u)), bytes);
  const ByteLayout layout(bytes, sizeof bytes);
  CHECK(layout.at(0u, 4u) == 7u);
  CHECK(layout.at(4u, 4u) == 1u);
  CHECK(layout.at(8u, 8u) == av::GuestBytes::slot_bytes * 3u); }

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

TEST(blob_import_request_decode, round_trips_the_role_it_was_sent_with)
{ const BlobRole roles[] = {BlobRole(), BlobRole(BlobRole::Kind::arena_segment, 0u),
                            BlobRole(BlobRole::Kind::arena_segment, av::GuestBytes::slot_bytes * 63u)};
  for (const BlobRole &role : roles) {
      std::uint8_t bytes[BlobImportRequest::encoded_size] = {};
      encode(BlobImportRequest(5u, role), bytes);
      BlobImportRequest decoded;
      CHECK_STATUS(decoded.decode(bytes, sizeof bytes), Status::ok);
      CHECK(decoded.role() == role);
  } }

TEST(blob_import_request_decode, any_length_but_its_own_is_framing)
{ std::uint8_t bytes[BlobImportRequest::encoded_size + 8u] = {};
  encode(BlobImportRequest(5u), bytes);
  const std::size_t lengths[] = {0u, 1u, 4u, 7u, 8u, 9u, 15u, 17u, 24u};
  for (const std::size_t length : lengths) {
      BlobImportRequest decoded(9u);
      CHECK_STATUS(decoded.decode(bytes, length), Status::malformed);
      CHECK_EQ(decoded.resource_id(), 9u);
  } }

TEST(blob_import_request_decode, no_resource_or_a_bad_role_is_an_invalid_parameter_left_unread)
{ struct Case { std::size_t offset; std::uint64_t value; std::size_t width; };
  const Case cases[] = {{0u, 0u, 4u}, {4u, 2u, 4u}, {4u, 0x80000000u, 4u}, {8u, 1u, 8u}, {8u, ~std::uint64_t{0}, 8u}};
  for (const Case &c : cases) {
      std::uint8_t bytes[BlobImportRequest::encoded_size] = {};
      encode(BlobImportRequest(5u), bytes);
      std::memcpy(bytes + c.offset, &c.value, c.width);
      BlobImportRequest decoded(9u);
      CHECK_STATUS(decoded.decode(bytes, sizeof bytes), Status::invalid_parameter);
      CHECK_EQ(decoded.resource_id(), 9u);
      CHECK(decoded.role() == BlobRole());
  } }

TEST(blob_import_request_decode, an_arena_base_off_a_slot_or_past_the_table_is_refused)
{ const std::uint64_t bases[] = {1u, av::GuestBytes::slot_bytes - 1u, av::GuestBytes::slot_bytes + 4096u,
                                 av::GuestBytes::slot_bytes * av::GuestBytes::slot_count};
  for (const std::uint64_t base : bases) {
      std::uint8_t bytes[BlobImportRequest::encoded_size] = {};
      encode(BlobImportRequest(5u, BlobRole(BlobRole::Kind::arena_segment, 0u)), bytes);
      std::memcpy(bytes + 8u, &base, sizeof base);
      BlobImportRequest decoded;
      CHECK_STATUS(decoded.decode(bytes, sizeof bytes), Status::invalid_parameter);
  } }

TEST(blob_import_request_decode, survives_the_hostile_sweep)
{ std::uint8_t bytes[BlobImportRequest::encoded_size] = {};
  encode(BlobImportRequest(0x01000146u), bytes);
  const av::HostileSweep sweep(rejects, bytes, sizeof bytes);
  sweep.run();
  CHECK(sweep.cases_run() > 64u);
  const std::uint32_t acceptable = 32u + 1u + 8u;
  CHECK(sweep.rejections() >= sweep.cases_run() - acceptable); }
