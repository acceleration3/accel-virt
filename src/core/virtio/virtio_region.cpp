#include "core/virtio/virtio_region.hpp"

namespace av {

VirtioRegion::VirtioRegion() : present_(false), bar_(0u), offset_(0u), length_(0u) {}

VirtioRegion::VirtioRegion(uint8_t bar, uint64_t offset, uint64_t length)
    : present_(true), bar_(bar), offset_(offset), length_(length) {}

bool     VirtioRegion::present() const { return present_; }
uint8_t  VirtioRegion::bar() const     { return bar_; }
uint64_t VirtioRegion::offset() const  { return offset_; }
uint64_t VirtioRegion::length() const  { return length_; }

}  // namespace av
