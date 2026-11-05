#include "device/execution/channel_stats_command.hpp"

#include "core/trace/function_scope.hpp"
#include "device/execution/context_executor.hpp"

namespace av {

ChannelStatsCommand::ChannelStatsCommand(ContextExecutor &executor, const CommandHeader &header, const uint8_t *body)
    : executor_(executor), header_(header), body_(body)
{
    AV_SCOPE("ChannelStatsCommand::ChannelStatsCommand");
}

Status ChannelStatsCommand::run(size_t &produced)
{
    AV_SCOPE("ChannelStatsCommand::run");
    produced = 0u;
    if (header_.generates_reply() || header_.size() < CommandHeader::encoded_size) return Status::malformed;
    executor_.channel_stats(body_, header_.size() - CommandHeader::encoded_size);
    return Status::ok;
}

}
