#include "virtq_segment.hpp"

namespace av {

VirtqSegment::VirtqSegment(uint64_t physical_address, uint32_t length, SegmentDirection direction)
    : physical_address_(physical_address), length_(length), direction_(direction) {}

uint64_t         VirtqSegment::physical_address() const { return physical_address_; }
uint32_t         VirtqSegment::length() const           { return length_; }
SegmentDirection VirtqSegment::direction() const        { return direction_; }

}  // namespace av
