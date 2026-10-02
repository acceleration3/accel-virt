/* The display clock page's header and a head's blank as the device writes
 * them and the guest reads them back: the fields at their documented offsets,
 * a period that is the rate's rounded down, and anything a device could get
 * wrong - magic, version, head count, reserved bytes, a rate out of range, a
 * period that is not the rate's - refused with the reader's state unchanged. */
#include "core/support/status.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/pages/clock_page_header.hpp"
#include "proto/pages/vblank_record.hpp"

#include <cstdint>
#include <cstring>

using av::ClockPageHeader;
using av::Status;
using av::VblankRecord;

namespace {

template <typename T>
T field_at(const std::uint8_t *bytes, std::size_t offset)
{
    T value = 0;
    std::memcpy(&value, bytes + offset, sizeof value);
    return value;
}

}  // namespace

TEST(clock_page_header, a_page_s_header_names_its_heads_and_where_each_record_is)
{ std::uint8_t bytes[ClockPageHeader::encoded_size] = {};
  const ClockPageHeader header(2u);
  CHECK_STATUS(header.encode(bytes, sizeof bytes), Status::ok);
  CHECK_EQ(field_at<std::uint32_t>(bytes, 0u), 0x00435641u);
  CHECK_EQ(field_at<std::uint32_t>(bytes, 4u), 1u);
  CHECK_EQ(field_at<std::uint32_t>(bytes, 8u), 2u);
  ClockPageHeader read;
  CHECK_STATUS(read.decode(bytes, sizeof bytes), Status::ok);
  CHECK_EQ(read.heads(), 2u);
  CHECK_EQ(read.record_offset(0u), 32u);
  CHECK_EQ(read.record_offset(1u), 96u);
  CHECK_EQ(read.record_offset(ClockPageHeader::max_heads - 1u) + ClockPageHeader::record_size,
           32u + 16u * 64u);
  static_assert(32u + 16u * 64u <= ClockPageHeader::page_size, "every head fits the page"); }

TEST(clock_page_header, a_header_with_no_heads_or_too_many_is_not_written)
{ std::uint8_t bytes[ClockPageHeader::encoded_size] = {};
  CHECK_STATUS(ClockPageHeader(0u).encode(bytes, sizeof bytes), Status::invalid_parameter);
  CHECK_STATUS(ClockPageHeader(17u).encode(bytes, sizeof bytes), Status::invalid_parameter);
  CHECK_STATUS(ClockPageHeader(16u).encode(bytes, sizeof bytes), Status::ok);
  CHECK_STATUS(ClockPageHeader(1u).encode(bytes, sizeof bytes - 1u), Status::too_large); }

TEST(clock_page_header, a_header_the_guest_cannot_trust_leaves_what_it_had)
{ std::uint8_t good[ClockPageHeader::encoded_size] = {};
  CHECK_STATUS(ClockPageHeader(3u).encode(good, sizeof good), Status::ok);
  struct Damage {
      std::size_t   offset;
      std::uint8_t  value;
  };
  const Damage damages[] = {{0u, 0x53u},   /* magic */
                            {4u, 2u},      /* version */
                            {8u, 0u},      /* no heads */
                            {8u, 17u},     /* more than a page holds */
                            {11u, 1u},     /* the head count's top byte */
                            {12u, 1u},     /* reserved */
                            {31u, 1u}};    /* reserved, last byte */
  for (const Damage &damage : damages) {
      std::uint8_t bytes[ClockPageHeader::encoded_size] = {};
      std::memcpy(bytes, good, sizeof bytes);
      bytes[damage.offset] = damage.value;
      ClockPageHeader header(5u);
      CHECK_STATUS(header.decode(bytes, sizeof bytes), Status::malformed);
      CHECK_EQ(header.heads(), 5u);
  }
  ClockPageHeader header(5u);
  CHECK_STATUS(header.decode(good, sizeof good - 1u), Status::malformed);
  CHECK_EQ(header.heads(), 5u);
  CHECK_STATUS(header.decode(nullptr, 64u), Status::malformed); }

