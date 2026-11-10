#ifndef AV_ALLOCATION_KIND_HPP
#define AV_ALLOCATION_KIND_HPP

#include <stdint.h>

namespace av {

enum class AllocationKind : uint16_t {
    shared_primary = 1u,
    shadow = 2u,
    staging = 3u,
    gdi = 4u,
    surface = 5u,
    cross_adapter = 6u,
    host_heap = 7u,
    client = 16u,
    context_ring = 17u,
};

}

#endif
