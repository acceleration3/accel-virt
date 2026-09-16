#include "harness/executor/importable_types_probe.hpp"

namespace av {

ImportableTypesProbe::ImportableTypesProbe(ImportableTypes &target) : types(target) {}

void ImportableTypesProbe::set_mask(uint32_t mask) { types.mask_ = mask; }

}  // namespace av
