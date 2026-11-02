#ifndef AV_GUEST_RELEASE_ROUTE_HPP
#define AV_GUEST_RELEASE_ROUTE_HPP

#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class ReleaseRoute {
public:
    ReleaseRoute(const ReleaseRoute &) = delete;
    ReleaseRoute &operator=(const ReleaseRoute &) = delete;

    [[nodiscard]] virtual Status release(const uint8_t *record, uint32_t size) = 0;

    virtual void waited_long() = 0;

protected:
    ReleaseRoute() = default;
    ~ReleaseRoute() = default;
};

}

#endif
