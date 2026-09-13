/* Reaches into a GuestRegion so invariant tests can corrupt it. Declared a
 * friend by GuestRegion; nothing outside the tests uses it. */
#ifndef AV_GUEST_REGION_PROBE_HPP
#define AV_GUEST_REGION_PROBE_HPP

#include "device/executor/guest_region.hpp"

#include <stdint.h>

namespace av {

struct GuestRegionProbe {
    explicit GuestRegionProbe(GuestRegion &target);

    void set_length(uint64_t length);

    GuestRegion &region;
};

}  // namespace av

#endif  // AV_GUEST_REGION_PROBE_HPP
