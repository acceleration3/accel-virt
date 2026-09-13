/* The memory this adapter has, as VidMm is told about it: the aperture over
 * guest system memory, and - where the device offers one - the CPU-visible
 * segment over its hostmem region. It is the only thing that knows how many
 * segments the adapter reports, because everything else that needs the number
 * needs the same one (feature 041, FR-001). */
#ifndef AV_MEMORY_SEGMENTS_HPP
#define AV_MEMORY_SEGMENTS_HPP

#include "core/platform/platform.hpp"
#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class AdapterInfoQuery;
class ApertureSegment;
class HostVisibleSegment;

class MemorySegments {
public:
    MemorySegments(Platform &platform, const ApertureSegment &aperture, const HostVisibleSegment &host_visible);
    MemorySegments(const MemorySegments &) = delete;
    MemorySegments &operator=(const MemorySegments &) = delete;

    uint32_t count() const;
    /* The segment VidMm appends after the last one reported
     * (CreatePhysicalAdapterSegments), which is where an allocation with no
     * segment of its own goes. */
    uint32_t system_memory_segment_id() const;
    /* DXGKQAITYPE_QUERYSEGMENT4: the count when no descriptor array is given,
     * otherwise every segment at the caller's stride. The paging buffer is the
     * aperture's, whatever else is reported. */
    [[nodiscard]] Status report(const AdapterInfoQuery &query) const;

    bool invariants_hold() const;

private:
    static constexpr uint32_t physical_adapter_bytes = 4u;   /* DXGK_QUERYSEGMENTIN4 */

    Platform                 &platform_;
    const ApertureSegment    &aperture_;
    const HostVisibleSegment &host_visible_;
};

}  // namespace av

#endif  // AV_MEMORY_SEGMENTS_HPP
