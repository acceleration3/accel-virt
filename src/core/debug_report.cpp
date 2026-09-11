#include "debug_report.hpp"

#include "../proto/wire_writer.hpp"

namespace av {

DebugReport::DebugReport(Platform &platform, bool started, bool powered, uint64_t features)
    : platform_(platform), started_(started), powered_(powered), features_(features) {}

size_t DebugReport::write(uint8_t *buffer, size_t capacity) const
{
    platform_.require_irql_at_most(Irql::passive, "DebugReport::write");
    WireWriter writer(buffer, capacity);
    writer.write_u32(magic);
    writer.write_u16(format);
    writer.write_u8(started_ ? 1u : 0u);
    writer.write_u8(powered_ ? 1u : 0u);
    writer.write_u64(features_);
    return writer.offset();
}

}  // namespace av
