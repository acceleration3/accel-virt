#include "device/execution/lose_device_command.hpp"

#include "core/trace/function_scope.hpp"
#include "device/execution/context_executor.hpp"

namespace av {

LoseDeviceCommand::LoseDeviceCommand(ContextExecutor &executor, const CommandHeader &header)
    : executor_(executor), header_(header)
{
    AV_SCOPE("LoseDeviceCommand::LoseDeviceCommand");
}

Status LoseDeviceCommand::run(uint8_t *reply, size_t capacity, size_t &produced)
{
    AV_SCOPE("LoseDeviceCommand::run");
    produced = 0u;
    if (header_.size() != CommandHeader::encoded_size) return Status::malformed;
    const uint32_t status =
        executor_.lose_device() == Status::ok ? CommandHeader::host_device_lost : CommandHeader::host_device_kept;
    produced = CommandHeader::lose_host_device_answer;
    if (reply == nullptr || capacity < produced) return Status::ok;
    for (uint32_t byte = 0u; byte < produced; byte++)
        reply[byte] = byte < sizeof status ? static_cast<uint8_t>(status >> (8u * byte)) : 0u;
    return Status::ok;
}

}  // namespace av
