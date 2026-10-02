#include "device/executor/command_refusal.hpp"

#include "core/log/log_line.hpp"
#include "core/trace/function_scope.hpp"

namespace av {

CommandRefusal::CommandRefusal(DriverLog &lines) : lines_(lines)
{
    AV_SCOPE("CommandRefusal::CommandRefusal");
}

bool CommandRefusal::removes(const char *command, uint32_t device, bool removed) const
{
    AV_SCOPE("CommandRefusal::removes");
    if (device == 0u) {
        LogLine(lines_).text(command).text(" refused with no device selected; the session faults").write();
        return false;
    }
    if (!removed) LogLine(lines_).text(command).text(" refused; device ").number(device).text(" is removed").write();
    return true;
}

}  // namespace av
