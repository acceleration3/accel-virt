#include "device/executor/d3d12_view_writer.hpp"

#include "core/trace/function_scope.hpp"
#include "device/executor/d3d12_address_map.hpp"
#include "device/executor/d3d_object_table.hpp"
#include "device/shim/d3d_headers.hpp"

namespace av {

namespace {

constexpr uint32_t max_format = 191u;
constexpr uint32_t wire_args = 6u;
constexpr uint32_t buffer_dimension = 1u;
/* CBVs are 256-byte aligned and at most 64 KiB, as Direct3D 12 has them. */
constexpr uint32_t constant_alignment = 256u;
constexpr uint32_t most_constant_bytes = 65536u;

/* Each dimension's UINT members, and whether a ResourceMinLODClamp follows
 * them, indexed by the kind's D3D12_*_DIMENSION: 0 is unknown and none. */
struct Shape {
    uint32_t words;
    bool     lod;
    bool     known;
};
constexpr Shape srv_shapes[] = {{0u, false, false}, {0u, false, true}, {2u, true, true}, {4u, true, true},
                                {3u, true, true},   {5u, true, true},  {0u, false, true}, {2u, false, true},
                                {2u, true, true},   {2u, true, true},  {4u, true, true}};
constexpr Shape uav_shapes[] = {{0u, false, false}, {0u, false, true}, {1u, false, true}, {3u, false, true},
                                {2u, false, true},  {4u, false, true}, {0u, false, false}, {0u, false, false},
                                {3u, false, true}};
constexpr Shape rtv_shapes[] = {{0u, false, false}, {0u, false, true}, {1u, false, true}, {3u, false, true},
                                {2u, false, true},  {4u, false, true}, {0u, false, true}, {2u, false, true},
                                {3u, false, true}};
constexpr Shape dsv_shapes[] = {{0u, false, false}, {1u, false, true}, {3u, false, true}, {1u, false, true},
                                {3u, false, true},  {0u, false, true}, {2u, false, true}};

template <size_t N> const Shape *shape_of(const Shape (&shapes)[N], uint32_t dimension)
{
    AV_SCOPE("shape_of");
    return dimension < N && shapes[dimension].known ? &shapes[dimension] : nullptr;
}

}  // namespace

D3d12ViewWriter::D3d12ViewWriter(const D3dObjectTable &objects, const D3d12AddressMap &addresses)
    : objects_(objects), addresses_(addresses), device_(nullptr)
{
    AV_SCOPE("D3d12ViewWriter::D3d12ViewWriter");
}

void D3d12ViewWriter::bind(ID3D12Device *device)
{
    AV_SCOPE("D3d12ViewWriter::bind");
    device_ = device;
}

ID3D12Resource *D3d12ViewWriter::resource(uint32_t id) const
{
    AV_SCOPE("D3d12ViewWriter::resource");
    return static_cast<ID3D12Resource *>(static_cast<void *>(objects_.find(id, D3dObjectKind::resource12)));
}

bool D3d12ViewWriter::members(void *union_members, uint32_t count, bool lod, const d3d12proto::ViewDesc &desc) const
{
    AV_SCOPE("D3d12ViewWriter::members");
    if (count > wire_args) return false;
    uint32_t *const words = static_cast<uint32_t *>(union_members);
    for (uint32_t index = 0u; index < count; index++) words[index] = desc.args[index];
    if (lod) __builtin_memcpy(words + count, &desc.min_lod, sizeof(float));
    return true;
}

bool D3d12ViewWriter::shader_resource(size_t at, bool has_desc, const d3d12proto::ViewDesc &desc) const
{
    AV_SCOPE("D3d12ViewWriter::shader_resource");
    if (device_ == nullptr) return false;
    ID3D12Resource *const viewed = resource(desc.resource);
    if (desc.resource != 0u && viewed == nullptr) return false;
    const D3D12_CPU_DESCRIPTOR_HANDLE handle = {at};
    if (!has_desc) {
        if (viewed == nullptr) return false;
        device_->CreateShaderResourceView(viewed, nullptr, handle);
        return true;
    }
    const Shape *const shape = shape_of(srv_shapes, desc.dimension);
    if (shape == nullptr || desc.format > max_format) return false;
    D3D12_SHADER_RESOURCE_VIEW_DESC view = {};
    view.Format = static_cast<DXGI_FORMAT>(desc.format);
    view.ViewDimension = static_cast<D3D12_SRV_DIMENSION>(desc.dimension);
    view.Shader4ComponentMapping = desc.mapping;
    if (desc.dimension == buffer_dimension) {
        view.Buffer.FirstElement = desc.first_element;
        view.Buffer.NumElements = desc.args[0];
        view.Buffer.StructureByteStride = desc.args[1];
        view.Buffer.Flags = static_cast<D3D12_BUFFER_SRV_FLAGS>(desc.flags);
    } else if (!members(&view.Texture2D, shape->words, shape->lod, desc)) {
        return false;
    }
    device_->CreateShaderResourceView(viewed, &view, handle);
    return true;
}

bool D3d12ViewWriter::unordered_access(size_t at, bool has_desc, const d3d12proto::ViewDesc &desc) const
{
    AV_SCOPE("D3d12ViewWriter::unordered_access");
    if (device_ == nullptr) return false;
    ID3D12Resource *const viewed = resource(desc.resource);
    ID3D12Resource *const counter = resource(desc.counter_resource);
    if ((desc.resource != 0u && viewed == nullptr) || (desc.counter_resource != 0u && counter == nullptr))
        return false;
    const D3D12_CPU_DESCRIPTOR_HANDLE handle = {at};
    if (!has_desc) {
        if (viewed == nullptr) return false;
        device_->CreateUnorderedAccessView(viewed, counter, nullptr, handle);
        return true;
    }
    const Shape *const shape = shape_of(uav_shapes, desc.dimension);
    if (shape == nullptr || desc.format > max_format) return false;
    D3D12_UNORDERED_ACCESS_VIEW_DESC view = {};
    view.Format = static_cast<DXGI_FORMAT>(desc.format);
    view.ViewDimension = static_cast<D3D12_UAV_DIMENSION>(desc.dimension);
    if (desc.dimension == buffer_dimension) {
        view.Buffer.FirstElement = desc.first_element;
        view.Buffer.NumElements = desc.args[0];
        view.Buffer.StructureByteStride = desc.args[1];
        view.Buffer.CounterOffsetInBytes = desc.counter_offset;
        view.Buffer.Flags = static_cast<D3D12_BUFFER_UAV_FLAGS>(desc.flags);
    } else if (!members(&view.Texture2D, shape->words, shape->lod, desc)) {
        return false;
    }
    device_->CreateUnorderedAccessView(viewed, counter, &view, handle);
    return true;
}

bool D3d12ViewWriter::render_target(size_t at, bool has_desc, const d3d12proto::ViewDesc &desc) const
{
    AV_SCOPE("D3d12ViewWriter::render_target");
    if (device_ == nullptr) return false;
    ID3D12Resource *const viewed = resource(desc.resource);
    if (desc.resource != 0u && viewed == nullptr) return false;
    const D3D12_CPU_DESCRIPTOR_HANDLE handle = {at};
    if (!has_desc) {
        if (viewed == nullptr) return false;
        device_->CreateRenderTargetView(viewed, nullptr, handle);
        return true;
    }
    const Shape *const shape = shape_of(rtv_shapes, desc.dimension);
    if (shape == nullptr || desc.format > max_format) return false;
    D3D12_RENDER_TARGET_VIEW_DESC view = {};
    view.Format = static_cast<DXGI_FORMAT>(desc.format);
    view.ViewDimension = static_cast<D3D12_RTV_DIMENSION>(desc.dimension);
    if (desc.dimension == buffer_dimension) {
        view.Buffer.FirstElement = desc.first_element;
        view.Buffer.NumElements = desc.args[0];
    } else if (!members(&view.Texture2D, shape->words, shape->lod, desc)) {
        return false;
    }
    device_->CreateRenderTargetView(viewed, &view, handle);
    return true;
}

bool D3d12ViewWriter::depth_stencil(size_t at, bool has_desc, const d3d12proto::ViewDesc &desc) const
{
    AV_SCOPE("D3d12ViewWriter::depth_stencil");
    if (device_ == nullptr) return false;
    ID3D12Resource *const viewed = resource(desc.resource);
    if (desc.resource != 0u && viewed == nullptr) return false;
    const D3D12_CPU_DESCRIPTOR_HANDLE handle = {at};
    if (!has_desc) {
        if (viewed == nullptr) return false;
        device_->CreateDepthStencilView(viewed, nullptr, handle);
        return true;
    }
    const Shape *const shape = shape_of(dsv_shapes, desc.dimension);
    if (shape == nullptr || desc.format > max_format) return false;
    D3D12_DEPTH_STENCIL_VIEW_DESC view = {};
    view.Format = static_cast<DXGI_FORMAT>(desc.format);
    view.ViewDimension = static_cast<D3D12_DSV_DIMENSION>(desc.dimension);
    view.Flags = static_cast<D3D12_DSV_FLAGS>(desc.flags);
    if (!members(&view.Texture2D, shape->words, shape->lod, desc)) return false;
    device_->CreateDepthStencilView(viewed, &view, handle);
    return true;
}

bool D3d12ViewWriter::constant_buffer(size_t at, uint64_t address, uint32_t size) const
{
    AV_SCOPE("D3d12ViewWriter::constant_buffer");
    if (device_ == nullptr || size > most_constant_bytes || size % constant_alignment != 0u) return false;
    D3D12_CONSTANT_BUFFER_VIEW_DESC view = {};
    if (address != 0u) {
        view.BufferLocation = addresses_.translate(address, size);
        if (view.BufferLocation == 0u) return false;
    }
    view.SizeInBytes = size;
    device_->CreateConstantBufferView(&view, D3D12_CPU_DESCRIPTOR_HANDLE{at});
    return true;
}

bool D3d12ViewWriter::sampler(size_t at, const d3d12proto::SamplerDesc &desc) const
{
    AV_SCOPE("D3d12ViewWriter::sampler");
    if (device_ == nullptr) return false;
    D3D12_SAMPLER_DESC made = {};
    made.Filter = static_cast<D3D12_FILTER>(desc.filter);
    made.AddressU = static_cast<D3D12_TEXTURE_ADDRESS_MODE>(desc.address_u);
    made.AddressV = static_cast<D3D12_TEXTURE_ADDRESS_MODE>(desc.address_v);
    made.AddressW = static_cast<D3D12_TEXTURE_ADDRESS_MODE>(desc.address_w);
    made.MipLODBias = desc.mip_lod_bias;
    made.MaxAnisotropy = desc.max_anisotropy;
    made.ComparisonFunc = static_cast<D3D12_COMPARISON_FUNC>(desc.comparison);
    for (uint32_t channel = 0u; channel < 4u; channel++) made.BorderColor[channel] = desc.border_color[channel];
    made.MinLOD = desc.min_lod;
    made.MaxLOD = desc.max_lod;
    device_->CreateSampler(&made, D3D12_CPU_DESCRIPTOR_HANDLE{at});
    return true;
}

}  // namespace av
