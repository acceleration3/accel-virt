#include "icd/opencl/d3d11_resource.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/resources/mip_extent.hpp"
#include "proto/resources/subresource_layout.hpp"

namespace av::clicd {

D3d11Resource::D3d11Resource()
    : kind_(Kind::none), format_(0u), bytes_(0u), width_(0u), height_(0u), depth_(0u), mips_(0u), layers_(0u),
      samples_(0u)
{
    AV_SCOPE("D3d11Resource::D3d11Resource");
}

D3d11Resource::D3d11Resource(Kind kind, uint32_t format, uint64_t bytes, uint32_t width, uint32_t height,
                             uint32_t depth, uint32_t mips, uint32_t layers, uint32_t samples)
    : kind_(kind), format_(format), bytes_(bytes), width_(width), height_(height), depth_(depth), mips_(mips),
      layers_(layers), samples_(samples)
{
    AV_SCOPE("D3d11Resource::D3d11Resource");
}

D3d11Resource::Kind D3d11Resource::kind() const
{
    AV_SCOPE("D3d11Resource::kind");
    return kind_;
}

uint32_t D3d11Resource::format() const
{
    AV_SCOPE("D3d11Resource::format");
    return format_;
}

uint64_t D3d11Resource::bytes() const
{
    AV_SCOPE("D3d11Resource::bytes");
    return bytes_;
}

bool D3d11Resource::multisampled() const
{
    AV_SCOPE("D3d11Resource::multisampled");
    return samples_ > 1u;
}

bool D3d11Resource::holds(uint32_t subresource) const
{
    AV_SCOPE("D3d11Resource::holds");
    if (kind_ == Kind::buffer) return subresource == 0u;
    if (kind_ == Kind::texture2d) return SubresourceLayout(mips_, layers_, 1u).holds(subresource);
    return kind_ == Kind::texture3d && SubresourceLayout(mips_, 1u, 1u).holds(subresource);
}

uint32_t D3d11Resource::width_of(uint32_t subresource) const
{
    AV_SCOPE("D3d11Resource::width_of");
    return static_cast<uint32_t>(extent_of(subresource).width());
}

uint32_t D3d11Resource::height_of(uint32_t subresource) const
{
    AV_SCOPE("D3d11Resource::height_of");
    return extent_of(subresource).height();
}

uint32_t D3d11Resource::depth_of(uint32_t subresource) const
{
    AV_SCOPE("D3d11Resource::depth_of");
    return extent_of(subresource).depth();
}

MipExtent D3d11Resource::extent_of(uint32_t subresource) const
{
    AV_SCOPE("D3d11Resource::extent_of");
    return MipExtent(width_, height_, kind_ == Kind::texture3d ? depth_ : 1u,
                     SubresourceLayout(mips_, layers_, 1u).mip_of(subresource));
}

}  // namespace av::clicd
