#include "um/common/readable_extent.hpp"

#include "core/trace/function_scope.hpp"

#include <windows.h>

#include <stdint.h>

namespace av {

ReadableExtent::ReadableExtent(size_t most) : most_(most)
{
    AV_SCOPE("ReadableExtent::ReadableExtent");
}

size_t ReadableExtent::from(const void *start) const
{
    AV_SCOPE("ReadableExtent::from");
    size_t         total = 0u;
    const uint8_t *at = static_cast<const uint8_t *>(start);
    while (start != nullptr && total < most_) {
        MEMORY_BASIC_INFORMATION region = {};
        if (VirtualQuery(at, &region, sizeof region) == 0u || region.State != MEM_COMMIT ||
            (region.Protect & (PAGE_NOACCESS | PAGE_GUARD)) != 0u)
            break;
        const size_t left = static_cast<size_t>(static_cast<const uint8_t *>(region.BaseAddress) + region.RegionSize - at);
        total += left;
        at += left;
    }
    return total;
}

}
