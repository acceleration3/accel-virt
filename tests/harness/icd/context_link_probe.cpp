#include "harness/icd/context_link_probe.hpp"

namespace av::icd {

ContextLinkProbe::ContextLinkProbe(ContextLink &target) : held(target) {}

DeviceLink *&ContextLinkProbe::link() { return held.link_; }

}
