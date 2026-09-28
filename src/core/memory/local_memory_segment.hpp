/* The host GPU's device-local memory as VidMm is told about it: a memory
 * segment the size the device's capset gives, which Windows counts as the
 * adapter's dedicated video memory - DXGI's DedicatedVideoMemory, dxdiag's
 * "Dedicated Memory", and the local budget an application sizes itself by.
 * VidMm counts a segment that way when it is neither Aperture nor
 * ReservedSysMem, nor PopulatedFromSystemMemory (VIDMM_GLOBAL::
 * GetTotalSegmentSize, dxgmms2.sys @ 14010636c), and makes a
 * VIDMM_MEMORY_SEGMENT for one without calling the driver
 * (InitializePhysicalAdapterSegments @ 140116728).
 *
 * Nothing is ever placed in it. The host makes every resource in its own
 * memory; every allocation names the aperture or system memory, page tables
 * live in system memory and DMA and paging buffers in the aperture, so VidMm
 * pages nothing in or out of it and no guest page stands behind it. */
#ifndef AV_LOCAL_MEMORY_SEGMENT_HPP
#define AV_LOCAL_MEMORY_SEGMENT_HPP

#include "core/platform/platform.hpp"

#include <stdint.h>

namespace av {

class CapsetInfo;
struct DxgkSegmentDescriptor4Record;
struct LocalMemorySegmentProbe;

class LocalMemorySegment {
public:
    /* The segment after the aperture, which is the only one before it. */
    static constexpr uint32_t segment_id = 2u;

    explicit LocalMemorySegment(Platform &platform);
    LocalMemorySegment(const LocalMemorySegment &) = delete;
    LocalMemorySegment &operator=(const LocalMemorySegment &) = delete;

    /* The capset's device-local memory, rounded down to a page: none when the
     * host GPU reports less than a page, and then no segment is described. */
    void offer(const CapsetInfo &capset);
    /* The adapter stopped: the next start reads the capset again. */
    void forget();
    bool present() const;
    /* The descriptor VidMm is given, placed at `base_address` so its range
     * follows the aperture's rather than overlapping it. */
    void describe(DxgkSegmentDescriptor4Record &descriptor, int64_t base_address) const;
    uint64_t size() const;

    bool invariants_hold() const;

private:
    static constexpr uint64_t page_bytes = 0x1000u;

    Platform &platform_;
    uint64_t  size_;

    friend struct LocalMemorySegmentProbe;
};

}  // namespace av

#endif  // AV_LOCAL_MEMORY_SEGMENT_HPP
