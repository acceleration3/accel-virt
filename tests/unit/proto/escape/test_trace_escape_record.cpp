#include "harness/framework/hostile_sweep.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/escape/trace_escape_record.hpp"

#include <cstdint>
#include <cstring>

using av::Status;
using av::TraceEscapeRecord;

namespace {

using Kind = TraceEscapeRecord::Kind;

constexpr std::size_t escape_bytes = 40u + 64u;

void put(std::uint8_t *bytes, std::size_t offset, std::uint64_t value, std::size_t width)
{
    for (std::size_t byte = 0u; byte < width; byte++) bytes[offset + byte] = static_cast<std::uint8_t>(value >> (8u * byte));
}

bool reject_request(const std::uint8_t *bytes, std::size_t length)
{
    TraceEscapeRecord record;
    const Status decoded = record.decode(bytes, length);
    return decoded != Status::ok || record.window() > length - TraceEscapeRecord::encoded_size;
}

}  // namespace

/* docs/trace.md's layout, byte by byte. */
TEST(trace_escape_record, encodes_the_documented_layout)
{ std::uint8_t bytes[escape_bytes];
  std::memset(bytes, 0xEE, sizeof bytes);
  const TraceEscapeRecord request(Kind::state, 0x1122334455667788ull, 64u);
  CHECK_STATUS(request.encode(bytes, sizeof bytes), Status::ok);
  const std::uint8_t expected[40] = {
      'A', 'V', 'D', 0, 1, 0, 2, 0, 0x88, 0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      64, 0, 0, 0, 0, 0, 0, 0};
  CHECK(std::memcmp(bytes, expected, sizeof expected) == 0);
  CHECK_EQ(bytes[40], 0xEEu);
  CHECK_STATUS(request.encode(bytes, 39u), Status::invalid_parameter);
  CHECK_STATUS(request.encode(nullptr, 40u), Status::invalid_parameter); }

TEST(trace_escape_record, a_request_decodes_and_its_answer_is_accepted)
{ std::uint8_t bytes[escape_bytes];
  const TraceEscapeRecord request(Kind::table, 4096u, 64u);
  CHECK_STATUS(request.encode(bytes, sizeof bytes), Status::ok);
  TraceEscapeRecord kernel;
  CHECK_STATUS(kernel.decode(bytes, sizeof bytes), Status::ok);
  CHECK(kernel.kind() == Kind::table);
  CHECK_EQ(kernel.offset(), 4096u);
  CHECK_EQ(kernel.window(), 64u);
  CHECK_EQ(kernel.total(), 0u);
  CHECK_EQ(kernel.clock(), 0u);
  const TraceEscapeRecord answer = kernel.answered(4100u, 4u, 777u);
  CHECK_STATUS(answer.encode(bytes, sizeof bytes), Status::ok);
  TraceEscapeRecord process = request;
  CHECK_STATUS(process.accept_answer(bytes, sizeof bytes), Status::ok);
  CHECK_EQ(process.total(), 4100u);
  CHECK_EQ(process.window(), 4u);
  CHECK_EQ(process.clock(), 777u);
  CHECK(process.kind() == Kind::table);
  CHECK_EQ(process.offset(), 4096u); }

TEST(trace_escape_record, the_kernel_refuses_a_request_that_is_not_one)
{ std::uint8_t valid[escape_bytes];
  CHECK_STATUS(TraceEscapeRecord(Kind::state, 0u, 64u).encode(valid, sizeof valid), Status::ok);
  struct Break { std::size_t offset; std::uint64_t value; std::size_t width; };
  const Break breaks[] = {
      {0u, 0x444B5653u, 4u}, {4u, 2u, 2u}, {4u, 0u, 2u}, {6u, 0u, 2u}, {6u, 3u, 2u}, {6u, 0xFFFFu, 2u},
      {16u, 1u, 8u}, {24u, 1u, 8u}, {32u, 65u, 4u}, {32u, 0xFFFFFFFFu, 4u}, {36u, 1u, 4u}, {39u, 0x80u, 1u},
  };
  for (const Break &broken : breaks) {
    std::uint8_t bytes[escape_bytes];
    std::memcpy(bytes, valid, sizeof bytes);
    put(bytes, broken.offset, broken.value, broken.width);
    TraceEscapeRecord record(Kind::table, 9u, 9u);
    CHECK_STATUS(record.decode(bytes, sizeof bytes), Status::malformed);
    CHECK_EQ(record.offset(), 9u);
    CHECK_EQ(record.window(), 9u);
  }
  TraceEscapeRecord record;
  CHECK_STATUS(record.decode(valid, 39u), Status::malformed);
  CHECK_STATUS(record.decode(nullptr, sizeof valid), Status::malformed);
  /* A window of exactly the bytes after the record fits; one more does not. */
  CHECK_STATUS(record.decode(valid, sizeof valid), Status::ok);
  CHECK_STATUS(record.decode(valid, sizeof valid - 1u), Status::malformed);
  std::uint8_t empty[40];
  CHECK_STATUS(TraceEscapeRecord(Kind::table, 0u, 0u).encode(empty, sizeof empty), Status::ok);
  CHECK_STATUS(record.decode(empty, sizeof empty), Status::ok);
  CHECK_EQ(record.window(), 0u); }

