#include "executor/d3d/d3d12_indirect_stride.hpp"

#include "core/trace/function_scope.hpp"
#include "executor/shim/d3d_headers.hpp"

namespace av {

namespace {
constexpr GUID stride_guid = {0xc0a6da73u, 0x99d7u, 0x49a0u, {0xb0u, 0x88u, 0xdfu, 0x4fu, 0x45u, 0x89u, 0x63u, 0x75u}};

}

D3d12IndirectStride::D3d12IndirectStride() : stride_(0u), addressed_(false)
{
    AV_SCOPE("D3d12IndirectStride::D3d12IndirectStride");
}

D3d12IndirectStride::D3d12IndirectStride(uint32_t stride) : stride_(stride), addressed_(false)
{
    AV_SCOPE("D3d12IndirectStride::D3d12IndirectStride");
}

D3d12IndirectStride::D3d12IndirectStride(uint32_t stride, bool addressed) : stride_(stride), addressed_(addressed)
{
    AV_SCOPE("D3d12IndirectStride::D3d12IndirectStride");
}

bool D3d12IndirectStride::stamp(ID3D12CommandSignature *signature) const
{
    AV_SCOPE("D3d12IndirectStride::stamp");
    const uint32_t words[2] = {stride_, addressed_ ? 1u : 0u};
    return SUCCEEDED(signature->SetPrivateData(stride_guid, sizeof words, words));
}

bool D3d12IndirectStride::read(ID3D12CommandSignature *signature)
{
    AV_SCOPE("D3d12IndirectStride::read");
    uint32_t words[2] = {};
    UINT     size = sizeof words;
    if (FAILED(signature->GetPrivateData(stride_guid, &size, words)) || size != sizeof words) return false;
    stride_ = words[0];
    addressed_ = words[1] != 0u;
    return true;
}

bool D3d12IndirectStride::addressed() const
{
    AV_SCOPE("D3d12IndirectStride::addressed");
    return addressed_;
}

bool D3d12IndirectStride::reaches(uint64_t offset, uint32_t count, uint64_t width) const
{
    AV_SCOPE("D3d12IndirectStride::reaches");
    return offset % word_bytes == 0u && offset <= width && uint64_t{count} * stride_ <= width - offset;
}

}
