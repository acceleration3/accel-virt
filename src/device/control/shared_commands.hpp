/* The driver's own control command that a shared object is gone from the
 * guest (SharedReleaseRequest): what was shared under the id goes from the
 * contexts that hold it, on this device's host GPU. A request that does not
 * decode is refused; one naming an id nothing holds is answered all the same,
 * because a release is idempotent. Main loop. */
#ifndef AV_SHARED_COMMANDS_HPP
#define AV_SHARED_COMMANDS_HPP

#include <stdint.h>

namespace av {

class ContextPlane;
class ControlRequest;
class Host;

class SharedCommands {
public:
    /* `gpu_uuid` is this device's host GPU, what the executor's copy is on. */
    SharedCommands(Host &host, ContextPlane &contexts, const uint8_t *gpu_uuid);
    SharedCommands(const SharedCommands &) = delete;
    SharedCommands &operator=(const SharedCommands &) = delete;

    /* Whether the core answered `request`; false leaves it to the base class. */
    bool answer(ControlRequest &request, uint32_t type);

private:
    Host          &host_;
    ContextPlane  &contexts_;
    const uint8_t *gpu_uuid_;
};

}  // namespace av

#endif  // AV_SHARED_COMMANDS_HPP
