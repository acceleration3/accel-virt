/* One window chunk's memory on the device: a host blob of host GPU memory,
 * made for one av context and placed in the hostmem region so the guest can
 * address it through the BAR (docs/host-memory.md) - or made over pages a
 * process lent, which the device turns into host GPU memory where they are
 * ("Application memory"), and never placed.
 *
 * The device is a parameter and not a member because it is not stable for this
 * object's lifetime: an allocation outlives a power transition, and the
 * VirtioGpuDevice it was created against is destroyed at D3 and built again at
 * D0. What this owns is what survives that - the resource id, where it is
 * placed, and whether the device holds a mapping for it. */
#ifndef AV_HOST_VISIBLE_BLOB_HPP
#define AV_HOST_VISIBLE_BLOB_HPP

#include "core/platform/platform.hpp"
#include "core/support/status.hpp"
#include "proto/resources/host_memory_request.hpp"

#include <stdint.h>

namespace av {

class VirtioGpuDevice;
struct HostVisibleBlobProbe;

class HostVisibleBlob {
public:
    static constexpr uint64_t page_bytes = 0x1000u;
    /* A piece's memory entries fill no more than the control queue's wide
     * slot, one per page at worst. */
    static constexpr uint32_t pages_per_piece = 2048u;

    explicit HostVisibleBlob(Platform &platform);
    /* The blob of `resource`, which this driver created and the device mapped
     * at `offset`: what a chunk remembers of it, to take it down again. */
    HostVisibleBlob(Platform &platform, uint32_t resource, uint64_t offset);
    /* The blob of `resource`, which this driver created over lent pages and
     * never placed. */
    HostVisibleBlob(Platform &platform, uint32_t resource);
    HostVisibleBlob(const HostVisibleBlob &) = delete;
    HostVisibleBlob &operator=(const HostVisibleBlob &) = delete;

    /* A mappable blob of `bytes` of the memory `memory` names, for the
     * device's context `context_id`. invalid_parameter for no context, no
     * resource, no memory, no bytes or a size that is not a page multiple;
     * invalid_state if this already holds one. A create the device did not
     * answer (not_found) counts as created: the device may still make it, and
     * only an unref can tell it to let go of the id. */
    [[nodiscard]] Status create(VirtioGpuDevice &device, uint32_t context_id, uint32_t resource_id,
                                HostMemoryRequest memory, uint64_t bytes);
    /* A blob of the memory `memory` names over the `bytes` of process pages
     * `locked` describes (Platform::lock_process_pages), for the device's
     * context `context_id`: HOST3D_GUEST, then the pages in order as
     * RESOURCE_ATTACH_BACKING pieces of at most pages_per_piece pages. As
     * create, and no_memory when no room for a piece can be had; a blob the
     * device made counts as created whichever piece failed. */
    [[nodiscard]] Status create_over(VirtioGpuDevice &device, uint32_t context_id, uint32_t resource_id,
                                     HostMemoryRequest memory, void *locked, uint64_t bytes);
    /* Placed at `offset` of the region. The device's answer is judged: only
     * OK_MAP_INFO with a caching this driver can give a process counts as
     * mapped, and caching() is then that caching. */
    [[nodiscard]] Status map(VirtioGpuDevice &device, uint64_t offset);
    [[nodiscard]] Status unmap(VirtioGpuDevice &device);
    /* Unmaps first if mapped, then unrefs: the device refuses to release a
     * resource still placed in its region. */
    [[nodiscard]] Status destroy(VirtioGpuDevice &device);
    /* The device that held it is gone - a power transition destroyed it, or it
     * turned out to be another device - so there is nothing to tell and
     * nothing left to hold. */
    void                 abandon();

    bool     created() const;
    bool     mapped() const;
    uint32_t resource() const;
    /* How the device said a view of it must be cached; write-back until
     * mapped. */
    PageCaching caching() const;
    /* Where in the region it is mapped; zero when it is not. */
    uint64_t offset() const;

    bool invariants_hold() const;

private:
    static constexpr uint32_t map_cache_cached = 1u;   /* VIRTIO_GPU_MAP_CACHE_CACHED */
    static constexpr uint32_t map_cache_wc = 3u;       /* VIRTIO_GPU_MAP_CACHE_WC */
    /* The largest of the three requests this sends is the 56-byte create, so a
     * writer over `request_` cannot refuse one and its answer is not a branch
     * anybody can take. */
    static constexpr size_t   request_bytes = 64u;
    static constexpr size_t   answer_bytes = 64u;

    [[nodiscard]] Status exchange(VirtioGpuDevice &device, const uint8_t *request, size_t length, uint32_t &map_info);
    /* The pages of `locked` as attach pieces, through `request` and `frames`. */
    [[nodiscard]] Status attach_pieces(VirtioGpuDevice &device, void *locked, uint64_t bytes, uint8_t *request,
                                       uint64_t *frames);

    Platform &platform_;
    uint32_t  resource_;
    uint64_t  offset_;
    bool      mapped_;
    PageCaching caching_;
    friend struct HostVisibleBlobProbe;

    uint8_t   request_[request_bytes];
    uint8_t   answer_[answer_bytes];
};

}  // namespace av

#endif  // AV_HOST_VISIBLE_BLOB_HPP
