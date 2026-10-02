/* Where the device's hostmem region is: a BAR the guest can address and into
 * which the device places host memory, which the driver hands out as its host
 * window (docs/host-window.md). VidMm is never told of it.
 *
 * It is offered only where two things agree - the PCI capability walk found the
 * region, and the capset says the device has one (feature 039's
 * `feature_host_visible_memory`). A device without it has no window, and every
 * chunk falls back to guest RAM. */
#ifndef AV_HOST_VISIBLE_SEGMENT_HPP
#define AV_HOST_VISIBLE_SEGMENT_HPP

#include "core/platform/platform.hpp"
#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

struct HostVisibleSegmentProbe;

class CapsetInfo;
class PciBaseAddresses;
class VirtioPciLayout;

class HostVisibleSegment {
public:
    static constexpr uint64_t page_bytes = 0x1000u;

    explicit HostVisibleSegment(Platform &platform);
    HostVisibleSegment(const HostVisibleSegment &) = delete;
    HostVisibleSegment &operator=(const HostVisibleSegment &) = delete;

    /* StartDevice: where the region is, if the device presents one. Shared
     * memory region 1 is virtio-gpu's host-visible region (VIRTIO_GPU_SHM_ID_
     * HOST_VISIBLE), and its guest physical base is the BAR firmware assigned
     * plus the region's offset in it. */
    void                 discover(const PciBaseAddresses &bars, const VirtioPciLayout &layout);
    /* What the device says it has: the region is offered only where it was
     * found and the capset claims it. */
    void                 offer(const CapsetInfo &capset);
    /* StopDevice. */
    void                 forget();

    bool                 offered() const;
    /* The region's guest physical base, and its size in bytes; zero until it is
     * offered. */
    uint64_t             base() const;
    uint64_t             size() const;

    bool invariants_hold() const;

private:
    static constexpr uint8_t  host_visible_region = 1u;

    friend struct HostVisibleSegmentProbe;

    Platform &platform_;
    uint64_t  base_;
    uint64_t  size_;
    bool      offered_;
};

}  // namespace av

#endif  // AV_HOST_VISIBLE_SEGMENT_HPP
