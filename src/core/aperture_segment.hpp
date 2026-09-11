/* The one memory segment a WDDM 1.0-phase adapter on a stock virtio-gpu reports
 * to VidMm: an aperture over guest system memory, which the device reaches
 * through each allocation's pages. basicrender.sys reports the same shape. */
#ifndef AV_APERTURE_SEGMENT_HPP
#define AV_APERTURE_SEGMENT_HPP

#include "platform.hpp"
#include "status.hpp"

#include <stdint.h>

namespace av {

class AdapterInfoQuery;
struct ApertureSegmentProbe;

class ApertureSegment {
public:
    static constexpr uint32_t segment_id = 1u;
    static constexpr uint64_t page_bytes = 0x1000u;

    explicit ApertureSegment(Platform &platform);
    ApertureSegment(const ApertureSegment &) = delete;
    ApertureSegment &operator=(const ApertureSegment &) = delete;

    /* DXGK_DEVICE_INFO.SystemMemorySize, rounded down to a page. */
    [[nodiscard]] Status initialize(int64_t system_memory_bytes);
    /* DXGKQAITYPE_QUERYSEGMENT4: the count when no descriptor array is given
     * (CreatePhysicalAdapterSegments' first pass), otherwise the descriptor. */
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
