#include "core/support/status.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "proto/wire/spill_block.hpp"
#include "proto/wire/wire_spill.hpp"
#include "proto/wire/wire_writer.hpp"

#include <cstddef>
#include <cstdint>

using av::MockPlatform;
using av::SpillBlock;
using av::Status;
using av::WireSpill;
using av::WireWriter;

TEST(spill_block, grows_from_a_lent_room_keeping_every_byte_written)
{ MockPlatform platform;
  SpillBlock   spill(platform);
  CHECK(spill.bytes() == nullptr);
  std::uint8_t room[16] = {};
  WireWriter   writer(room, sizeof room, spill);
  for (std::uint32_t index = 0u; index < 300000u; ++index) writer.write_u8(static_cast<std::uint8_t>(index * 7u));
  CHECK_STATUS(writer.status(), Status::ok);
  CHECK(writer.spilled());
  CHECK_EQ(writer.offset(), 300000u);
  CHECK(writer.bytes() == spill.bytes());
  CHECK(spill.capacity() >= 300000u);
  CHECK(spill.capacity() < 600000u);
  std::uint32_t wrong = 0u;
  for (std::uint32_t index = 0u; index < 300000u; ++index)
      if (spill.bytes()[index] != static_cast<std::uint8_t>(index * 7u)) wrong++;
  CHECK_EQ(wrong, 0u);
  CHECK(writer.invariants_hold());
  CHECK(spill.invariants_hold());
  spill.release();
  CHECK(spill.bytes() == nullptr);
  CHECK_EQ(spill.capacity(), 0u);
  CHECK(spill.invariants_hold()); }

TEST(spill_block, a_grow_copies_what_was_in_the_room)
{ MockPlatform platform;
  SpillBlock   spill(platform);
  const std::uint8_t written[4] = {1u, 2u, 3u, 4u};
  std::size_t        capacity = 0u;
  WireSpill         &as_spill = spill;
  std::uint8_t *const grown = as_spill.grow(written, sizeof written, 10u, capacity);
  CHECK(grown != nullptr);
  CHECK(capacity >= 10u);
  CHECK_EQ(grown[0], 1u);
  CHECK_EQ(grown[3], 4u);
  std::uint8_t *const again = spill.grow(grown, 4u, capacity + 1u, capacity);
  CHECK(again != nullptr);
  CHECK_EQ(again[2], 3u);
  CHECK(spill.grow(again, 8u, 4u, capacity) == nullptr); }

TEST(spill_block, no_memory_is_a_writer_that_latches_too_large)
{ MockPlatform platform;
  SpillBlock   spill(platform);
  platform.fail_allocation_at(1u);
  std::uint8_t room[4] = {};
  WireWriter   writer(room, sizeof room, spill);
  writer.write_u32(7u);
  writer.write_u8(1u);
  CHECK_STATUS(writer.status(), Status::too_large);
  CHECK(!writer.spilled());
  CHECK_EQ(writer.offset(), 4u);
  CHECK(writer.bytes() == room);
  SpillBlock  none;
  std::size_t capacity = 1u;
  CHECK(none.grow(room, 0u, 1u, capacity) == nullptr);
  CHECK_EQ(capacity, 0u); }
