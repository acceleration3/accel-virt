/* Commands framed into one block as the device reads a batch (FramedBlock):
 * each lent room after a header's worth, its header written in front of what
 * it wrote, and nothing framed that ran past the block. What OpenGL's and
 * OpenCL's batches, the Direct3D stream and a Vulkan command buffer all
 * record into. */
#include "core/support/status.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/icd/framed_block_probe.hpp"
#include "icd/common/framed_block.hpp"
#include "proto/submission/command_header.hpp"
#include "proto/wire/wire_writer.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

using av::CommandHeader;
using av::Status;
using av::WireWriter;
using av::icd::FramedBlock;
using av::icd::FramedBlockProbe;

namespace {

constexpr std::size_t   header = FramedBlock::header_bytes;
constexpr std::uint32_t first_id = 0x10001u;

/* The header framed at `at`, decoded. */
CommandHeader header_at(const FramedBlock &framed, std::size_t at)
{
    CommandHeader decoded;
    CHECK_STATUS(decoded.decode(framed.bytes() + at, header), Status::ok);
    return decoded;
}

}  // namespace

TEST(framed_block, a_null_block_holds_nothing_and_lends_nothing)
{ FramedBlock framed(nullptr, 4096u);
  CHECK_EQ(framed.capacity(), 0u);
  CHECK(!framed.fits(1u));
  CHECK(framed.fits(0u));
  WireWriter writer = framed.lend(64u);
  writer.write_u32(1u);
  CHECK(writer.status() != Status::ok);
  CHECK(!framed.frame(first_id, writer, 0u));
  CHECK_EQ(framed.used(), 0u);
  CHECK(framed.invariants_hold()); }

/* Each command lands after the last, its header in front of its body, and
 * the size the header carries counts both. */
TEST(framed_block, commands_are_framed_one_after_another)
{ std::uint8_t block[256] = {};
  FramedBlock  framed(block, sizeof block);
  WireWriter   one = framed.lend(64u);
  one.write_u32(0xAABBCCDDu);
  CHECK(framed.frame(first_id, one, 0u));
  CHECK_EQ(framed.used(), header + 4u);
  WireWriter two = framed.lend(64u);
  two.write_u64(7u);
  CHECK(framed.frame(first_id + 1u, two, CommandHeader::generate_reply));
  CHECK_EQ(framed.used(), 2u * header + 12u);
  const CommandHeader first = header_at(framed, 0u);
  CHECK_EQ(first.id(), first_id);
  CHECK_EQ(first.size(), header + 4u);
  CHECK(!first.generates_reply());
  const CommandHeader second = header_at(framed, header + 4u);
  CHECK_EQ(second.id(), first_id + 1u);
  CHECK_EQ(second.size(), header + 8u);
  CHECK(second.generates_reply());
  std::uint32_t body = 0u;
  std::memcpy(&body, framed.bytes() + header, sizeof body);
  CHECK_EQ(body, 0xAABBCCDDu);
  CHECK(framed.bytes() == block);
  CHECK(framed.invariants_hold()); }

/* A writer is lent the room asked for, or what is left past a header when
 * less is; a block without even a header's room lends nothing. */
TEST(framed_block, a_writer_is_lent_no_more_than_is_left)
{ std::uint8_t block[64] = {};
  FramedBlock  framed(block, sizeof block);
  WireWriter   asked = framed.lend(8u);
  asked.write_u64(1u);
  CHECK(asked.status() == Status::ok);
  asked.write_u32(1u);
  CHECK(asked.status() != Status::ok);
  WireWriter rest = framed.lend(1024u);
  for (std::size_t at = 0u; at < sizeof block - header; at++) rest.write_u8(1u);
  CHECK(rest.status() == Status::ok);
  CHECK(framed.frame(first_id, rest, 0u));
  CHECK(!framed.fits(1u));
  WireWriter none = framed.lend(8u);
  none.write_u8(1u);
  CHECK(none.status() != Status::ok);
  CHECK_EQ(framed.used(), sizeof block); }

/* A writer that failed is not framed, and neither is one that wrote past
 * what is left: nothing of either is in the block. */
TEST(framed_block, a_failed_writer_or_one_past_the_block_is_not_framed)
{ std::uint8_t block[64] = {};
  FramedBlock  framed(block, sizeof block);
  WireWriter   failed = framed.lend(4u);
  failed.write_u64(1u);
  CHECK(!framed.frame(first_id, failed, 0u));
  CHECK_EQ(framed.used(), 0u);
  /* A writer over room the block no longer has: another command was framed
   * since it was lent. */
  WireWriter late = framed.lend(32u);
  late.write_u64(1u);
  late.write_u64(2u);
  late.write_u64(3u);
  WireWriter first = framed.lend(32u);
  for (std::size_t at = 0u; at < 32u; at++) first.write_u8(1u);
  CHECK(framed.frame(first_id, first, 0u));
  CHECK(!framed.frame(first_id + 1u, late, 0u));
  CHECK_EQ(framed.used(), header + 32u);
  CHECK(framed.invariants_hold()); }

TEST(framed_block, commands_framed_elsewhere_are_appended_whole_or_not_at_all)
{ std::uint8_t block[32] = {};
  FramedBlock  framed(block, sizeof block);
  const std::uint8_t commands[24] = {1u, 2u, 3u};
  CHECK(framed.append(commands, sizeof commands));
  CHECK_EQ(framed.used(), sizeof commands);
  CHECK(std::memcmp(framed.bytes(), commands, sizeof commands) == 0);
  CHECK(!framed.append(commands, sizeof commands));
  CHECK_EQ(framed.used(), sizeof commands);
  CHECK(framed.append(nullptr, 0u));
  CHECK_EQ(framed.used(), sizeof commands); }

/* A block that moves keeps what was framed in it; one that is emptied frames
 * from its start again. */
TEST(framed_block, an_adopted_block_carries_on_and_a_cleared_one_starts_over)
{ std::uint8_t small[32] = {};
  std::uint8_t large[64] = {};
  FramedBlock  framed(small, sizeof small);
  const std::uint8_t commands[16] = {9u};
  CHECK(framed.append(commands, sizeof commands));
  std::memcpy(large, small, framed.used());
  framed.adopt(large, sizeof large);
  CHECK(framed.bytes() == large);
  CHECK_EQ(framed.capacity(), sizeof large);
  CHECK_EQ(framed.used(), sizeof commands);
  CHECK(framed.fits(sizeof large - sizeof commands));
  CHECK(!framed.fits(sizeof large - sizeof commands + 1u));
  framed.clear();
  CHECK_EQ(framed.used(), 0u);
  CHECK(framed.fits(sizeof large));
  framed.adopt(nullptr, sizeof large);
  CHECK_EQ(framed.capacity(), 0u);
  CHECK(framed.invariants_hold()); }

TEST(framed_block, the_invariant_catches_a_count_past_the_block_and_room_without_one)
{ std::uint8_t     block[32] = {};
  FramedBlock      framed(block, sizeof block);
  FramedBlockProbe probe(framed);
  probe.used() = sizeof block + 1u;
  CHECK(!framed.invariants_hold());
  probe.used() = sizeof block;
  CHECK(framed.invariants_hold());
  probe.block() = nullptr;
  CHECK(!framed.invariants_hold());
  probe.capacity() = 0u;
  probe.used() = 0u;
  CHECK(framed.invariants_hold()); }
