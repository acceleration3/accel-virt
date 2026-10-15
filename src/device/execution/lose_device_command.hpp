/* LOSE_HOST_DEVICE, the transport command a test loses its context's host
 * device with (docs/protocol.md, "Losing the host device"): the executor
 * makes the device lost as a GPU fault would, and the answer says whether it
 * could. The context is not faulted here - the batch completes with its
 * answer - but at the worker's next poll, as every other context on the lost
 * device is, so a test sees exactly what a fault the host found by itself
 * does. */
#ifndef AV_LOSE_DEVICE_COMMAND_HPP
#define AV_LOSE_DEVICE_COMMAND_HPP

#include "core/support/status.hpp"
#include "proto/submission/command_header.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class ContextExecutor;

class LoseDeviceCommand {
public:
    LoseDeviceCommand(ContextExecutor &executor, const CommandHeader &header);

    /* Worker. malformed for a body, which no producer frames; otherwise ok
     * with the answer written - host_device_lost, or host_device_kept where
     * the host cannot lose one - and `produced` its length. A slot too
     * small is written nothing, as the executor ABI leaves it. */
    [[nodiscard]] Status run(uint8_t *reply, size_t capacity, size_t &produced);

private:
    ContextExecutor &executor_;
    CommandHeader    header_;
};

}  // namespace av

#endif  // AV_LOSE_DEVICE_COMMAND_HPP
