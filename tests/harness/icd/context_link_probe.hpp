#ifndef AV_CONTEXT_LINK_PROBE_HPP
#define AV_CONTEXT_LINK_PROBE_HPP

#include "icd/common/context_link.hpp"

#include <stdint.h>

namespace av::icd {

class DeviceLink;

struct ContextLinkProbe {
    explicit ContextLinkProbe(ContextLink &target);

    DeviceLink *&link();

    ContextLink &held;
};

}

#endif
