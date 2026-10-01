#include "device/executor/d3d12_copy_extent.hpp"

#include "core/trace/function_scope.hpp"
#include "device/shim/d3d_headers.hpp"

namespace av {

namespace {

/* A depth-stencil format's stencil is a second plane of subresources. */
constexpr uint64_t most_planes = 2u;

}  // namespace

D3d12CopyExtent::D3d12CopyExtent(const D3D12_TEXTURE_COPY_LOCATION &source, const d3d12proto::Box *box)
    : read_{}, known_(true)
{
    AV_SCOPE("D3d12CopyExtent::D3d12CopyExtent");
    if (source.Type == D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT) {
        const D3D12_SUBRESOURCE_FOOTPRINT &footprint = source.PlacedFootprint.Footprint;
        read_ = box != nullptr ? *box : d3d12proto::Box{0u, 0u, 0u, footprint.Width, footprint.Height, footprint.Depth};
        return;
    }
    const D3D12_RESOURCE_DESC desc = source.pResource->GetDesc();
    const bool                volume = desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D;
    const uint64_t            mips = desc.MipLevels == 0u ? 1u : desc.MipLevels;
    const uint64_t            layers = volume ? 1u : desc.DepthOrArraySize;
    known_ = source.SubresourceIndex < mips * layers * most_planes;
    if (box != nullptr) {
        read_ = *box;
        return;
    }
    const uint32_t mip = static_cast<uint32_t>(source.SubresourceIndex % mips);
    const uint64_t width = desc.Width >> mip;
    const uint32_t height = desc.Height >> mip;
    const uint32_t depth = volume ? static_cast<uint32_t>(desc.DepthOrArraySize >> mip) : 1u;
    read_ = d3d12proto::Box{0u, 0u, 0u, width == 0u ? 1u : static_cast<uint32_t>(width), height == 0u ? 1u : height,
                            depth == 0u ? 1u : depth};
}

bool D3d12CopyExtent::known() const
{
    AV_SCOPE("D3d12CopyExtent::known");
    return known_;
}

const d3d12proto::Box &D3d12CopyExtent::read() const
{
    AV_SCOPE("D3d12CopyExtent::read");
    return read_;
}

bool D3d12CopyExtent::written_at(uint32_t x, uint32_t y, uint32_t z, d3d12proto::Box &region) const
{
    AV_SCOPE("D3d12CopyExtent::written_at");
    if (read_.right < read_.left || read_.bottom < read_.top || read_.back < read_.front) return false;
    const uint64_t right = static_cast<uint64_t>(x) + (read_.right - read_.left);
    const uint64_t bottom = static_cast<uint64_t>(y) + (read_.bottom - read_.top);
    const uint64_t back = static_cast<uint64_t>(z) + (read_.back - read_.front);
    constexpr uint64_t most = 0xFFFFFFFFu;
    if (right > most || bottom > most || back > most) return false;
    region = d3d12proto::Box{x, y, z, static_cast<uint32_t>(right), static_cast<uint32_t>(bottom),
                             static_cast<uint32_t>(back)};
    return true;
}

}  // namespace av
