#include "harness/framework/test_registry.hpp"
#include "harness/icd/cl_transfer_host.hpp"
#include "proto/client/transfer_room.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdlib>

using av::ClTransferHost;
using av::TransferRoom;

TEST(transfer_room, leaves_a_command_s_worth_of_each_buffer_and_nothing_when_it_is_smaller)
{ const TransferRoom room(10000u, 6000u);
  CHECK_EQ(room.inline_write(), 10000u - TransferRoom::command_bytes);
  CHECK_EQ(room.inline_read(), 6000u - TransferRoom::command_bytes);
  const TransferRoom tight(TransferRoom::command_bytes, 100u);
  CHECK_EQ(tight.inline_write(), 0u);
  CHECK_EQ(tight.inline_read(), 0u); }

TEST(transfer_room_inline_both, holds_only_what_both_directions_carry)
{ const TransferRoom room(10000u, 6000u);
  CHECK(room.inline_both(0u));
  CHECK(room.inline_both(room.inline_read()));
  CHECK(!room.inline_both(room.inline_read() + 1u));
  const TransferRoom wide(6000u, 10000u);
  CHECK(wide.inline_both(wide.inline_write()));
  CHECK(!wide.inline_both(wide.inline_write() + 1u)); }

TEST(transfer_room_write_piece, writes_what_the_staging_holds_until_the_wire_lends_memory)
{ ClTransferHost host;
  const TransferRoom room(10000u, 6000u);
  CHECK_EQ(room.write_piece(host, 100u), 100u);
  CHECK_EQ(room.write_piece(host, 1000000u), room.inline_write());
  const std::size_t bytes = std::size_t{8} << 20u;
  auto *const memory = static_cast<std::uint8_t *>(std::aligned_alloc(4096u, bytes));
  host.lend(memory, bytes);
  CHECK_EQ(room.write_piece(host, 100u), 100u);
  CHECK_EQ(room.write_piece(host, 1000000u), 1000000u);
  CHECK_EQ(room.write_piece(host, std::uint64_t{1} << 40u), TransferRoom::lent_bytes);
  CHECK(host.room_requests() >= 3u);
  host.lend(nullptr, 0u);
  std::free(memory); }

TEST(transfer_room_read_piece, reads_a_lent_piece_when_the_wire_lends_memory_and_the_reply_otherwise)
{ ClTransferHost host;
  const TransferRoom room(10000u, 6000u);
  CHECK_EQ(room.read_piece(host), room.inline_read());
  const std::size_t bytes = std::size_t{8} << 20u;
  auto *const memory = static_cast<std::uint8_t *>(std::aligned_alloc(4096u, bytes));
  host.lend(memory, bytes);
  CHECK_EQ(room.read_piece(host), TransferRoom::lent_bytes);
  host.lend(nullptr, 0u);
  std::free(memory); }
