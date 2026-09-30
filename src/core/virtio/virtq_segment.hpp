/* One buffer in a descriptor chain, as a device address and length. */
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
    /* Whether this can continue a request: bytes the device reads. */
    bool             continues_request() const;

private:
    uint64_t         physical_address_;
    uint32_t         length_;
    SegmentDirection direction_;
};

}  // namespace av

#endif  // AV_VIRTQ_SEGMENT_HPP
