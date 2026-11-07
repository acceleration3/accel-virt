#include "executor/d3d/d3d12_subresource_extent.hpp"

#include "core/trace/function_scope.hpp"
#include "executor/shim/d3d_headers.hpp"
#include "proto/d3d/dxgi_format_layout.hpp"
#include "proto/resources/mip_extent.hpp"
#include "proto/resources/subresource_layout.hpp"

namespace av {

namespace {

constexpr uint32_t planar_formats[] = {19u, 20u, 44u, 45u};

uint32_t planes_of(uint32_t format)
{
    AV_SCOPE("planes_of");
    for (const uint32_t planar : planar_formats)
        if (format == planar) return 2u;
    return 1u;
}

uint64_t rounded(uint64_t edge, uint32_t block)
{
    AV_SCOPE("rounded");
    return (edge + block - 1u) / block * block;
}

}

D3d12SubresourceExtent::D3d12SubresourceExtent(const D3D12_RESOURCE_DESC &desc, uint32_t subresource)
    : whole_{}, block_width_(1u), block_height_(1u), known_(false)
{
    AV_SCOPE("D3d12SubresourceExtent::D3d12SubresourceExtent");
    if (desc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER || desc.Dimension == D3D12_RESOURCE_DIMENSION_UNKNOWN) return;
    const DxgiFormatLayout layout(static_cast<uint32_t>(desc.Format));
    if (!layout.known()) return;
    const bool              volume = desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D;
    const uint32_t          mips = desc.MipLevels == 0u ? 1u : desc.MipLevels;
    const uint32_t          layers = volume ? 1u : desc.DepthOrArraySize;
    const SubresourceLayout subresources(mips, layers, planes_of(static_cast<uint32_t>(desc.Format)));
    if (!subresources.holds(subresource)) return;
    const MipExtent extent(desc.Width, desc.Height, volume ? desc.DepthOrArraySize : 1u,
                           subresources.mip_of(subresource));
    if (extent.width() > UINT32_MAX) return;
    block_width_ = layout.element_width();
    block_height_ = layout.element_height();
    whole_ = d3d12proto::Box{0u, 0u, 0u, static_cast<uint32_t>(extent.width()), extent.height(), extent.depth()};
    known_ = true;
}

bool D3d12SubresourceExtent::known() const
{
    AV_SCOPE("D3d12SubresourceExtent::known");
    return known_;
}

const d3d12proto::Box &D3d12SubresourceExtent::whole() const
{
    AV_SCOPE("D3d12SubresourceExtent::whole");
    return whole_;
}

bool D3d12SubresourceExtent::holds(const d3d12proto::Box &region) const
{
    AV_SCOPE("D3d12SubresourceExtent::holds");
    return known_ && region.left <= region.right && region.top <= region.bottom && region.front <= region.back &&
           region.right <= rounded(whole_.right, block_width_) && region.bottom <= rounded(whole_.bottom, block_height_) &&
           region.back <= whole_.back;
}

}
