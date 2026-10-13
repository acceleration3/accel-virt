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
      0x41, 0x56, 0x58, 0x51, 0x01, 0x00, 0x02, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x88, 0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0xA5, 0xA5};
  CHECK(std::memcmp(bytes, expected, sizeof bytes) == 0);
  ExportRequest read;
  CHECK_STATUS(read.decode(bytes, sizeof bytes), Status::ok);
  CHECK(read.operation() == ExportOperation::sync);
  CHECK_EQ(read.surface(), 0x1122334455667788ull);
  CHECK_EQ(read.tag(), 0xA5A5000000000001ull);
  std::uint8_t small[ExportRequest::encoded_size - 1u] = {};
  CHECK_STATUS(sent.encode(small, sizeof small), Status::too_large); }

/* Hello and timing name no surface; export, sync and release name one, and
 * head names a head, 1 to 16 for heads 0 to 15. A timing built as a surface
 * request has no refresh and no moment, and is no timing. */
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
                        {ExportOperation::timing, 9u, Status::malformed},
                        {ExportOperation::head, 9u, Status::ok},
                        {ExportOperation::head, 0u, Status::malformed},
                        {ExportOperation::head, ExportRequest::most_heads + 1u, Status::malformed},
                        {static_cast<ExportOperation>(7u), 9u, Status::malformed}};
  for (const Case &item : cases) {
      std::uint8_t bytes[ExportRequest::encoded_size] = {};
      CHECK_STATUS(ExportRequest(item.operation, item.surface, 7u).encode(bytes, sizeof bytes), Status::ok);
      ExportRequest read(ExportOperation::release, 3u, 4u);
      CHECK_STATUS(read.decode(bytes, sizeof bytes), item.expected);
      /* A refusal leaves what was there. */
      CHECK_EQ(read.surface(), item.expected == Status::ok ? item.surface : 3u);
      CHECK_EQ(read.tag(), item.expected == Status::ok ? 7u : 4u);
  } }

/* A timing's refresh is where other requests have their reserved word, and
 * the moment it names where they name a surface. */
TEST(export_request, a_timing_carries_its_refresh_and_a_blank_at_the_documented_offsets)
{ std::uint8_t bytes[ExportRequest::encoded_size] = {};
  const ExportRequest sent(144000u, 0x0102030405060708ull, 9u);
  CHECK_EQ(sent.refresh_millihz(), 144000u);
  CHECK_EQ(sent.blank(), 0x0102030405060708ull);
  CHECK_EQ(sent.surface(), 0u);
  CHECK_STATUS(sent.encode(bytes, sizeof bytes), Status::ok);
  const std::uint8_t expected[ExportRequest::encoded_size] = {
      0x41, 0x56, 0x58, 0x51, 0x01, 0x00, 0x02, 0x00, 0x05, 0x00, 0x00, 0x00, 0x80, 0x32, 0x02, 0x00,
      0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x09, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  CHECK(std::memcmp(bytes, expected, sizeof bytes) == 0);
  ExportRequest read;
  CHECK_STATUS(read.decode(bytes, sizeof bytes), Status::ok);
  CHECK(read.operation() == ExportOperation::timing);
  CHECK_EQ(read.refresh_millihz(), 144000u);
  CHECK_EQ(read.blank(), 0x0102030405060708ull);
  CHECK_EQ(read.surface(), 0u);
  CHECK_EQ(read.tag(), 9u);
  /* Other requests carry neither. */
  CHECK_EQ(ExportRequest(ExportOperation::sync, 4u, 5u).refresh_millihz(), 0u);
  CHECK_EQ(ExportRequest(ExportOperation::sync, 4u, 5u).blank(), 0u); }

/* The display clock runs from 1 Hz to 1 kHz; a moment of 0 is none. */
TEST(export_request, a_timing_the_clock_cannot_run_at_or_naming_no_moment_is_malformed)
{ struct Case {
      std::uint32_t refresh;
      std::uint64_t blank;
      Status        expected;
  };
  const Case cases[] = {{1000u, 1u, Status::ok},           {1000000u, 1u, Status::ok},
                        {999u, 1u, Status::malformed},     {1000001u, 1u, Status::malformed},
                        {60000u, 0u, Status::malformed}};
  for (const Case &item : cases) {
      std::uint8_t bytes[ExportRequest::encoded_size] = {};
      CHECK_STATUS(ExportRequest(item.refresh, item.blank, 7u).encode(bytes, sizeof bytes), Status::ok);
      ExportRequest read;
      CHECK_STATUS(read.decode(bytes, sizeof bytes), item.expected);
      CHECK_EQ(read.refresh_millihz(), item.expected == Status::ok ? item.refresh : 0u);
  }
  /* A refresh in any other request is a reserved word that is not 0. */
  std::uint8_t bytes[ExportRequest::encoded_size] = {};
  CHECK_STATUS(ExportRequest(ExportOperation::hello, 0u, 7u).encode(bytes, sizeof bytes), Status::ok);
  put_u32(bytes, 12u, 60000u);
  ExportRequest read;
  CHECK_STATUS(read.decode(bytes, sizeof bytes), Status::malformed); }

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

/* A head request names head N as N + 1, and only a head request has one. */
TEST(export_request, a_head_request_names_its_head_less_one)
{ std::uint8_t bytes[ExportRequest::encoded_size] = {};
  CHECK_STATUS(ExportRequest(ExportOperation::head, 1u, 7u).encode(bytes, sizeof bytes), Status::ok);
  ExportRequest read;
  CHECK_STATUS(read.decode(bytes, sizeof bytes), Status::ok);
  CHECK(read.operation() == ExportOperation::head);
  CHECK_EQ(read.head(), 0u);
  CHECK_EQ(ExportRequest(ExportOperation::head, 16u, 7u).head(), 15u);
  CHECK_EQ(ExportRequest(ExportOperation::sync, 16u, 7u).head(), 0u); }
