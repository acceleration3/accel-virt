#include "proto/resources/mip_extent.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

MipExtent::MipExtent(uint64_t width, uint32_t height, uint32_t depth, uint32_t level)
    : width_(width), height_(height), depth_(depth), level_(level)
{
    AV_SCOPE("MipExtent::MipExtent");
}

uint64_t MipExtent::at_level(uint64_t edge) const
{
    AV_SCOPE("MipExtent::at_level");
    const uint64_t shifted = level_ < 64u ? edge >> level_ : 0u;
    return shifted != 0u ? shifted : 1u;
}

uint64_t MipExtent::width() const
{
    AV_SCOPE("MipExtent::width");
    return at_level(width_);
}

uint32_t MipExtent::height() const
{
    AV_SCOPE("MipExtent::height");
    return static_cast<uint32_t>(at_level(height_));
}

uint32_t MipExtent::depth() const
{
    AV_SCOPE("MipExtent::depth");
    return static_cast<uint32_t>(at_level(depth_));
}

}
