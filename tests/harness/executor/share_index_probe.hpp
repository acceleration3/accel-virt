/* Reaches into a ShareIndex so a test can see how much room it holds.
 * Declared a friend by ShareIndex; nothing outside the tests uses it. */
#ifndef AV_SHARE_INDEX_PROBE_HPP
#define AV_SHARE_INDEX_PROBE_HPP

#include "executor/sharing/share_index.hpp"

#include <stdint.h>

namespace av {

struct ShareIndexProbe {
    explicit ShareIndexProbe(const ShareIndex &target);
    /* The buckets of `domain`'s table; 0 for a domain with none. */
    uint32_t buckets(uint32_t domain) const;
    /* The domains a table is held for. */
    uint32_t domains() const;

    const ShareIndex &index;
};

}  // namespace av

#endif  // AV_SHARE_INDEX_PROBE_HPP
