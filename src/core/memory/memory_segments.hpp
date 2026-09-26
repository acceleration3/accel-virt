/* The memory this adapter has, as VidMm is told about it: the aperture over
 * guest system memory, and nothing else. The device's hostmem region is not a
 * segment: VidMm hands out the aperture's staging pages when a process locks a
 * CPU-visible allocation, so the driver places chunks in that window itself
 * (docs/host-window.md). It is the only thing that knows how many segments the
 * adapter reports, because everything else that needs the number needs the
 * same one. */
#ifndef AV_MEMORY_SEGMENTS_HPP
#define AV_MEMORY_SEGMENTS_HPP

#include "core/platform/platform.hpp"
#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class AdapterInfoQuery;
class ApertureSegment;

class MemorySegments {
public:
    MemorySegments(Platform &platform, const ApertureSegment &aperture);
    MemorySegments(const MemorySegments &) = delete;
    MemorySegments &operator=(const MemorySegments &) = delete;

    uint32_t count() const;
    /* The segment VidMm appends after the last one reported
     * (CreatePhysicalAdapterSegments), which is where an allocation with no
     * segment of its own goes. */
    uint32_t system_memory_segment_id() const;
    /* DXGKQAITYPE_QUERYSEGMENT4: the count when no descriptor array is given,
     * otherwise the aperture's descriptor. The paging buffer is the
     * aperture's. */
    [[nodiscard]] Status report(const AdapterInfoQuery &query) const;

    bool invariants_hold() const;

private:
    static constexpr uint32_t physical_adapter_bytes = 4u;   /* DXGK_QUERYSEGMENTIN4 */

    static constexpr uint32_t segments = 1u;

    Platform              &platform_;
    const ApertureSegment &aperture_;
};

}  // namespace av

#endif  // AV_MEMORY_SEGMENTS_HPP
