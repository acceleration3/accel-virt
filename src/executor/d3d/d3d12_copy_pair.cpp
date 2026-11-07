#include "executor/d3d/d3d12_copy_pair.hpp"

#include "core/trace/function_scope.hpp"
#include "executor/d3d/d3d12_copy_formats.hpp"
#include "executor/shim/d3d_headers.hpp"

namespace av {

D3d12CopyPair::D3d12CopyPair(const D3D12_RESOURCE_DESC &destination, const D3D12_RESOURCE_DESC &source)
    : destination_(destination), source_(source)
{
    AV_SCOPE("D3d12CopyPair::D3d12CopyPair");
}

bool D3d12CopyPair::whole() const
{
    AV_SCOPE("D3d12CopyPair::whole");
    if (destination_.Dimension != source_.Dimension) return false;
    if (destination_.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER) return destination_.Width == source_.Width;
    if (source_.Width > 0xFFFFFFFFu) return false;
    if (!texels() || destination_.DepthOrArraySize != source_.DepthOrArraySize ||
        destination_.MipLevels != source_.MipLevels)
        return false;
    return destination_.Width == formats().destination_extent(static_cast<uint32_t>(source_.Width), true) &&
           destination_.Height == formats().destination_extent(source_.Height, false);
}

bool D3d12CopyPair::texels() const
{
    AV_SCOPE("D3d12CopyPair::texels");
    return destination_.Dimension != D3D12_RESOURCE_DIMENSION_BUFFER &&
           source_.Dimension != D3D12_RESOURCE_DIMENSION_BUFFER &&
           destination_.SampleDesc.Count == source_.SampleDesc.Count &&
           formats().same_texels();
}

D3d12CopyFormats D3d12CopyPair::formats() const
{
    AV_SCOPE("D3d12CopyPair::formats");
    return D3d12CopyFormats(static_cast<uint32_t>(destination_.Format), static_cast<uint32_t>(source_.Format));
}

}
