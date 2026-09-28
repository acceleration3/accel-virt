#include "device/executor/d3d12_root_layout.hpp"

#include "core/trace/function_scope.hpp"
#include "device/shim/d3d_headers.hpp"

namespace av {

namespace {

/* {124c5515-2c7f-4159-a086-ef9cc0405e27} */
constexpr GUID layout_guid = {0x124c5515u, 0x2c7fu, 0x4159u, {0xa0u, 0x86u, 0xefu, 0x9cu, 0xc0u, 0x40u, 0x5eu, 0x27u}};

}  // namespace

D3d12RootLayout::D3d12RootLayout() : stored_{}
{
    AV_SCOPE("D3d12RootLayout::D3d12RootLayout");
}

D3d12RootLayout::D3d12RootLayout(const d3d12proto::RootParameter *parameters, uint32_t count) : stored_{}
{
    AV_SCOPE("D3d12RootLayout::D3d12RootLayout");
    stored_.count_ = count < max_parameters ? count : max_parameters;
    for (uint32_t index = 0u; index < stored_.count_; index++) {
        stored_.types_[index] = parameters[index].type;
        stored_.values_[index] = parameters[index].values;
    }
}

bool D3d12RootLayout::attach(ID3D12RootSignature *root) const
{
    AV_SCOPE("D3d12RootLayout::attach");
    return SUCCEEDED(root->SetPrivateData(layout_guid, sizeof stored_, &stored_));
}

bool D3d12RootLayout::read(ID3D12RootSignature *root)
{
    AV_SCOPE("D3d12RootLayout::read");
    Stored stored = {};
    UINT   size = sizeof stored;
    if (FAILED(root->GetPrivateData(layout_guid, &size, &stored)) || size != sizeof stored ||
        stored.count_ > max_parameters)
        return false;
    stored_ = stored;
    return true;
}

bool D3d12RootLayout::has_constants(uint32_t index, uint32_t offset, uint32_t count) const
{
    AV_SCOPE("D3d12RootLayout::has_constants");
    return index < stored_.count_ && stored_.types_[index] == constants_type && count != 0u &&
           uint64_t{offset} + count <= stored_.values_[index];
}

bool D3d12RootLayout::has_descriptor(uint32_t index, uint32_t type) const
{
    AV_SCOPE("D3d12RootLayout::has_descriptor");
    return index < stored_.count_ && stored_.types_[index] == type && type != constants_type;
}

}  // namespace av
