#include "device/executor/command_refusal.hpp"

#include "core/trace/function_scope.hpp"

#include <stdio.h>

namespace av {

CommandRefusal::CommandRefusal(const char *api) : api_(api)
{
    AV_SCOPE("CommandRefusal::CommandRefusal");
}

bool CommandRefusal::removes(const char *command, uint32_t device, bool removed) const
{
    AV_SCOPE("CommandRefusal::removes");
    if (device == 0u) {
        fprintf(stderr, "virtio-gpu-av: %s: %s refused with no device selected; the session faults\n", api_,
                command);
        return false;
    }
    if (!removed) fprintf(stderr, "virtio-gpu-av: %s: %s refused; device %u is removed\n", api_, command, device);
    return true;
}

}  // namespace av
