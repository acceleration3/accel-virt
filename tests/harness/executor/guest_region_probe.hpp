#ifndef AV_GUEST_REGION_PROBE_HPP
#define AV_GUEST_REGION_PROBE_HPP

#include "executor/guest_region.hpp"

#include <stdint.h>

namespace av {

struct GuestRegionProbe {
    explicit GuestRegionProbe(GuestRegion &target);

    void set_length(uint64_t length);
    void forget_base();

    GuestRegion &region;
};

}

#endif