TEST(vblank_record, a_blank_s_fields_sit_where_the_page_documents_them)
{ std::uint8_t record[ClockPageHeader::record_size] = {};
  const VblankRecord blank(7u, 0x0102030405060708u, 0x1112131415161718u, 143998u);
  CHECK_EQ(blank.period_ns(), 6944540u);   /* 10^12 / 143998, rounded down */
  CHECK_STATUS(blank.encode(record + VblankRecord::fields_at, sizeof record - VblankRecord::fields_at), Status::ok);
  CHECK_EQ(field_at<std::uint32_t>(record, 4u), 7u);
  CHECK_EQ(field_at<std::uint64_t>(record, 8u), 0x0102030405060708u);
  CHECK_EQ(field_at<std::uint64_t>(record, 16u), 0x1112131415161718u);
  CHECK_EQ(field_at<std::uint32_t>(record, 24u), 6944540u);
  CHECK_EQ(field_at<std::uint32_t>(record, 28u), 143998u);
  CHECK_EQ(field_at<std::uint32_t>(record, VblankRecord::interrupt_every_at), 0u);   /* the guest's, untouched */
  VblankRecord read;
  CHECK_STATUS(read.decode(record + VblankRecord::fields_at, VblankRecord::encoded_size), Status::ok);
  CHECK_EQ(read.latched(), 7u);
  CHECK_EQ(read.sequence(), 0x0102030405060708u);
  CHECK_EQ(read.vblank_ns(), 0x1112131415161718u);
  CHECK_EQ(read.period_ns(), 6944540u);
  CHECK_EQ(read.refresh_millihz(), 143998u); }

TEST(vblank_record, the_period_is_the_rate_s_at_both_ends_of_the_range)
{ CHECK_EQ(VblankRecord(0u, 1u, 0u, 60000u).period_ns(), 16666666u);
  CHECK_EQ(VblankRecord(0u, 1u, 0u, 1000u).period_ns(), 1000000000u);
  CHECK_EQ(VblankRecord(0u, 1u, 0u, 1000000u).period_ns(), 1000000u);
  CHECK_EQ(VblankRecord(0u, 1u, 0u, 0u).period_ns(), 0u); }

TEST(vblank_record, a_rate_out_of_range_or_a_period_not_the_rate_s_is_refused)
{ std::uint8_t good[VblankRecord::encoded_size] = {};
  CHECK_STATUS(VblankRecord(9u, 5u, 100u, 60000u).encode(good, sizeof good), Status::ok);
  struct Damage {
      std::size_t   offset;   /* within the fields */
      std::uint32_t value;
  };
  const Damage damages[] = {{20u, 16666667u},   /* one ns long */
                            {20u, 16666665u},   /* one ns short */
                            {24u, 999u},        /* under 1 Hz */
                            {24u, 1000001u},    /* past 1 kHz */
                            {24u, 0u}};
  for (const Damage &damage : damages) {
      std::uint8_t bytes[VblankRecord::encoded_size] = {};
      std::memcpy(bytes, good, sizeof bytes);
      std::memcpy(bytes + damage.offset, &damage.value, sizeof damage.value);
      VblankRecord record(1u, 2u, 3u, 1000u);
      CHECK_STATUS(record.decode(bytes, sizeof bytes), Status::malformed);
      CHECK_EQ(record.sequence(), 2u);
      CHECK_EQ(record.latched(), 1u);
  }
  VblankRecord record(1u, 2u, 3u, 1000u);
  CHECK_STATUS(record.decode(good, sizeof good - 1u), Status::malformed);
  CHECK_EQ(record.sequence(), 2u);
  CHECK_STATUS(VblankRecord(1u, 2u, 3u, 1000u).encode(good, sizeof good - 1u), Status::too_large); }
