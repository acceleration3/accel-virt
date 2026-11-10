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

    void     offer_local(const CapsetInfo &capset);
    void     forget_local();
    uint32_t count() const;
    uint32_t system_memory_segment_id() const;
    uint32_t host_heap_segment_id(bool cpu_visible) const;
    [[nodiscard]] Status report(const AdapterInfoQuery &query) const;

    bool invariants_hold() const;

private:
    static constexpr uint32_t physical_adapter_bytes = 4u;

    void describe(DxgkQuerySegmentOut4Record &out) const;

    Platform                 &platform_;
    const ApertureSegment    &aperture_;
    LocalMemorySegment        local_;

    friend struct MemorySegmentsProbe;
};

}

#endif
