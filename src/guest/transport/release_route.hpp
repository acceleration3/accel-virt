/* Where a transport that releases its batches by packet sends the packet
 * (ReleaseRecord): the device's owner submits it on a kernel context it
 * chooses - the one the runtime queued the waits its work depends on - so the
 * scheduler hands the batch over only after them. */
#ifndef AV_GUEST_RELEASE_ROUTE_HPP
#define AV_GUEST_RELEASE_ROUTE_HPP

#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class ReleaseRoute {
public:
    ReleaseRoute(const ReleaseRoute &) = delete;
    ReleaseRoute &operator=(const ReleaseRoute &) = delete;

    /* The packet whose private data is `record`, `size` bytes. not_supported
     * when there is no context to put it on right now: the batch then goes
     * through the kernel on its own context, still without publishing, and
     * takes its turn there (ReleaseQueue). Any other error fails the
     * transport. */
    [[nodiscard]] virtual Status release(const uint8_t *record, uint32_t size) = 0;

    /* A wait on the host for a released batch has lasted a few seconds: told
     * once per wait, for the route to say where it was noticed. */
    virtual void waited_long() = 0;

protected:
    ReleaseRoute() = default;
    ~ReleaseRoute() = default;
};

}  // namespace av

#endif  // AV_GUEST_RELEASE_ROUTE_HPP
