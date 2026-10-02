#include "miniport/adapter/debug_report.hpp"

#include "miniport/scheduling/execution_node.hpp"
#include "core/trace/function_scope.hpp"
#include "proto/wire/wire_writer.hpp"

namespace av {

DebugReport::DebugReport(Platform &platform, bool started, bool powered, uint64_t features, ExecutionNode &node)
    : platform_(platform), started_(started), powered_(powered), features_(features), node_(node)
    {
        AV_SCOPE("DebugReport::DebugReport");
    }

size_t DebugReport::write(uint8_t *buffer, size_t capacity) const
{
    AV_SCOPE("DebugReport::write");
    platform_.require_irql_at_most(Irql::passive, "DebugReport::write");
    WireWriter writer(buffer, capacity);
    writer.write_u32(magic);
    writer.write_u16(format);
    writer.write_u8(started_ ? 1u : 0u);
    writer.write_u8(powered_ ? 1u : 0u);
    writer.write_u64(features_);
    writer.write_u32(node_.last_submitted());
    writer.write_u32(node_.current_fence());
    writer.write_u64(node_.progress_time());
    writer.write_u32(node_.pending());
    writer.write_zeros(4u);
    return writer.offset();
}

}  // namespace av
