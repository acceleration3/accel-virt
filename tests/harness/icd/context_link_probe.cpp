#include "harness/icd/context_link_probe.hpp"

namespace av::icd {

ContextLinkProbe::ContextLinkProbe(ContextLink &target) : held(target) {}

uint32_t &ContextLinkProbe::link() { return held.link_; }

}  // namespace av::icd
