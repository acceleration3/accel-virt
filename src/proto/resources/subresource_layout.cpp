#include "proto/resources/subresource_layout.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

SubresourceLayout::SubresourceLayout(uint32_t mips, uint32_t layers, uint32_t planes)
    : mips_(mips), layers_(layers), count_(0u)
{
    AV_SCOPE("SubresourceLayout::SubresourceLayout");
    uint32_t per_plane = 0u;
    if (__builtin_mul_overflow(mips, layers, &per_plane) || __builtin_mul_overflow(per_plane, planes, &count_))
        count_ = 0u;
}

uint32_t SubresourceLayout::count() const
{
    AV_SCOPE("SubresourceLayout::count");
    return count_;
}

bool SubresourceLayout::holds(uint32_t subresource) const
{
    AV_SCOPE("SubresourceLayout::holds");
    return subresource < count_;
}

uint32_t SubresourceLayout::mip_of(uint32_t subresource) const
{
    AV_SCOPE("SubresourceLayout::mip_of");
    return mips_ != 0u ? subresource % mips_ : 0u;
}

uint32_t SubresourceLayout::layer_of(uint32_t subresource) const
{
    AV_SCOPE("SubresourceLayout::layer_of");
    return mips_ != 0u && layers_ != 0u ? subresource / mips_ % layers_ : 0u;
}

}  // namespace av
