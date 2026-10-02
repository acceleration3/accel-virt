#include "core/support/status.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/dma/present_command.hpp"
#include "proto/dma/present_operation.hpp"
#include "proto/dma/rectangle.hpp"
#include "proto/wire/wire_writer.hpp"

#include <cstdint>
#include <cstring>

using av::PresentCommand;
using av::PresentOperation;
using av::Rectangle;
using av::Status;
using av::WireWriter;

namespace {

std::uint32_t u32(const std::uint8_t *bytes, unsigned offset)
{
    return std::uint32_t{bytes[offset]} | std::uint32_t{bytes[offset + 1u]} << 8u |
           std::uint32_t{bytes[offset + 2u]} << 16u | std::uint32_t{bytes[offset + 3u]} << 24u;
}

}  // namespace

TEST(present_command_write, lays_out_the_documented_header)
{ const PresentCommand command(PresentOperation::color_fill, 0x00FF8040u, Rectangle(0, 0, 8, 8),
                               Rectangle(1, 2, 3, 4), 1u, 5u, av::PresentAddressSpace::physical);
  std::uint8_t bytes[PresentCommand::header_size + 1u];
  std::memset(bytes, 0xEE, sizeof bytes);
  WireWriter writer(bytes, PresentCommand::header_size);
  command.write(writer);
  CHECK_STATUS(writer.status(), Status::ok);
  CHECK_EQ(writer.offset(), PresentCommand::header_size);
  CHECK_EQ(u32(bytes, 0u), 0x00505641u);
  CHECK_EQ(u32(bytes, 4u), 2u);
  CHECK_EQ(u32(bytes, 8u), 0x00FF8040u);
  CHECK_EQ(u32(bytes, 12u), 5u);
  CHECK_EQ(u32(bytes, 24u), 8u);
  CHECK_EQ(u32(bytes, 32u), 1u);
  CHECK_EQ(u32(bytes, 44u), 4u);
  CHECK_EQ(u32(bytes, PresentCommand::source_address_offset), 0u);
  CHECK_EQ(u32(bytes, PresentCommand::destination_address_offset + 4u), 0u);
  CHECK_EQ(u32(bytes, 64u), 1u);
  CHECK_EQ(u32(bytes, 68u), 0u);
  CHECK_EQ(bytes[PresentCommand::header_size], 0xEEu); }

TEST(present_command_write, names_gpu_virtual_addresses)
{ const PresentCommand command(PresentOperation::blt, 0u, Rectangle(0, 0, 1, 1), Rectangle(0, 0, 1, 1), 0u, 0u,
                               av::PresentAddressSpace::gpu_virtual);
  std::uint8_t bytes[PresentCommand::header_size] = {};
  WireWriter writer(bytes, sizeof bytes);
  command.write(writer);
  CHECK_EQ(u32(bytes, 68u), 1u); }

TEST(present_command, operations_are_the_documented_values)
{ CHECK_EQ(static_cast<unsigned>(PresentOperation::blt), 1u);
  CHECK_EQ(static_cast<unsigned>(PresentOperation::color_fill), 2u);
  CHECK_EQ(static_cast<unsigned>(PresentOperation::flip), 3u);
  CHECK_EQ(PresentCommand::sub_rectangle_size, 16u); }
