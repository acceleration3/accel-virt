#include "core/support/status.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/submission/command_header.hpp"
#include "proto/submission/command_pieces.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace av {

struct CommandPiecesProbe {
    static void set_framed(CommandPieces &pieces, std::size_t framed) { pieces.framed_ = framed; }
    static void set_done(CommandPieces &pieces, bool done) { pieces.done_ = done; }
};

}

using av::CommandHeader;
using av::CommandPiecesProbe;
using av::CommandPieces;
using av::Status;

namespace {

constexpr std::uint32_t command = 0x00020021u;

}

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
  CHECK_EQ(pieces.frame_next(room, sizeof room), 0u);
  CHECK(pieces.invariants_hold()); }

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

TEST(command_pieces, a_room_with_no_byte_past_a_header_frames_nothing)
{ const std::uint8_t body[20] = {};
  CommandPieces      pieces(command, 0u, body, sizeof body);
  std::uint8_t       room[CommandHeader::encoded_size] = {};
  CHECK_EQ(pieces.frame_next(room, sizeof room), 0u);
  CHECK_EQ(pieces.frame_next(nullptr, 64u), 0u);
  CHECK_EQ(pieces.frame_next(room, 3u), 0u);
  CHECK(!pieces.done());
  CHECK_EQ(pieces.rest(), 20u);
  CommandPieces empty(command, 0u, nullptr, 0u);
  CHECK_EQ(empty.frame_next(room, sizeof room), CommandHeader::encoded_size);
  CHECK(empty.done()); }

TEST(command_pieces, a_piece_is_never_framed_larger_than_a_command_may_be)
{ const std::size_t        most_body = CommandHeader::max_size - CommandHeader::encoded_size;
  const std::vector<std::uint8_t> body(most_body + 10u, 0x5Au);
  std::vector<std::uint8_t>       room(std::size_t{CommandHeader::max_size} + 64u, 0u);
  CommandPieces                   pieces(command, 0u, body.data(), body.size());
  CHECK_EQ(pieces.frame_next(room.data(), room.size()), std::size_t{CommandHeader::max_size});
  CommandHeader header;
  CHECK_STATUS(header.decode(room.data(), CommandHeader::encoded_size), Status::ok);
  CHECK_EQ(header.size(), CommandHeader::max_size);
  CHECK_EQ(header.id(), CommandHeader::piece_id);
  CHECK(!pieces.done());
  CHECK_EQ(pieces.rest(), 10u);
  CHECK_EQ(pieces.pieces(), 1u);
  CHECK(pieces.invariants_hold());
  CHECK_EQ(pieces.frame_next(room.data(), room.size()), CommandHeader::encoded_size + 10u);
  CHECK(pieces.done());
  CHECK(pieces.invariants_hold()); }

TEST(command_pieces, the_invariant_catches_a_frame_past_the_payload_an_unfinished_done_and_a_missing_body)
{ const std::uint8_t body[3] = {1u, 2u, 3u};
  CommandPieces      pieces(command, 0u, body, sizeof body);
  CHECK(pieces.invariants_hold());
  CommandPiecesProbe::set_framed(pieces, 4u);
  CHECK(!pieces.invariants_hold());
  CommandPiecesProbe::set_framed(pieces, 1u);
  CommandPiecesProbe::set_done(pieces, true);
  CHECK(!pieces.invariants_hold());
  CommandPiecesProbe::set_framed(pieces, 3u);
  CHECK(pieces.invariants_hold());
  const CommandPieces empty(command, 0u, nullptr, 0u);
  CHECK(empty.invariants_hold());
  const CommandPieces missing(command, 0u, nullptr, 5u);
  CHECK(!missing.invariants_hold()); }
