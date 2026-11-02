#ifndef AV_VIRTQ_SEGMENT_HPP
#define AV_VIRTQ_SEGMENT_HPP

#include <stdint.h>

namespace av {

enum class SegmentDirection : uint8_t { device_reads, device_writes };

class VirtqSegment {
public:
    VirtqSegment(uint64_t physical_address, uint32_t length, SegmentDirection direction);

    uint64_t         physical_address() const;
    uint32_t         length() const;
    SegmentDirection direction() const;
    bool             continues_request() const;

private:
    uint64_t         physical_address_;
    uint32_t         length_;
    SegmentDirection direction_;
};

}

#endif
