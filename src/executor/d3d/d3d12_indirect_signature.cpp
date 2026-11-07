#include "executor/d3d/d3d12_indirect_signature.hpp"

#include "core/trace/function_scope.hpp"
#include "executor/d3d/d3d12_indirect_stride.hpp"
#include "executor/d3d/d3d12_root_layout.hpp"
#include "executor/shim/d3d_headers.hpp"

namespace av {

D3d12IndirectSignature::D3d12IndirectSignature(const d3d12proto::IndirectArgument *arguments, uint32_t count,
                                               uint32_t stride)
    : arguments_(arguments), count_(count), stride_(stride)
{
    AV_SCOPE("D3d12IndirectSignature::D3d12IndirectSignature");
}

bool D3d12IndirectSignature::holds(const D3d12RootLayout *root) const
{
    AV_SCOPE("D3d12IndirectSignature::holds");
    if (count_ == 0u || count_ > max_arguments || stride_ == 0u || stride_ > max_stride || stride_ % word_bytes != 0u)
        return false;
    uint64_t bytes = 0u;
    for (uint32_t index = 0u; index < count_; index++) {
        const uint64_t taken = bytes_of(arguments_[index], root);
        if (taken == 0u) return false;
        bytes += taken;
    }
    return bytes <= stride_;
}

uint64_t D3d12IndirectSignature::bytes_of(const d3d12proto::IndirectArgument &argument,
                                          const D3d12RootLayout *root) const
{
    AV_SCOPE("D3d12IndirectSignature::bytes_of");
    switch (argument.type) {
    case draw: return sizeof(D3D12_DRAW_ARGUMENTS);
    case draw_indexed: return sizeof(D3D12_DRAW_INDEXED_ARGUMENTS);
    case dispatch: return sizeof(D3D12_DISPATCH_ARGUMENTS);
    case constant:
        return root != nullptr && root->has_constants(argument.a, argument.b, argument.c)
                   ? uint64_t{argument.c} * word_bytes
                   : 0u;
    default: return 0u;
    }
}

ID3D12CommandSignature *D3d12IndirectSignature::make(ID3D12Device *device, ID3D12RootSignature *root) const
{
    AV_SCOPE("D3d12IndirectSignature::make");
    D3D12_INDIRECT_ARGUMENT_DESC descs[max_arguments] = {};
    for (uint32_t index = 0u; index < count_; index++) {
        const d3d12proto::IndirectArgument &from = arguments_[index];
        D3D12_INDIRECT_ARGUMENT_DESC       &to = descs[index];
        to.Type = static_cast<D3D12_INDIRECT_ARGUMENT_TYPE>(from.type);
        to.Constant = {from.a, from.b, from.c};
    }
    const D3D12_COMMAND_SIGNATURE_DESC desc = {stride_, count_, descs, 0u};
    ID3D12CommandSignature *made = nullptr;
    if (FAILED(device->CreateCommandSignature(&desc, root, __uuidof(ID3D12CommandSignature),
                                              reinterpret_cast<void **>(&made))))
        return nullptr;
    if (D3d12IndirectStride(stride_).stamp(made)) return made;
    made->Release();
    return nullptr;
}

}
