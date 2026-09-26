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
    /* What an rvk context runs on, and what feature 043's transport gives a
     * process: a submission ring of a power-of-two page size, and the one page
     * a context's fences are written to. */
    context_ring = 17u,
    fence_page = 18u,
    /* The slot a context's answers are written to (feature 043). */
    reply_slot = 19u,
    /* Guest RAM the host GPU imports as host-visible Vulkan memory: a process
     * binds it to its rvk context and maps it with no copy (feature 045). */
    host_visible_memory = 20u,
    /* Host memory a process maps through the device's hostmem BAR: placed by
     * VidMm in the CPU-visible segment, whose storage is the host's. */
    host_memory = 21u,
};

}  // namespace av

#endif  // AV_ALLOCATION_KIND_HPP