TEST(trace_escape_record, the_process_refuses_an_answer_to_another_request)
{ const TraceEscapeRecord request(Kind::table, 100u, 64u);
  struct Case { TraceEscapeRecord answer; std::size_t length; };
  const Case cases[] = {
      {TraceEscapeRecord(Kind::state, 100u, 64u).answered(200u, 64u, 1u), escape_bytes},
      {TraceEscapeRecord(Kind::table, 101u, 64u).answered(200u, 64u, 1u), escape_bytes},
      {TraceEscapeRecord(Kind::table, 100u, 64u).answered(200u, 65u, 1u), escape_bytes + 1u},
      {TraceEscapeRecord(Kind::table, 100u, 64u).answered(99u, 0u, 1u), escape_bytes},
      {TraceEscapeRecord(Kind::table, 100u, 64u).answered(150u, 51u, 1u), escape_bytes},
      {TraceEscapeRecord(Kind::table, 100u, 64u).answered(200u, 64u, 1u), escape_bytes - 1u},
  };
  for (const Case &wrong : cases) {
    std::uint8_t bytes[escape_bytes + 1u] = {};
    CHECK_STATUS(wrong.answer.encode(bytes, sizeof bytes), Status::ok);
    TraceEscapeRecord process = request;
    CHECK_STATUS(process.accept_answer(bytes, wrong.length), Status::malformed);
    CHECK_EQ(process.total(), 0u);
    CHECK_EQ(process.window(), 64u);
  }
  std::uint8_t garbage[escape_bytes] = {};
  TraceEscapeRecord process = request;
  CHECK_STATUS(process.accept_answer(garbage, sizeof garbage), Status::malformed);
  /* The window may end exactly at the total, and an offset at the total is an empty answer. */
  std::uint8_t bytes[escape_bytes];
  CHECK_STATUS(request.answered(150u, 50u, 3u).encode(bytes, sizeof bytes), Status::ok);
  CHECK_STATUS(process.accept_answer(bytes, sizeof bytes), Status::ok);
  CHECK_EQ(process.window(), 50u);
  TraceEscapeRecord at_end = request;
  CHECK_STATUS(request.answered(100u, 0u, 3u).encode(bytes, sizeof bytes), Status::ok);
  CHECK_STATUS(at_end.accept_answer(bytes, sizeof bytes), Status::ok);
  CHECK_EQ(at_end.window(), 0u);
  /* An offset near 2^64 cannot make the window wrap past the total. */
  const TraceEscapeRecord far(Kind::table, 0xFFFFFFFFFFFFFFF0ull, 64u);
  CHECK_STATUS(far.answered(0xFFFFFFFFFFFFFFF8ull, 32u, 1u).encode(bytes, sizeof bytes), Status::ok);
  TraceEscapeRecord far_process = far;
  CHECK_STATUS(far_process.accept_answer(bytes, sizeof bytes), Status::malformed); }

TEST(trace_escape_record, hostile_requests_never_claim_a_window_past_their_bytes)
{ std::uint8_t valid[escape_bytes];
  CHECK_STATUS(TraceEscapeRecord(Kind::state, 5u, 64u).encode(valid, sizeof valid), Status::ok);
  av::HostileSweep sweep(&reject_request, valid, sizeof valid);
  sweep.run();
  CHECK(sweep.cases_run() > sizeof valid);
  /* The record is 40 bytes in front of a 64-byte window, and the window's 512
   * bits are payload no reader touches. Of the record, only the offset is free
   * (64 bits): the magic, the version, the kind, the reserved word and the
   * answer fields a request must leave zero are all judged, and of the window
   * length only the flip that clears 64 to nothing still fits the bytes it
   * arrived in. Every shorter input either cannot hold the record or cannot
   * hold the window it claims, all-zero and all-ones carry no magic, and the
   * untouched request is the last acceptance. */
  const std::uint32_t acceptable = 512u + 64u + 1u + 1u;
  CHECK(sweep.rejections() >= sweep.cases_run() - acceptable); }
