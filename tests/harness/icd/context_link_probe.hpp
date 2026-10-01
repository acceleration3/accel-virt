/* Reaches into a ContextLink so the invariant tests of the contexts that hold
 * one can name a link they never acquired, or forget one they did. Declared
 * a friend by ContextLink; nothing outside the tests uses it. */
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

}  // namespace av::icd

#endif  // AV_CONTEXT_LINK_PROBE_HPP
