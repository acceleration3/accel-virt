#include "core/virtio/virtq_segment.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

VirtqSegment::VirtqSegment(uint64_t physical_address, uint32_t length, SegmentDirection direction)
    : physical_address_(physical_address), length_(length), direction_(direction)
    {
        AV_SCOPE("VirtqSegment::VirtqSegment");
    }

uint64_t         VirtqSegment::physical_address() const
{
    AV_SCOPE("VirtqSegment::physical_address");
    return physical_address_;
}
uint32_t         VirtqSegment::length() const
{
    AV_SCOPE("VirtqSegment::length");
    return length_;
}
SegmentDirection VirtqSegment::direction() const
{
    AV_SCOPE("VirtqSegment::direction");
    return direction_;
}

}  // namespace av
