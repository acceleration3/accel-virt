#include "core/support/status.hpp"
#include "harness/framework/byte_layout.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/escape/release_record.hpp"

#include <cstddef>
#include <cstdint>

using av::ByteLayout;
using av::ReleaseRecord;
using av::Status;
using av::SubmissionDescriptor;

TEST(release_record, places_its_fields_and_reads_back_as_written)
{ std::uint8_t bytes[ReleaseRecord::encoded_size] = {};
  CHECK_STATUS(ReleaseRecord(0x11223344u, SubmissionDescriptor(7u, 64u, 128u)).encode(bytes, sizeof bytes), Status::ok);
  const ByteLayout layout(bytes, sizeof bytes);
  CHECK(layout.at(0u, 4u) == 0x00525641u);
  CHECK(layout.at(4u, 4u) == 0x11223344u);
  ReleaseRecord read;
  CHECK_STATUS(read.decode(bytes, sizeof bytes), Status::ok);
  CHECK_EQ(read.ring(), 0x11223344u);
  CHECK(read.batch().fence_id() == 7u && read.batch().begin() == 64u && read.batch().end() == 128u); }

TEST(release_record, a_longer_private_data_carries_the_record_as_its_prefix)
{ std::uint8_t bytes[ReleaseRecord::encoded_size + 16u] = {};
  CHECK_STATUS(ReleaseRecord(5u, SubmissionDescriptor(1u, 0u, 8u)).encode(bytes, sizeof bytes), Status::ok);
  ReleaseRecord read;
  CHECK_STATUS(read.decode(bytes, sizeof bytes), Status::ok);
  CHECK_EQ(read.ring(), 5u); }

TEST(release_record, anything_else_is_malformed_and_leaves_the_reader_unchanged)
{ std::uint8_t bytes[ReleaseRecord::encoded_size] = {};
  CHECK_STATUS(ReleaseRecord(5u, SubmissionDescriptor(1u, 0u, 8u)).encode(bytes, sizeof bytes), Status::ok);
  ReleaseRecord read(3u, SubmissionDescriptor(2u, 8u, 16u));
  CHECK_STATUS(read.decode(bytes, ReleaseRecord::encoded_size - 1u), Status::malformed);
  CHECK_STATUS(read.decode(nullptr, ReleaseRecord::encoded_size), Status::malformed);
  std::uint8_t other[ReleaseRecord::encoded_size] = {};
  for (std::size_t i = 0u; i < sizeof other; i++) other[i] = bytes[i];
  other[0] ^= 0xFFu;
  CHECK_STATUS(read.decode(other, sizeof other), Status::malformed);
  for (std::size_t i = 0u; i < sizeof other; i++) other[i] = bytes[i];
  other[4] = other[5] = other[6] = other[7] = 0u;
  CHECK_STATUS(read.decode(other, sizeof other), Status::malformed);
  for (std::size_t i = 0u; i < sizeof other; i++) other[i] = bytes[i];
  for (std::size_t i = 16u; i < 24u; i++) other[i] = 0u;
  CHECK_STATUS(read.decode(other, sizeof other), Status::malformed);
  CHECK_EQ(read.ring(), 3u);
  CHECK_STATUS(ReleaseRecord(5u, SubmissionDescriptor(1u, 0u, 8u)).encode(other, ReleaseRecord::encoded_size - 1u),
               Status::too_large); }

TEST(release_record, a_missing_destination_is_too_large_whatever_the_capacity)
{ CHECK_STATUS(ReleaseRecord(5u, SubmissionDescriptor(1u, 0u, 8u)).encode(nullptr, ReleaseRecord::encoded_size),
               Status::too_large); }
