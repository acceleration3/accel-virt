/* The escape naming a surface's next present point: the fields at their
 * offsets, a round trip, and anything else - another magic, a point or surface
 * of 0, another length - refused with the reader unchanged. */
#include "core/support/status.hpp"
#include "harness/framework/byte_layout.hpp"
#include "harness/framework/guarded_buffer.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/escape/next_point_record.hpp"

#include <cstddef>
#include <cstdint>

using av::ByteLayout;
using av::GuardedBuffer;
using av::NextPointRecord;
using av::Status;

TEST(next_point_record, places_its_fields_and_reads_back_as_written)
{ std::uint8_t bytes[NextPointRecord::encoded_size] = {};
  CHECK_STATUS(NextPointRecord(0x1122334455667788ull, 0x99AABBCCu).encode(bytes, sizeof bytes), Status::ok);
  const ByteLayout layout(bytes, sizeof bytes);
  CHECK(layout.at(0u, 4u) == 0x004E5641u);   /* "AVN" */
  CHECK(layout.at(4u, 4u) == 0x99AABBCCu);
  CHECK(layout.at(8u, 8u) == 0x1122334455667788ull);
  NextPointRecord read;
  CHECK(read.surface() == 0u && read.point() == 0u);
  CHECK_STATUS(read.decode(bytes, sizeof bytes), Status::ok);
  CHECK(read.surface() == 0x1122334455667788ull);
  CHECK(read.point() == 0x99AABBCCu); }

TEST(next_point_record, anything_else_is_malformed_and_changes_nothing)
{ struct Case { std::size_t offset; std::size_t width; std::uint64_t value; };
  /* The present's own "AVP", a point of 0, a surface of 0. */
  const Case cases[] = {{0u, 4u, 0x00505641u}, {4u, 4u, 0u}, {8u, 8u, 0u}};
  for (const Case &c : cases) {
      std::uint8_t bytes[NextPointRecord::encoded_size] = {};
      CHECK_STATUS(NextPointRecord(9u, 7u).encode(bytes, sizeof bytes), Status::ok);
      for (std::size_t i = 0u; i < c.width; i++) bytes[c.offset + i] = static_cast<std::uint8_t>(c.value >> (8u * i));
      NextPointRecord read(5u, 3u);
      CHECK_STATUS(read.decode(bytes, sizeof bytes), Status::malformed);
      CHECK(read.surface() == 5u && read.point() == 3u);
  }
  std::uint8_t bytes[NextPointRecord::encoded_size + 1u] = {};
  CHECK_STATUS(NextPointRecord(9u, 7u).encode(bytes, sizeof bytes), Status::ok);
  NextPointRecord read(5u, 3u);
  CHECK_STATUS(read.decode(bytes, NextPointRecord::encoded_size - 1u), Status::malformed);
  CHECK_STATUS(read.decode(bytes, NextPointRecord::encoded_size + 1u), Status::malformed);
  CHECK_STATUS(read.decode(nullptr, NextPointRecord::encoded_size), Status::malformed);
  CHECK(read.surface() == 5u && read.point() == 3u);
  GuardedBuffer buffer(NextPointRecord::encoded_size - 1u);
  CHECK_STATUS(NextPointRecord(9u, 7u).encode(buffer.data(), buffer.size()), Status::too_large);
  CHECK(buffer.guards_intact()); }
