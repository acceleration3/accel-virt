/* A present's point as the user-mode driver hands it DdiPresent: the fields at
 * their offsets, a round trip, and private data that is anything else -
 * another magic, a point of 0, reserved bytes, too few bytes, a snapshot key
 * an older driver sent - refused with the reader unchanged. */
#include "core/support/status.hpp"
#include "harness/framework/byte_layout.hpp"
#include "harness/framework/guarded_buffer.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/submission/present_point_record.hpp"

#include <cstddef>
#include <cstdint>

using av::ByteLayout;
using av::GuardedBuffer;
using av::PresentPointRecord;
using av::Status;

TEST(present_point_record, places_its_fields_and_reads_back_as_written)
{ std::uint8_t bytes[PresentPointRecord::encoded_size] = {};
  CHECK_STATUS(PresentPointRecord(0x88776655u).encode(bytes, sizeof bytes), Status::ok);
  const ByteLayout layout(bytes, sizeof bytes);
  CHECK(layout.at(0u, 4u) == 0x504B5652u);   /* "RVKP" */
  CHECK(layout.at(4u, 4u) == 0x88776655u);
  CHECK(layout.at(8u, 8u) == 0u);
  PresentPointRecord read;
  CHECK(read.point() == 0u);
  CHECK_STATUS(read.decode(bytes, sizeof bytes), Status::ok);
  CHECK(read.point() == 0x88776655u); }

TEST(present_point_record, anything_else_is_malformed_and_changes_nothing)
{ struct Case { std::size_t offset; std::size_t width; std::uint64_t value; };
  /* Another magic - the snapshot record's "RVKS" among them - no point, and
   * each reserved byte. */
  const Case cases[] = {{0u, 4u, 0x534B5652u}, {0u, 4u, 0x504B5653u}, {4u, 4u, 0u}, {8u, 1u, 1u}, {15u, 1u, 0x80u}};
  for (const Case &c : cases) {
      std::uint8_t bytes[PresentPointRecord::encoded_size] = {};
      CHECK_STATUS(PresentPointRecord(7u).encode(bytes, sizeof bytes), Status::ok);
      for (std::size_t i = 0u; i < c.width; i++) bytes[c.offset + i] = static_cast<std::uint8_t>(c.value >> (8u * i));
      PresentPointRecord read(3u);
      CHECK_STATUS(read.decode(bytes, sizeof bytes), Status::malformed);
      CHECK(read.point() == 3u);
  }
  std::uint8_t bytes[PresentPointRecord::encoded_size] = {};
  CHECK_STATUS(PresentPointRecord(7u).encode(bytes, sizeof bytes), Status::ok);
  PresentPointRecord read(3u);
  CHECK_STATUS(read.decode(bytes, sizeof bytes - 1u), Status::malformed);
  CHECK_STATUS(read.decode(nullptr, sizeof bytes), Status::malformed);
  CHECK(read.point() == 3u);
  GuardedBuffer buffer(PresentPointRecord::encoded_size - 1u);
  CHECK_STATUS(PresentPointRecord(7u).encode(buffer.data(), buffer.size()), Status::too_large);
  CHECK(buffer.guards_intact()); }
