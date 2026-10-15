/* One command framed as the pieces the device gathers back into it
 * (docs/protocol.md, "Long commands"). */
#include "core/support/status.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/submission/command_header.hpp"
#include "proto/submission/command_pieces.hpp"

#include <cstddef>
#include <cstdint>

using av::CommandHeader;
using av::CommandPieces;
using av::Status;

namespace {

constexpr std::uint32_t command = 0x00020021u;

}  // namespace

/* A payload that fits the first room is the command alone, its flags kept. */
TEST(command_pieces, a_payload_that_fits_is_the_command_itself)
{ const std::uint8_t body[3] = {9u, 8u, 7u};
  CommandPieces      pieces(command, CommandHeader::generate_reply, body, sizeof body);
  std::uint8_t       room[64] = {};
  CHECK_EQ(pieces.rest(), 3u);
  CHECK_EQ(pieces.frame_next(room, sizeof room), CommandHeader::encoded_size + 3u);
  CHECK(pieces.done());
  CHECK_EQ(pieces.pieces(), 0u);
  CHECK_EQ(pieces.rest(), 0u);
  CommandHeader header;
  CHECK_STATUS(header.decode(room, sizeof room), Status::ok);
  CHECK_EQ(header.id(), command);
  CHECK(header.generates_reply());
  CHECK_EQ(room[CommandHeader::encoded_size + 2u], 7u);
  /* Done is done. */
  CHECK_EQ(pieces.frame_next(room, sizeof room), 0u);
  CHECK(pieces.invariants_hold()); }

/* A longer payload: pieces as large as each room, in order, none asking
 * for an answer, and the command last with the rest and its own flags. */
TEST(command_pieces, a_longer_payload_is_pieces_then_the_command_with_the_rest)
{ std::uint8_t body[100] = {};
  for (std::uint32_t index = 0u; index < sizeof body; ++index) body[index] = static_cast<std::uint8_t>(index);
  CommandPieces pieces(command, CommandHeader::generate_reply, body, sizeof body);
  std::uint8_t  room[48] = {};
  std::uint8_t  gathered[sizeof body] = {};
  std::size_t   at = 0u;
  std::uint32_t framed_pieces = 0u;
  while (!pieces.done()) {
      const std::size_t framed = pieces.frame_next(room, sizeof room);
      CHECK(framed > CommandHeader::encoded_size);
      CommandHeader header;
      CHECK_STATUS(header.decode(room, framed), Status::ok);
      CHECK_EQ(header.size(), framed);
      if (!pieces.done()) {
          CHECK_EQ(header.id(), CommandHeader::piece_id);
          CHECK(!header.generates_reply());
          framed_pieces++;
      } else {
          CHECK_EQ(header.id(), command);
          CHECK(header.generates_reply());
      }
      for (std::size_t index = CommandHeader::encoded_size; index < framed; ++index) gathered[at++] = room[index];
  }
  CHECK_EQ(at, sizeof body);
  CHECK_EQ(framed_pieces, 2u);
  CHECK_EQ(pieces.pieces(), 2u);
  std::uint32_t wrong = 0u;
  for (std::uint32_t index = 0u; index < sizeof body; ++index)
      if (gathered[index] != body[index]) wrong++;
  CHECK_EQ(wrong, 0u);
  CHECK(pieces.invariants_hold()); }

/* A room with no byte past a header frames nothing, and no room at all. */
TEST(command_pieces, a_room_with_no_byte_past_a_header_frames_nothing)
{ const std::uint8_t body[20] = {};
  CommandPieces      pieces(command, 0u, body, sizeof body);
  std::uint8_t       room[CommandHeader::encoded_size] = {};
  CHECK_EQ(pieces.frame_next(room, sizeof room), 0u);
  CHECK_EQ(pieces.frame_next(nullptr, 64u), 0u);
  CHECK_EQ(pieces.frame_next(room, 3u), 0u);
  CHECK(!pieces.done());
  CHECK_EQ(pieces.rest(), 20u);
  /* An empty payload is a header alone, in a room of just a header. */
  CommandPieces empty(command, 0u, nullptr, 0u);
  CHECK_EQ(empty.frame_next(room, sizeof room), CommandHeader::encoded_size);
  CHECK(empty.done()); }
