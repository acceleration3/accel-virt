#include "core/virtio/virtio_region.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

VirtioRegion::VirtioRegion() : present_(false), bar_(0u), offset_(0u), length_(0u)
{
    AV_SCOPE("VirtioRegion::VirtioRegion");
}

VirtioRegion::VirtioRegion(uint8_t bar, uint64_t offset, uint64_t length)
    : present_(true), bar_(bar), offset_(offset), length_(length)
    {
        AV_SCOPE("VirtioRegion::VirtioRegion");
    }

bool     VirtioRegion::present() const
{
    AV_SCOPE("VirtioRegion::present");
    return present_;
}
uint8_t  VirtioRegion::bar() const
{
    AV_SCOPE("VirtioRegion::bar");
    return bar_;
}
uint64_t VirtioRegion::offset() const
{
    AV_SCOPE("VirtioRegion::offset");
    return offset_;
}
uint64_t VirtioRegion::length() const
{
    AV_SCOPE("VirtioRegion::length");
    return length_;
}

}  // namespace av
