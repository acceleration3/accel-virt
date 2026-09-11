/* What an allocation is for. The first four are D3DKMDT_STANDARDALLOCATION_TYPE
 * values, the allocations the runtime creates without a user-mode driver. */
#ifndef AV_ALLOCATION_KIND_HPP
#define AV_ALLOCATION_KIND_HPP

#include <stdint.h>

namespace av {

enum class AllocationKind : uint16_t {
    shared_primary = 1u,
    shadow = 2u,
    staging = 3u,
    gdi = 4u,
    client = 16u,
};

}  // namespace av

#endif  // AV_ALLOCATION_KIND_HPP
