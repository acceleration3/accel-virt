#include "core/support/status.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/wire/wire_reader.hpp"

#include <cstddef>
#include <cstdint>

namespace av {

struct WireReaderProbe {
    explicit WireReaderProbe(WireReader &target) : reader(target) {}
    void set_offset(std::size_t offset) { reader.offset_ = offset; }
    void drop_bytes() { reader.bytes_ = nullptr; }
    WireReader &reader;
};

}  // namespace av

using av::Status;
using av::WireReader;
using av::WireReaderProbe;

namespace {

const std::uint8_t sample[] = {
    0x01,                                           /* u8  */
    0x34, 0x12,                                     /* u16 0x1234 */
    0x78, 0x56, 0x34, 0x12,                         /* u32 0x12345678 */
    0xEF, 0xCD, 0xAB, 0x89, 0x67, 0x45, 0x23, 0x01, /* u64 0x0123456789ABCDEF */
};

}  // namespace

TEST(wire_reader, reads_little_endian_fields_in_order)
{ WireReader reader(sample, sizeof sample);
  CHECK_EQ(reader.read_u8(), 0x01);
  CHECK(reader.invariants_hold());
  CHECK_EQ(reader.read_u16(), 0x1234);
  CHECK_EQ(reader.read_u32(), 0x12345678u);
  CHECK(reader.read_u64() == 0x0123456789ABCDEFull);
  CHECK(reader.invariants_hold());
  CHECK_EQ(reader.offset(), sizeof sample);
  CHECK_EQ(reader.remaining(), 0u);
  CHECK_STATUS(reader.status(), Status::ok); }

TEST(wire_reader, a_read_past_the_end_latches_malformed_without_advancing)
{ WireReader reader(sample, 3u);
  CHECK_EQ(reader.read_u8(), 0x01);
  CHECK_EQ(reader.offset(), 1u);
  CHECK_STATUS(reader.status(), Status::ok);
  CHECK_EQ(reader.read_u32(), 0u);
  CHECK_STATUS(reader.status(), Status::malformed);
  CHECK_EQ(reader.offset(), 1u);
  CHECK(reader.invariants_hold()); }

TEST(wire_reader, after_a_failure_every_read_returns_zero)
{ WireReader reader(sample, 1u);
  (void)reader.read_u64();
  CHECK_EQ(reader.read_u8(), 0u);
  CHECK_EQ(reader.read_u16(), 0u);
  CHECK_STATUS(reader.status(), Status::malformed);
  CHECK_EQ(reader.offset(), 0u); }

TEST(wire_reader, a_null_pointer_is_empty_whatever_the_length)
{ WireReader reader(nullptr, 64u);
  CHECK(reader.invariants_hold());
  CHECK_EQ(reader.remaining(), 0u);
  CHECK_EQ(reader.read_u8(), 0u);
  CHECK_STATUS(reader.status(), Status::malformed); }

TEST(wire_reader_expect_zero, accepts_zero_bytes)
{ const std::uint8_t zeros[4] = {};
  WireReader reader(zeros, sizeof zeros);
  reader.expect_zero(4u);
  CHECK_STATUS(reader.status(), Status::ok);
  CHECK_EQ(reader.offset(), 4u); }

TEST(wire_reader_expect_zero, a_non_zero_byte_is_malformed)
{ const std::uint8_t bytes[4] = {0, 0, 1, 0};
  WireReader reader(bytes, sizeof bytes);
  reader.expect_zero(4u);
  CHECK_STATUS(reader.status(), Status::malformed); }

TEST(wire_reader_expect_zero, a_short_buffer_is_malformed)
{ const std::uint8_t zeros[2] = {};
  WireReader reader(zeros, sizeof zeros);
  reader.expect_zero(3u);
  CHECK_STATUS(reader.status(), Status::malformed);
  CHECK_EQ(reader.offset(), 0u); }

TEST(wire_reader_copy_bytes, copies_the_next_bytes)
{ WireReader reader(sample, sizeof sample);
  std::uint8_t out[3] = {};
  reader.copy_bytes(out, 3u);
  CHECK_EQ(out[0], 0x01); CHECK_EQ(out[1], 0x34); CHECK_EQ(out[2], 0x12);
  CHECK_EQ(reader.offset(), 3u); }

TEST(wire_reader_copy_bytes, a_short_buffer_zero_fills_the_destination)
{ WireReader reader(sample, 2u);
  std::uint8_t out[3] = {9, 9, 9};
  reader.copy_bytes(out, 3u);
  CHECK_EQ(out[0], 0); CHECK_EQ(out[1], 0); CHECK_EQ(out[2], 0);
  CHECK_STATUS(reader.status(), Status::malformed); }

TEST(wire_reader, a_huge_count_does_not_overflow_the_bounds_check)
{ WireReader reader(sample, sizeof sample);
  (void)reader.read_u8();
  reader.expect_zero(~std::size_t{0});
  CHECK_STATUS(reader.status(), Status::malformed);
  CHECK_EQ(reader.offset(), 1u); }

TEST(wire_reader_invariants, an_offset_past_the_end_is_detected)
{ WireReader reader(sample, sizeof sample);
  WireReaderProbe(reader).set_offset(sizeof sample + 1u);
  CHECK(!reader.invariants_hold()); }

TEST(wire_reader_invariants, a_length_without_bytes_is_detected)
{ WireReader reader(sample, sizeof sample);
  WireReaderProbe(reader).drop_bytes();
  CHECK(!reader.invariants_hold()); }

TEST(wire_reader, nothing_remains_after_a_failure_so_read_loops_end)
{ WireReader reader(sample, sizeof sample);
  CHECK_EQ(reader.read_u8(), 0x01);
  (void)reader.read_u64();
  (void)reader.read_u64();
  CHECK_STATUS(reader.status(), Status::malformed);
  CHECK_EQ(reader.remaining(), 0u); }
