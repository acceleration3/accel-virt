#include "executor/d3d/d3d12_indirect_stride.hpp"

#include "core/trace/function_scope.hpp"
#include "executor/shim/d3d_headers.hpp"

namespace av {

namespace {

/* {c0a6da73-99d7-49a0-b088-df4f45896375} */
constexpr GUID stride_guid = {0xc0a6da73u, 0x99d7u, 0x49a0u, {0xb0u, 0x88u, 0xdfu, 0x4fu, 0x45u, 0x89u, 0x63u, 0x75u}};

}  // namespace

D3d12IndirectStride::D3d12IndirectStride() : stride_(0u)
{
    AV_SCOPE("D3d12IndirectStride::D3d12IndirectStride");
}

D3d12IndirectStride::D3d12IndirectStride(uint32_t stride) : stride_(stride)
{
    AV_SCOPE("D3d12IndirectStride::D3d12IndirectStride");
}

bool D3d12IndirectStride::stamp(ID3D12CommandSignature *signature) const
{
    AV_SCOPE("D3d12IndirectStride::stamp");
    return SUCCEEDED(signature->SetPrivateData(stride_guid, sizeof stride_, &stride_));
}

bool D3d12IndirectStride::read(ID3D12CommandSignature *signature)
{
    AV_SCOPE("D3d12IndirectStride::read");
    uint32_t stride = 0u;
    UINT     size = sizeof stride;
    if (FAILED(signature->GetPrivateData(stride_guid, &size, &stride)) || size != sizeof stride) return false;
    stride_ = stride;
    return true;
}

/* A stride is at most D3d12IndirectSignature::max_stride, so the product
 * stays far inside 64 bits. */
bool D3d12IndirectStride::reaches(uint64_t offset, uint32_t count, uint64_t width) const
{
    AV_SCOPE("D3d12IndirectStride::reaches");
    return offset % word_bytes == 0u && offset <= width && uint64_t{count} * stride_ <= width - offset;
}

}  // namespace av
