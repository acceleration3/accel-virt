#include "harness/executor/share_index_probe.hpp"

namespace av {

ShareIndexProbe::ShareIndexProbe(const ShareIndex &target) : index(target) {}

uint32_t ShareIndexProbe::capacity() const { return index.capacity_; }

}  // namespace av
