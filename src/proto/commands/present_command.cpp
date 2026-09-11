#include "proto/commands/present_command.hpp"

#include "proto/wire/wire_writer.hpp"

namespace av {

PresentCommand::PresentCommand(PresentOperation operation, uint32_t color, const Rectangle &source,
                               const Rectangle &destination, uint32_t flip_interval, uint32_t sub_rectangles,
                               PresentAddressSpace addresses)
    : operation_(operation), color_(color), source_(source), destination_(destination),
      flip_interval_(flip_interval), sub_rectangles_(sub_rectangles), addresses_(addresses) {}

void PresentCommand::write(WireWriter &writer) const
{
    writer.write_u32(magic);
    writer.write_u32(static_cast<uint32_t>(operation_));
    writer.write_u32(color_);
    writer.write_u32(sub_rectangles_);
    source_.write(writer);
    destination_.write(writer);
    writer.write_u64(0u);
    writer.write_u64(0u);
    writer.write_u32(flip_interval_);
    writer.write_u32(static_cast<uint32_t>(addresses_));
}

}  // namespace av
