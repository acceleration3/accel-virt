#include "../../src/core/status.hpp"
#include "../../src/proto/wire_writer.hpp"
#include "../harness/guarded_buffer.hpp"
#include "../harness/test_registry.hpp"

#include <cstddef>
#include <cstdint>

namespace av {

struct WireWriterProbe {
    explicit WireWriterProbe(WireWriter &target) : writer(target) {}
    void set_offset(std::size_t offset) { writer.offset_ = offset; }
    void drop_bytes() { writer.bytes_ = nullptr; }
    WireWriter &writer;
};

}  // namespace av

using av::GuardedBuffer;
using av::WireWriterProbe;
using av::Status;
using av::WireWriter;

TEST(wire_writer, writes_little_endian_fields_in_order)
{ GuardedBuffer buffer(15u);
  WireWriter writer(buffer.data(), buffer.size());
  writer.write_u8(0x01);
  CHECK(writer.invariants_hold());
  writer.write_u16(0x1234);
  writer.write_u32(0x12345678u);
  writer.write_u64(0x0123456789ABCDEFull);
  CHECK(writer.invariants_hold());
  CHECK_STATUS(writer.status(), Status::ok);
  CHECK_EQ(writer.offset(), 15u);
  const std::uint8_t *b = buffer.data();
  CHECK_EQ(b[0], 0x01); CHECK_EQ(b[1], 0x34); CHECK_EQ(b[2], 0x12);
  CHECK_EQ(b[3], 0x78); CHECK_EQ(b[6], 0x12);
  CHECK_EQ(b[7], 0xEF); CHECK_EQ(b[14], 0x01);
  CHECK(buffer.guards_intact()); }

TEST(wire_writer, a_field_that_does_not_fit_is_not_partly_written)
{ GuardedBuffer buffer(3u);
  WireWriter writer(buffer.data(), buffer.size());
  writer.write_u32(0xFFFFFFFFu);
  CHECK_STATUS(writer.status(), Status::too_large);
  CHECK_EQ(writer.offset(), 0u);
  CHECK_EQ(buffer.data()[0], 0); CHECK_EQ(buffer.data()[2], 0);
  CHECK(buffer.guards_intact());
  CHECK(writer.invariants_hold()); }

TEST(wire_writer, after_a_failure_every_write_is_ignored)
{ GuardedBuffer buffer(4u);
  WireWriter writer(buffer.data(), buffer.size());
  writer.write_u64(1u);
  writer.write_u8(0xAA);
  writer.write_u16(0xAAAA);
  CHECK_EQ(writer.offset(), 0u);
  CHECK_EQ(buffer.data()[0], 0);
  CHECK_STATUS(writer.status(), Status::too_large); }

TEST(wire_writer, a_null_destination_has_no_capacity)
{ WireWriter writer(nullptr, 64u);
  CHECK(writer.invariants_hold());
  writer.write_u8(1u);
  CHECK_STATUS(writer.status(), Status::too_large); }

TEST(wire_writer_zeros, writes_zero_bytes)
{ GuardedBuffer buffer(4u);
  buffer.data()[1] = 7u;
  WireWriter writer(buffer.data(), buffer.size());
  writer.write_zeros(4u);
  CHECK_EQ(buffer.data()[1], 0);
  CHECK_EQ(writer.offset(), 4u);
  CHECK(buffer.guards_intact()); }

TEST(wire_writer_copy_bytes, copies_and_respects_capacity)
{ GuardedBuffer buffer(2u);
  const std::uint8_t source[3] = {1, 2, 3};
  WireWriter writer(buffer.data(), buffer.size());
  writer.copy_bytes(source, 2u);
  CHECK_EQ(buffer.data()[1], 2);
  writer.copy_bytes(source, 1u);
  CHECK_STATUS(writer.status(), Status::too_large);
  CHECK(buffer.guards_intact()); }

TEST(wire_writer, a_huge_count_does_not_overflow_the_bounds_check)
{ GuardedBuffer buffer(4u);
  WireWriter writer(buffer.data(), buffer.size());
  writer.write_u8(1u);
  writer.write_zeros(~std::size_t{0});
  CHECK_STATUS(writer.status(), Status::too_large);
  CHECK(buffer.guards_intact()); }

TEST(wire_writer_invariants, an_offset_past_capacity_is_detected)
{ GuardedBuffer buffer(4u);
  WireWriter writer(buffer.data(), buffer.size());
  WireWriterProbe(writer).set_offset(5u);
  CHECK(!writer.invariants_hold()); }

TEST(wire_writer_invariants, a_capacity_without_bytes_is_detected)
{ GuardedBuffer buffer(4u);
  WireWriter writer(buffer.data(), buffer.size());
  WireWriterProbe(writer).drop_bytes();
  CHECK(!writer.invariants_hold()); }
