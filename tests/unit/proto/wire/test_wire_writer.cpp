#include "core/support/status.hpp"
#include "harness/framework/guarded_buffer.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

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
using av::WireReader;
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

/* What the generated serializers call: fields of the registry's own types by
 * size, runs of bytes, and varints for lengths that are usually small
 * (feature 063). */
TEST(wire_writer_write_pod, writes_a_field_of_the_registry_s_own_type)
{ struct Extent { std::uint32_t width; std::uint32_t height; };
  std::uint8_t bytes[16] = {};
  WireWriter writer(bytes, sizeof bytes);
  const Extent extent = {1600u, 1200u};
  writer.write_pod(extent);
  CHECK_STATUS(writer.status(), Status::ok);
  CHECK_EQ(writer.offset(), sizeof extent);
  WireReader reader(bytes, sizeof bytes);
  Extent read_back = {};
  CHECK(reader.read_pod(read_back));
  CHECK_EQ(read_back.width, 1600u);
  CHECK_EQ(read_back.height, 1200u); }

TEST(wire_writer_write_pod, a_field_that_does_not_fit_writes_nothing)
{ std::uint8_t bytes[3] = {0xEE, 0xEE, 0xEE};
  WireWriter writer(bytes, sizeof bytes);
  writer.write_pod(std::uint32_t{0x11223344u});
  CHECK_STATUS(writer.status(), Status::too_large);
  CHECK_EQ(writer.offset(), 0u);
  CHECK_EQ(bytes[0], 0xEEu); }

/* Every varint the writer emits is one the reader takes back, which is the
 * only property either of them owes the other. */
TEST(wire_writer_write_varint, round_trips_every_width)
{ const std::uint64_t values[] = {0u, 1u, 127u, 128u, 300u, 0xFFFFFFFFu,
                                  0xFFFFFFFFFFFFFFFFull};
  for (const std::uint64_t value : values) {
      std::uint8_t bytes[16] = {};
      WireWriter writer(bytes, sizeof bytes);
      writer.write_varint(value);
      CHECK_STATUS(writer.status(), Status::ok);
      WireReader reader(bytes, writer.offset());
      std::uint64_t read_back = 0u;
      CHECK(reader.read_varint(read_back));
      CHECK(read_back == value);
      CHECK_EQ(reader.remaining(), 0u);
  } }

TEST(wire_writer_write_varint, a_varint_that_does_not_fit_writes_nothing)
{ std::uint8_t bytes[1] = {0xEE};
  WireWriter writer(bytes, sizeof bytes);
  writer.write_varint(std::uint32_t{300u});
  CHECK_STATUS(writer.status(), Status::too_large);
  CHECK_EQ(writer.offset(), 0u);
  CHECK_EQ(bytes[0], 0xEEu); }

/* An answer that does not fit has to say how much room it wanted, because the
 * device promises the guest a length it can allocate against and then ask
 * again (feature 042, the executor ABI's A7). */
TEST(wire_writer, says_how_much_room_the_whole_answer_wanted)
{ std::uint8_t frame[8];
  WireWriter writer(frame, sizeof frame);
  writer.write_u32(1u);
  CHECK_EQ(writer.wanted(), 4u);
  writer.write_u64(2u);
  CHECK_STATUS(writer.status(), Status::too_large);
  /* Four written, twelve wanted: the rest of the answer is still counted. */
  CHECK_EQ(writer.offset(), 4u);
  CHECK_EQ(writer.wanted(), 12u);
  /* And what follows a refusal is counted too, so the length is the answer's
   * and not the prefix that happened to fit. */
  writer.write_u16(3u);
  CHECK_EQ(writer.wanted(), 14u);
  CHECK(writer.invariants_hold()); }

TEST(wire_writer, an_answer_that_fits_wanted_exactly_what_it_wrote)
{ std::uint8_t frame[16];
  WireWriter writer(frame, sizeof frame);
  writer.write_u64(1u);
  writer.write_u32(2u);
  CHECK_STATUS(writer.status(), Status::ok);
  CHECK_EQ(writer.wanted(), writer.offset());
  CHECK_EQ(writer.wanted(), 12u); }
