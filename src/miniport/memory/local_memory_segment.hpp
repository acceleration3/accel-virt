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
    static constexpr uint32_t segment_id = 2u;

    explicit LocalMemorySegment(Platform &platform);
    LocalMemorySegment(const LocalMemorySegment &) = delete;
    LocalMemorySegment &operator=(const LocalMemorySegment &) = delete;

    void offer(const CapsetInfo &capset);
    void forget();
    bool present() const;
    void describe(DxgkSegmentDescriptor4Record &descriptor, int64_t base_address) const;
    uint64_t size() const;

    bool invariants_hold() const;

private:
    static constexpr uint64_t page_bytes = 0x1000u;

    Platform &platform_;
    uint64_t  size_;

    friend struct LocalMemorySegmentProbe;
};

}

#endif
