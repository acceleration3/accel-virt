/* One host-visible allocation's memory on the device: a blob of the host's own
 * pages, placed in the hostmem region so the guest can address it through the
 * BAR (docs/protocol.md).
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

#include <stdint.h>

namespace av {

class VirtioGpuDevice;
struct HostVisibleBlobProbe;

class HostVisibleBlob {
public:
    /* Above the cursor's and every scanout's, so a blob can never take a
     * resource the display is using. */
    static constexpr uint32_t first_resource_id = 0x1000u;
    static constexpr uint64_t page_bytes = 0x1000u;

    explicit HostVisibleBlob(Platform &platform);
    HostVisibleBlob(const HostVisibleBlob &) = delete;
    HostVisibleBlob &operator=(const HostVisibleBlob &) = delete;

    /* A mappable blob of `bytes` host pages. invalid_parameter for no resource,
     * no bytes or a size that is not a page multiple; invalid_state if this
     * already holds one. */
    [[nodiscard]] Status create(VirtioGpuDevice &device, uint32_t resource_id, uint64_t bytes);
    /* Placed at `offset` of the region. The device's answer is judged: only
     * OK_MAP_INFO with the caching it promised counts as mapped. */
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
    /* Where in the region it is mapped; zero when it is not. */
    uint64_t offset() const;

    bool invariants_hold() const;

private:
    static constexpr uint32_t map_cache_cached = 1u;   /* VIRTIO_GPU_MAP_CACHE_CACHED */
    /* The largest of the three requests this sends is the 56-byte create, so a
     * writer over `request_` cannot refuse one and its answer is not a branch
     * anybody can take. */
    static constexpr size_t   request_bytes = 64u;
    static constexpr size_t   answer_bytes = 64u;

    [[nodiscard]] Status exchange(VirtioGpuDevice &device, size_t length, uint32_t &map_info);

    Platform &platform_;
    uint32_t  resource_;
    uint64_t  offset_;
    bool      mapped_;
    friend struct HostVisibleBlobProbe;

    uint8_t   request_[request_bytes];
    uint8_t   answer_[answer_bytes];
};

}  // namespace av

#endif  // AV_HOST_VISIBLE_BLOB_HPP
