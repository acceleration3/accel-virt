#ifndef AV_SHARE_INDEX_PROBE_HPP
#define AV_SHARE_INDEX_PROBE_HPP

#include "executor/sharing/share_index.hpp"

#include <stdint.h>

namespace av {

struct ShareIndexProbe {
    explicit ShareIndexProbe(const ShareIndex &target);
    uint32_t buckets(uint32_t domain) const;
    uint32_t domains() const;

    const ShareIndex &index;
};

}

#endif
