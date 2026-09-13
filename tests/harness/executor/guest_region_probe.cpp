#include "harness/executor/guest_region_probe.hpp"

namespace av {

GuestRegionProbe::GuestRegionProbe(GuestRegion &target) : region(target) {}

void GuestRegionProbe::set_length(uint64_t length) { region.length_ = length; }

}  // namespace av
