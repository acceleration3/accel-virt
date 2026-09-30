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
    /* A user-mode driver's surface the kernel must reach: one presented,
     * blitted by the miniport or shared with another process, whose pages the
     * host's pixels are read back into (the Direct3D driver, rvkumd). */
    surface = 5u,
    client = 16u,
    /* The page that names an rvk context: the one handle of a context a driver
     * resolves in the process that made it, so another context of that process
     * names it by this (feature 050). No device reads it - a context runs on
     * memory the driver owns (docs/protocol.md, "Context memory"). */
    context_ring = 17u,
};

}  // namespace av

#endif  // AV_ALLOCATION_KIND_HPP
