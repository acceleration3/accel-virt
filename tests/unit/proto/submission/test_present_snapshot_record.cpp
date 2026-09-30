/* A present snapshot's key as the user-mode driver hands it DdiPresent: the
 * fields at their offsets, a round trip, and private data that is anything
 * else - another magic, reserved bytes, no key, too few bytes - refused with
 * the reader unchanged. */
#include "core/support/status.hpp"
#include "harness/framework/byte_layout.hpp"
#include "harness/framework/guarded_buffer.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/submission/present_snapshot_record.hpp"

#include <cstddef>
#include <cstdint>

using av::ByteLayout;
using av::GuardedBuffer;
using av::PresentSnapshotRecord;
using av::Status;

TEST(present_snapshot_record, places_its_fields_and_reads_back_as_written)
{ std::uint8_t bytes[PresentSnapshotRecord::encoded_size] = {};
  CHECK_STATUS(PresentSnapshotRecord(0x8877665544332211ull).encode(bytes, sizeof bytes), Status::ok);
  const ByteLayout layout(bytes, sizeof bytes);
  CHECK(layout.at(0u, 4u) == 0x534B5652u);   /* "RVKS" */
  CHECK(layout.at(4u, 4u) == 0u);
  CHECK(layout.at(8u, 8u) == 0x8877665544332211ull);
  PresentSnapshotRecord read;
  CHECK(read.key() == 0u);
  CHECK_STATUS(read.decode(bytes, sizeof bytes), Status::ok);
  CHECK(read.key() == 0x8877665544332211ull); }

TEST(present_snapshot_record, anything_else_is_malformed_and_changes_nothing)
{ struct Case { std::size_t offset; std::size_t width; std::uint64_t value; };
  const Case cases[] = {{0u, 4u, 0x534B5653u}, {4u, 4u, 1u}, {8u, 8u, 0u}};
  for (const Case &c : cases) {
      std::uint8_t bytes[PresentSnapshotRecord::encoded_size] = {};
      CHECK_STATUS(PresentSnapshotRecord(7u).encode(bytes, sizeof bytes), Status::ok);
      for (std::size_t i = 0u; i < c.width; i++) bytes[c.offset + i] = static_cast<std::uint8_t>(c.value >> (8u * i));
      PresentSnapshotRecord read(3u);
      CHECK_STATUS(read.decode(bytes, sizeof bytes), Status::malformed);
      CHECK(read.key() == 3u);
  }
  std::uint8_t bytes[PresentSnapshotRecord::encoded_size] = {};
  CHECK_STATUS(PresentSnapshotRecord(7u).encode(bytes, sizeof bytes), Status::ok);
  PresentSnapshotRecord read(3u);
  CHECK_STATUS(read.decode(bytes, sizeof bytes - 1u), Status::malformed);
  CHECK_STATUS(read.decode(nullptr, sizeof bytes), Status::malformed);
  CHECK(read.key() == 3u);
  GuardedBuffer buffer(PresentSnapshotRecord::encoded_size - 1u);
  CHECK_STATUS(PresentSnapshotRecord(7u).encode(buffer.data(), buffer.size()), Status::too_large);
  CHECK(buffer.guards_intact()); }
