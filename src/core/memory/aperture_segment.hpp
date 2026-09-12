/* The one memory segment a WDDM 1.0-phase adapter on a stock virtio-gpu reports
 * to VidMm: an aperture over guest system memory, which the device reaches
 * through each allocation's pages. basicrender.sys reports the same shape. */
#ifndef AV_APERTURE_SEGMENT_HPP
#define AV_APERTURE_SEGMENT_HPP

#include "core/platform/platform.hpp"
#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class AdapterInfoQuery;
struct ApertureSegmentProbe;

class ApertureSegment {
public:
    static constexpr uint32_t segment_id = 1u;
    /* The system memory segment VidMm appends after this one for a GPU MMU
     * adapter (CreatePhysicalAdapterSegments). */
    static constexpr uint32_t system_memory_segment_id = segment_id + 1u;
    static constexpr uint64_t page_bytes = 0x1000u;

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
    [[nodiscard]] Status report(const AdapterInfoQuery &query) const;
    uint64_t             size() const;

    bool invariants_hold() const;

private:
    static constexpr uint32_t flags = 0x15u;                 /* Aperture | CpuVisible | CacheCoherent */
    static constexpr int64_t  base_address = 0xC0000000;     /* as basicrender.sys */
    static constexpr uint32_t physical_adapter_bytes = 4u;   /* DXGK_QUERYSEGMENTIN4 */

    Platform &platform_;
    uint64_t  size_;

    friend struct ApertureSegmentProbe;
};

}  // namespace av

#endif  // AV_APERTURE_SEGMENT_HPP
