/* The first segment the adapter reports to VidMm: an aperture over guest
 * system memory, which the device reaches through each allocation's pages.
 * basicrender.sys reports the same shape as its only segment. */
#ifndef AV_APERTURE_SEGMENT_HPP
#define AV_APERTURE_SEGMENT_HPP

#include "core/platform/platform.hpp"
#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

struct DxgkSegmentDescriptor4Record;
struct ApertureSegmentProbe;

class ApertureSegment {
public:
    static constexpr uint32_t segment_id = 1u;
    /* The same segment as a DXGK_CONTEXTINFO.DmaBufferSegmentSet bit, which is
     * one per segment counting from identifier one. basicrender.sys answers 1,
     * its only segment; an empty set leaves VidMmInitDmaPool (dxgmms2.sys
     * @1400c7990) with no segment to take a DMA buffer from. */
    static constexpr uint32_t segment_set = 1u << (segment_id - 1u);
    static constexpr uint64_t page_bytes = 0x1000u;
    /* Where the segment starts, as basicrender.sys reports it: the addresses
     * dxgkrnl hands back for an allocation in it are offsets from here. */
    static constexpr int64_t  base_address = 0xC0000000;

    explicit ApertureSegment(Platform &platform);
    ApertureSegment(const ApertureSegment &) = delete;
    ApertureSegment &operator=(const ApertureSegment &) = delete;

    /* DXGK_DEVICE_INFO.SystemMemorySize, rounded down to a page. */
    [[nodiscard]] Status initialize(int64_t system_memory_bytes);
    /* DXGKQAITYPE_QUERYSEGMENT4: the count when no descriptor array is given
     * (CreatePhysicalAdapterSegments' first pass), otherwise the descriptor and
     * the paging buffer. The paging buffer names this segment, never zero:
     * segment identifiers are one-based, and VidMmInitDmaPool (dxgmms2.sys
     * @1400c7990) turns the identifier into a segment set and indexes the
     * adapter's segment pointers by bit position, so zero reaches a segment that
     * was never created. basicrender.sys answers 1 for the same reason. */
    /* The descriptor VidMm is given for this segment. `MemorySegments` is what
     * answers DXGKQAITYPE_QUERYSEGMENT4 with it, and what establishes first
     * that this segment has a size worth describing. */
    void                 describe(DxgkSegmentDescriptor4Record &descriptor) const;
    uint64_t             size() const;

    bool invariants_hold() const;

private:
    static constexpr uint32_t flags = 0x15u;   /* Aperture | CpuVisible | CacheCoherent */

    Platform &platform_;
    uint64_t  size_;

    friend struct ApertureSegmentProbe;
};

}  // namespace av

#endif  // AV_APERTURE_SEGMENT_HPP
