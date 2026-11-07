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
    if (destination_.Dimension != source_.Dimension || destination_.Width != source_.Width) return false;
    if (destination_.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER) return true;
    return destination_.Height == source_.Height && destination_.DepthOrArraySize == source_.DepthOrArraySize &&
           destination_.MipLevels == source_.MipLevels && texels();
}

bool D3d12CopyPair::texels() const
{
    AV_SCOPE("D3d12CopyPair::texels");
    return destination_.Dimension != D3D12_RESOURCE_DIMENSION_BUFFER &&
           source_.Dimension != D3D12_RESOURCE_DIMENSION_BUFFER &&
           destination_.SampleDesc.Count == source_.SampleDesc.Count &&
           D3d12CopyFormats(static_cast<uint32_t>(destination_.Format), static_cast<uint32_t>(source_.Format))
               .same_texels();
}

}
