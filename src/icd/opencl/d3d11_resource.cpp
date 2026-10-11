#include "icd/opencl/d3d11_resource.hpp"

#include "core/trace/function_scope.hpp"

namespace av::clicd {

namespace {

/* A mip level's extent is the top level's shifted right once per level; past
 * the width of the number every level is one texel. */
constexpr uint32_t widest_shift = 31u;

}  // namespace

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
    if (kind_ == Kind::texture2d) return mips_ != 0u && subresource / mips_ < layers_;
    return kind_ == Kind::texture3d && subresource < mips_;
}

uint32_t D3d11Resource::width_of(uint32_t subresource) const
{
    AV_SCOPE("D3d11Resource::width_of");
    return at_mip(width_, mip_of(subresource));
}

uint32_t D3d11Resource::height_of(uint32_t subresource) const
{
    AV_SCOPE("D3d11Resource::height_of");
    return at_mip(height_, mip_of(subresource));
}

uint32_t D3d11Resource::depth_of(uint32_t subresource) const
{
    AV_SCOPE("D3d11Resource::depth_of");
    return kind_ == Kind::texture3d ? at_mip(depth_, mip_of(subresource)) : 1u;
}

uint32_t D3d11Resource::mip_of(uint32_t subresource) const
{
    AV_SCOPE("D3d11Resource::mip_of");
    return mips_ != 0u ? subresource % mips_ : 0u;
}

uint32_t D3d11Resource::at_mip(uint32_t extent, uint32_t mip) const
{
    AV_SCOPE("D3d11Resource::at_mip");
    const uint32_t shifted = mip > widest_shift ? 0u : extent >> mip;
    return shifted != 0u ? shifted : 1u;
}

}  // namespace av::clicd
