#ifndef AV_CONTEXT_LINK_PROBE_HPP
#define AV_CONTEXT_LINK_PROBE_HPP

#include "icd/common/context_link.hpp"

#include <stdint.h>

namespace av::icd {

struct ContextLinkProbe {
    explicit ContextLinkProbe(ContextLink &target);

    uint32_t &link();

    ContextLink &held;
};

}

#endif
