#include "core/support/status.hpp"
#include "harness/framework/guarded_buffer.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_spill.hpp"
#include "proto/wire/wire_writer.hpp"

#include <cstddef>
#include <cstdint>

namespace av {

struct WireWriterProbe {
    explicit WireWriterProbe(WireWriter &target) : writer(target) {}
    void set_offset(std::size_t offset) { writer.offset_ = offset; }
    void drop_bytes() { writer.bytes_ = nullptr; }
    void claim_spilled() { writer.spilled_ = true; }
    WireWriter &writer;
};

}

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

TEST(wire_writer, says_how_much_room_the_whole_answer_wanted)
{ std::uint8_t frame[8];
  WireWriter writer(frame, sizeof frame);
  writer.write_u32(1u);
  CHECK_EQ(writer.wanted(), 4u);
  writer.write_u64(2u);
  CHECK_STATUS(writer.status(), Status::too_large);
  CHECK_EQ(writer.offset(), 4u);
  CHECK_EQ(writer.wanted(), 12u);
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

namespace {
class OneBlockSpill final : public av::WireSpill {
public:
    std::uint8_t *grow(const std::uint8_t *written, std::size_t used, std::size_t wanted,
                       std::size_t &capacity) override
    {
        asked_++;
        wanted_ = wanted;
        capacity = 0u;
        if (asked_ > 1u || wanted > sizeof block_) return nullptr;
        for (std::size_t index = 0u; index < used; ++index) block_[index] = written[index];
        capacity = sizeof block_;
        return block_;
    }
    std::uint32_t asked_ = 0u;
    std::size_t   wanted_ = 0u;
    std::uint8_t  block_[32] = {};
};

}

TEST(wire_writer, a_writer_with_a_spill_goes_on_past_its_room)
{ GuardedBuffer buffer(6u);
  OneBlockSpill spill;
  WireWriter    writer(buffer.data(), buffer.size(), spill);
  writer.write_u32(0x04030201u);
  CHECK(!writer.spilled());
  CHECK(writer.bytes() == buffer.data());
  writer.write_u64(0x0C0B0A0908070605ull);
  CHECK_STATUS(writer.status(), Status::ok);
  CHECK(writer.spilled());
  CHECK(writer.bytes() == spill.block_);
  CHECK_EQ(spill.wanted_, 12u);
  CHECK_EQ(writer.offset(), 12u);
  CHECK_EQ(writer.wanted(), 12u);
  for (std::uint8_t index = 0u; index < 12u; ++index) CHECK_EQ(spill.block_[index], index + 1u);
  CHECK(buffer.guards_intact());
  CHECK(writer.invariants_hold());
  writer.write_zeros(64u);
  CHECK_STATUS(writer.status(), Status::too_large);
  CHECK_EQ(writer.offset(), 12u);
  CHECK_EQ(spill.asked_, 2u); }

TEST(wire_writer, a_writer_without_a_spill_never_reads_as_spilled)
{ GuardedBuffer buffer(2u);
  WireWriter    writer(buffer.data(), buffer.size());
  writer.write_u32(1u);
  CHECK(!writer.spilled());
  CHECK(writer.bytes() == buffer.data());
  CHECK(writer.invariants_hold()); }

namespace {
class ShortSpill final : public av::WireSpill {
public:
    std::uint8_t *grow(const std::uint8_t *, std::size_t, std::size_t, std::size_t &capacity) override
    {
        capacity = 1u;
        return block_;
    }
    std::uint8_t block_[1] = {};
};

}

TEST(wire_writer, a_writer_with_a_spill_and_no_room_of_its_own_spills_at_the_first_write)
{ OneBlockSpill spill;
  WireWriter    writer(nullptr, 64u, spill);
  CHECK(writer.invariants_hold());
  writer.write_u32(0x04030201u);
  CHECK_STATUS(writer.status(), Status::ok);
  CHECK(writer.spilled());
  CHECK(writer.bytes() == spill.block_);
  CHECK_EQ(spill.block_[3], 4u);
  CHECK(writer.invariants_hold()); }

TEST(wire_writer, a_spill_that_hands_back_less_than_wanted_latches_too_large)
{ GuardedBuffer buffer(2u);
  ShortSpill    spill;
  WireWriter    writer(buffer.data(), buffer.size(), spill);
  writer.write_u32(1u);
  CHECK_STATUS(writer.status(), Status::too_large);
  CHECK(!writer.spilled());
  CHECK(writer.bytes() == buffer.data());
  CHECK_EQ(writer.offset(), 0u);
  CHECK(writer.invariants_hold()); }

TEST(wire_writer_invariants, a_spilled_writer_without_a_spill_is_detected)
{ GuardedBuffer buffer(4u);
  WireWriter writer(buffer.data(), buffer.size());
  WireWriterProbe(writer).claim_spilled();
  CHECK(!writer.invariants_hold()); }
