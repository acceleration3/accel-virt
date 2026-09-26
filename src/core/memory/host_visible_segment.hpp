/* The CPU-visible memory segment a paravirtual adapter really has: the device's
 * hostmem region, which is a BAR the guest can address and into which the
 * device places host memory (docs/kb/04-memory.md).
 *
 * It is offered only where two things agree - the PCI capability walk found the
 * region, and the capset says the device has one (feature 039's
 * `feature_host_visible_memory`). An adapter without it reports the aperture
 * segment alone, and a host-visible allocation falls back to guest memory
 * (feature 041, FR-001 and FR-007). */
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
struct DxgkSegmentDescriptor4Record;

class HostVisibleSegment {
public:
    /* The aperture is segment 1; this is the one after it. */
    static constexpr uint32_t segment_id = 2u;
    static constexpr uint64_t page_bytes = 0x1000u;

    explicit HostVisibleSegment(Platform &platform);
    HostVisibleSegment(const HostVisibleSegment &) = delete;
    HostVisibleSegment &operator=(const HostVisibleSegment &) = delete;

    /* StartDevice: where the region is, if the device presents one. Shared
     * memory region 1 is virtio-gpu's host-visible region (VIRTIO_GPU_SHM_ID_
     * HOST_VISIBLE), and its guest physical base is the BAR firmware assigned
     * plus the region's offset in it. */
    void                 discover(const PciBaseAddresses &bars, const VirtioPciLayout &layout);
    /* What the device says it has: the segment is offered only where the region
     * was found and the capset claims it. */
    void                 offer(const CapsetInfo &capset);
    /* StopDevice. */
    void                 forget();

    bool                 offered() const;
    /* The region's guest physical base, and its size in bytes; zero until it is
     * offered. */
    uint64_t             base() const;
    uint64_t             size() const;
    /* Only meaningful once offered, which is what `MemorySegments` establishes
     * before it asks. */
    void                 describe(DxgkSegmentDescriptor4Record &descriptor) const;

    bool invariants_hold() const;

private:
    /* DXGK_SEGMENTFLAGS: CpuVisible | CacheCoherent. Never Aperture - the
     * segment has storage of its own. CacheCoherent because what backs the BAR
     * is host RAM the guest reaches through KVM's page tables, not device MMIO:
     * without it VidMm places no CPU-visible allocation here, and it is what
     * lets a Cached allocation be mapped write-back (10.7 GB/s written, 20 GB/s
     * read on a live guest, against 0.7 and 0.07 uncached). */
    static constexpr uint32_t flags = 0x14u;
    static constexpr uint8_t  host_visible_region = 1u;

    friend struct HostVisibleSegmentProbe;

    Platform &platform_;
    uint64_t  base_;
    uint64_t  size_;
    bool      offered_;
};

}  // namespace av

#endif  // AV_HOST_VISIBLE_SEGMENT_HPP
