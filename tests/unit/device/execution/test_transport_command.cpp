/* The transport's own commands (docs/protocol.md, "Execution"): a NOP is
 * answered with nothing, a NOP carrying a body is framing no producer writes,
 * and any other transport id is one the device does not know. */
#include "core/support/status.hpp"
#include "device/execution/transport_command.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/submission/command_header.hpp"

#include <cstdint>

using av::CommandHeader;
using av::Status;
using av::TransportCommand;

TEST(transport_command_run, a_nop_without_a_body_runs_whether_or_not_it_asks_for_an_answer)
{ CHECK_EQ(CommandHeader::nop_id, 0x00000001u);
  CHECK_EQ(CommandHeader(CommandHeader::nop_id, 8u, 0u).api(), CommandHeader::transport_api);
  CHECK_STATUS(TransportCommand(CommandHeader(CommandHeader::nop_id, 8u, 0u)).run(), Status::ok);
  CHECK_STATUS(TransportCommand(CommandHeader(CommandHeader::nop_id, 8u, CommandHeader::generate_reply)).run(),
               Status::ok); }

TEST(transport_command_run, a_nop_of_any_size_but_its_header_is_malformed)
{ const std::uint32_t sizes[] = {0u, 1u, 7u, 9u, 15u, 16u, 0x1000u, CommandHeader::max_size - 1u,
                                 CommandHeader::max_size, 0xFFFFFFFFu};
  for (const std::uint32_t size : sizes) {
      CHECK_STATUS(TransportCommand(CommandHeader(CommandHeader::nop_id, size, 0u)).run(), Status::malformed);
      CHECK_STATUS(TransportCommand(CommandHeader(CommandHeader::nop_id, size, CommandHeader::generate_reply)).run(),
                   Status::malformed);
  } }

TEST(transport_command_run, a_transport_id_other_than_nop_is_not_known_whatever_its_size)
{ const std::uint32_t ids[] = {0x00000000u, 0x00000002u, 0x00000004u, 0x00000101u, 0x00008001u, 0x0000FFFFu};
  for (const std::uint32_t id : ids) {
      CHECK_STATUS(TransportCommand(CommandHeader(id, 8u, 0u)).run(), Status::not_found);
      CHECK_STATUS(TransportCommand(CommandHeader(id, 9u, CommandHeader::generate_reply)).run(), Status::not_found);
  } }

/* Every single-bit corruption of a NOP's framed header, decoded as the stream
 * decodes it: the outcome is decided per field by the contract, not read back
 * off the implementation. */
TEST(transport_command_run, every_bit_flip_of_a_nop_is_refused_skipped_or_still_a_nop)
{ std::uint8_t bytes[CommandHeader::encoded_size] = {};
  CHECK_STATUS(CommandHeader(CommandHeader::nop_id, 8u, 0u).encode(bytes, sizeof bytes), Status::ok);
  for (std::uint32_t bit = 0u; bit < 64u; bit++) {
      std::uint8_t flipped[CommandHeader::encoded_size] = {};
      for (std::uint32_t i = 0u; i < sizeof flipped; i++) flipped[i] = bytes[i];
      flipped[bit / 8u] = static_cast<std::uint8_t>(flipped[bit / 8u] ^ (1u << (bit % 8u)));
      CommandHeader header;
      const Status  decoded = header.decode(flipped, sizeof flipped);
      if (bit == 35u || bit > 56u) {
          CHECK_STATUS(decoded, Status::malformed);   /* a size of 0, or a flag nobody defined */
          continue;
      }
      CHECK_STATUS(decoded, Status::ok);
      if (bit >= 16u && bit < 32u) {
          CHECK(header.api() != CommandHeader::transport_api);   /* another API's command: never a transport one */
          continue;
      }
      const Status expected = bit < 16u ? Status::not_found : bit == 56u ? Status::ok : Status::malformed;
      CHECK_STATUS(TransportCommand(header).run(), expected);
  } }
