/* The memory this adapter has, as VidMm is told about it: the aperture over
 * guest system memory, then the host GPU's local memory when the capset gives
 * it any. The device's hostmem region is not a segment: VidMm hands out the
 * aperture's staging pages when a process locks a CPU-visible allocation, so
 * the driver places chunks in that window itself (docs/host-window.md). It is
 * the only thing that knows how many segments the adapter reports, because
 * everything else that needs the number needs the same one. */
#ifndef AV_MEMORY_SEGMENTS_HPP
#define AV_MEMORY_SEGMENTS_HPP

#include "miniport/memory/local_memory_segment.hpp"
#include "core/platform/platform.hpp"
#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class AdapterInfoQuery;
class ApertureSegment;
class CapsetInfo;
struct DxgkQuerySegmentOut4Record;
struct MemorySegmentsProbe;

class MemorySegments {
public:
    MemorySegments(Platform &platform, const ApertureSegment &aperture);
    MemorySegments(const MemorySegments &) = delete;
    MemorySegments &operator=(const MemorySegments &) = delete;

    /* StartDevice's capset: the host GPU's local memory, a segment from
     * the next query on. */
    void     offer_local(const CapsetInfo &capset);
    /* StopDevice. */
    void     forget_local();
    uint32_t count() const;
    /* The segment VidMm appends after the last one reported
     * (CreatePhysicalAdapterSegments), which is where an allocation with no
     * segment of its own goes. */
    uint32_t system_memory_segment_id() const;
    /* DXGKQAITYPE_QUERYSEGMENT4: the count when no descriptor array is given,
     * otherwise each segment's descriptor. The paging buffer is the
     * aperture's. */
    [[nodiscard]] Status report(const AdapterInfoQuery &query) const;

    bool invariants_hold() const;

private:
    static constexpr uint32_t physical_adapter_bytes = 4u;   /* DXGK_QUERYSEGMENTIN4 */

    void describe(DxgkQuerySegmentOut4Record &out) const;

    Platform                 &platform_;
    const ApertureSegment    &aperture_;
    LocalMemorySegment        local_;

    friend struct MemorySegmentsProbe;
};

}  // namespace av

#endif  // AV_MEMORY_SEGMENTS_HPP
