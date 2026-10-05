/* A consumer's request on the export socket, as the device reads it: 32 bytes
 * of magic, version, operation, a reserved word, the surface and the tag. What
 * a consumer could not have meant - a hello naming a surface, an export
 * naming none, an operation only the device sends - decodes to nothing, and
 * another major version is a refusal of its own, since its layout may be any. */
#include "core/support/status.hpp"
#include "harness/framework/hostile_sweep.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/export/export_request.hpp"

#include <cstdint>
#include <cstring>

using av::ExportOperation;
using av::ExportRequest;
using av::Status;

namespace {

bool rejects(const std::uint8_t *bytes, std::size_t length)
{
    ExportRequest request;
    return request.decode(bytes, length) != Status::ok;
}

void put_u32(std::uint8_t *bytes, std::size_t at, std::uint32_t value) { std::memcpy(bytes + at, &value, sizeof value); }

}  // namespace

TEST(export_request, encodes_at_the_documented_offsets_and_decodes_back)
{ std::uint8_t bytes[ExportRequest::encoded_size] = {};
  const ExportRequest sent(ExportOperation::sync, 0x1122334455667788ull, 0xA5A5000000000001ull);
  CHECK_STATUS(sent.encode(bytes, sizeof bytes), Status::ok);
  const std::uint8_t expected[ExportRequest::encoded_size] = {
      0x41, 0x56, 0x58, 0x51, 0x01, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x88, 0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0xA5, 0xA5};
  CHECK(std::memcmp(bytes, expected, sizeof bytes) == 0);
  ExportRequest read;
  CHECK_STATUS(read.decode(bytes, sizeof bytes), Status::ok);
  CHECK(read.operation() == ExportOperation::sync);
  CHECK_EQ(read.surface(), 0x1122334455667788ull);
  CHECK_EQ(read.tag(), 0xA5A5000000000001ull);
  std::uint8_t small[ExportRequest::encoded_size - 1u] = {};
  CHECK_STATUS(sent.encode(small, sizeof small), Status::too_large); }

/* Hello names no surface; the other three name one. */
TEST(export_request, each_operation_names_a_surface_or_none_as_it_must)
{ struct Case {
      ExportOperation operation;
      std::uint64_t   surface;
      Status          expected;
  };
  const Case cases[] = {{ExportOperation::hello, 0u, Status::ok},
                        {ExportOperation::hello, 1u, Status::malformed},
                        {ExportOperation::export_surface, 9u, Status::ok},
                        {ExportOperation::export_surface, 0u, Status::malformed},
                        {ExportOperation::sync, ~0ull, Status::ok},
                        {ExportOperation::sync, 0u, Status::malformed},
                        {ExportOperation::release, 9u, Status::ok},
                        {ExportOperation::release, 0u, Status::malformed},
                        {ExportOperation::revoked, 9u, Status::malformed},
                        {ExportOperation::unreadable, 0u, Status::malformed},
                        {static_cast<ExportOperation>(5u), 9u, Status::malformed}};
  for (const Case &item : cases) {
      std::uint8_t bytes[ExportRequest::encoded_size] = {};
      CHECK_STATUS(ExportRequest(item.operation, item.surface, 7u).encode(bytes, sizeof bytes), Status::ok);
      ExportRequest read(ExportOperation::release, 3u, 4u);
      CHECK_STATUS(read.decode(bytes, sizeof bytes), item.expected);
      /* A refusal leaves what was there. */
      CHECK_EQ(read.surface(), item.expected == Status::ok ? item.surface : 3u);
      CHECK_EQ(read.tag(), item.expected == Status::ok ? 7u : 4u);
  } }

/* Another major is not this layout, whatever its bytes; another minor is. */
TEST(export_request, another_major_is_not_supported_and_another_minor_is_read)
{ std::uint8_t bytes[ExportRequest::encoded_size] = {};
  CHECK_STATUS(ExportRequest(ExportOperation::export_surface, 5u, 6u).encode(bytes, sizeof bytes), Status::ok);
  bytes[6] = 0x07u;   /* minor 7 */
  ExportRequest read;
  CHECK_STATUS(read.decode(bytes, sizeof bytes), Status::ok);
  CHECK_EQ(read.surface(), 5u);
  bytes[4] = 0x02u;   /* major 2 */
  bytes[12] = 0xFFu;  /* and a reserved word no version 1 sends */
  ExportRequest other;
  CHECK_STATUS(other.decode(bytes, sizeof bytes), Status::not_supported);
  CHECK_EQ(other.surface(), 0u);
  bytes[4] = 0x00u;   /* major 0 */
  CHECK_STATUS(other.decode(bytes, sizeof bytes), Status::not_supported); }

TEST(export_request, a_wrong_length_magic_or_reserved_word_is_malformed)
{ std::uint8_t bytes[ExportRequest::encoded_size + 1u] = {};
  CHECK_STATUS(ExportRequest(ExportOperation::release, 5u, 6u).encode(bytes, sizeof bytes), Status::ok);
  ExportRequest read;
  CHECK_STATUS(read.decode(bytes, ExportRequest::encoded_size + 1u), Status::malformed);
  CHECK_STATUS(read.decode(bytes, ExportRequest::encoded_size - 1u), Status::malformed);
  CHECK_STATUS(read.decode(bytes, 0u), Status::malformed);
  put_u32(bytes, 12u, 1u);
  CHECK_STATUS(read.decode(bytes, ExportRequest::encoded_size), Status::malformed);
  put_u32(bytes, 12u, 0u);
  put_u32(bytes, 0u, 0x41585641u);   /* an answer's magic */
  CHECK_STATUS(read.decode(bytes, ExportRequest::encoded_size), Status::malformed);
  CHECK_EQ(read.surface(), 0u); }

TEST(export_request_decode, survives_the_hostile_sweep)
{ std::uint8_t bytes[ExportRequest::encoded_size] = {};
  CHECK_STATUS(ExportRequest(ExportOperation::export_surface, 0x41u, 0x99u).encode(bytes, sizeof bytes),
               Status::ok);
  const av::HostileSweep sweep(rejects, bytes, sizeof bytes);
  sweep.run();
  CHECK_EQ(sweep.cases_run(), 32u + 1u + 8u * 32u + 2u);
  /* Free: the minor's 16 flips; the surface's 64, since 0x41 has two bits
   * set and no single flip makes it 0; the tag's 64; and of the operation's,
   * only bit 0, which makes export (2) sync (3) - another operation naming a
   * surface. With the untouched request, 146 acceptances. */
  const std::uint32_t acceptable = 16u + 64u + 64u + 1u + 1u;
  CHECK(sweep.rejections() >= sweep.cases_run() - acceptable); }
