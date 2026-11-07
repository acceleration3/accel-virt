#ifndef AV_APERTURE_SEGMENT_HPP
#define AV_APERTURE_SEGMENT_HPP

#include "core/platform/platform.hpp"
#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class CapsetInfo;
struct DxgkSegmentDescriptor4Record;
struct ApertureSegmentProbe;

class ApertureSegment {
public:
    static constexpr uint32_t segment_id = 1u;
    static constexpr uint32_t segment_set = 1u << (segment_id - 1u);
    static constexpr uint64_t page_bytes = 0x1000u;
    static constexpr int64_t  base_address = 0xC0000000;

    explicit ApertureSegment(Platform &platform);
    ApertureSegment(const ApertureSegment &) = delete;
    ApertureSegment &operator=(const ApertureSegment &) = delete;

    [[nodiscard]] Status initialize(int64_t system_memory_bytes);
    void                 offer(const CapsetInfo &capset);
    void                 forget();
    void                 describe(DxgkSegmentDescriptor4Record &descriptor) const;
    uint64_t             size() const;
    uint64_t             commit_limit() const;

    bool invariants_hold() const;

private:
    static constexpr uint32_t flags = 0x415u;

    Platform &platform_;
    uint64_t  size_;
    uint64_t  host_budget_;

    friend struct ApertureSegmentProbe;
};

}

#endif
