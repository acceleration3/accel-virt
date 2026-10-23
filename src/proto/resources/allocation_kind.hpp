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
     * host's pixels are read back into (the Direct3D driver, avumd). */
    surface = 5u,
    /* A row-major texture or heap two adapters' devices share
     * (D3DKMDT_GDISURFACETYPE_TEXTURE_CROSSADAPTER, a shared-cross-adapter
     * D3D12 heap): pitch and rows as dxgkrnl lays them out
     * (CrossAdapterLayout), in the process's pages, which a host GPU reads
     * where they are (docs/host-memory.md, "Application memory"). Made by the
     * runtime without a user-mode driver as well as by one. */
    cross_adapter = 6u,
    client = 16u,
    /* The page that names an av context: the one handle of a context a driver
     * resolves in the process that made it, so another context of that process
     * names it by this (feature 050). No device reads it - a context runs on
     * memory the driver owns (docs/protocol.md, "Context memory"). */
    context_ring = 17u,
};

}  // namespace av

#endif  // AV_ALLOCATION_KIND_HPP
