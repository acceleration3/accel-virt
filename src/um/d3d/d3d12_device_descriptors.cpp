/* Descriptor heaps and the views written into them. A CPU descriptor handle is
 * the address of a slot in the heap's block that holds its own heap id and
 * index; a GPU one is the heap id over the index's byte offset. Either comes
 * back to (heap, index) without a search, which is how the wire names it. */
#include "core/trace/function_scope.hpp"
#include "proto/generated/d3d12/d3d12_dispatch_client.h"
#include "um/d3d/d3d12_device.hpp"

namespace av::d3d12 {

namespace {

constexpr uint32_t gpu_heap_shift = 32u;
constexpr uint64_t gpu_offset_mask = 0xffffffffu;
/* D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE. */
constexpr uint32_t api_shader_visible = 0x1u;
/* The kinds' D3D12_*_DIMENSION values the translation below chooses from. */
constexpr uint32_t view_buffer = 1u;
constexpr uint32_t srv_1d = 2u, srv_1d_array = 3u, srv_2d = 4u, srv_2d_array = 5u, srv_2d_ms = 6u,
                   srv_2d_ms_array = 7u, srv_3d = 8u, srv_cube = 9u, srv_cube_array = 10u;
constexpr uint32_t dsv_1d = 1u, dsv_1d_array = 2u, dsv_2d = 3u, dsv_2d_array = 4u, dsv_2d_ms = 5u,
                   dsv_2d_ms_array = 6u;

enum class ViewKind : uint32_t { shader_resource, unordered_access, render_target };

}  // namespace

SIZE_T D3d12Device::calc_private_descriptor_heap_size(const D3D12DDIARG_CREATE_DESCRIPTOR_HEAP_0001 *args)
{
    AV_SCOPE("D3d12Device::calc_private_descriptor_heap_size");
    const SIZE_T count = args == nullptr ? 0u : args->NumDescriptors;
    return sizeof(DescriptorHeapRecord) + count * descriptor_bytes;
}

HRESULT D3d12Device::create_descriptor_heap(const D3D12DDIARG_CREATE_DESCRIPTOR_HEAP_0001 *args,
                                            D3D12DDI_HDESCRIPTORHEAP handle)
{
    AV_SCOPE("D3d12Device::create_descriptor_heap");
    if (args == nullptr || handle.pDrvPrivate == nullptr) return E_INVALIDARG;
    DescriptorHeapRecord &heap = *static_cast<DescriptorHeapRecord *>(handle.pDrvPrivate);
    heap = DescriptorHeapRecord{};
    stamp(heap.header, DescriptorHeapRecord::tag, next_id());
    heap.type = static_cast<uint32_t>(args->Type);
    heap.count = args->NumDescriptors;
    heap.flags = (args->Flags & D3D12DDI_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE) != 0 ? api_shader_visible : 0u;
    heap.increment = descriptor_bytes;
    uint8_t *const slots = static_cast<uint8_t *>(handle.pDrvPrivate) + sizeof(DescriptorHeapRecord);
    for (uint32_t index = 0u; index < heap.count; index++) {
        const DescriptorSlot slot = {heap.header.id, index};
        __builtin_memcpy(slots + static_cast<size_t>(index) * descriptor_bytes, &slot, sizeof(slot));
    }
    if (!sent(d3d12proto::av_d3d12_CreateDescriptorHeap12(wire(), heap.header.id, heap.type, heap.count, heap.flags),
              "CreateDescriptorHeap12"))
        return E_OUTOFMEMORY;
    return S_OK;
}

VOID D3d12Device::destroy_descriptor_heap(D3D12DDI_HDESCRIPTORHEAP handle)
{
    AV_SCOPE("D3d12Device::destroy_descriptor_heap");
    DescriptorHeapRecord &heap = *static_cast<DescriptorHeapRecord *>(handle.pDrvPrivate);
    static_cast<void>(sent(d3d12proto::av_d3d12_Destroy12(wire(), heap.header.id), "Destroy12"));
    heap = DescriptorHeapRecord{};
}

UINT D3d12Device::get_descriptor_size_in_bytes(D3D12DDI_DESCRIPTOR_HEAP_TYPE)
{
    AV_SCOPE("D3d12Device::get_descriptor_size_in_bytes");
    return descriptor_bytes;
}

D3D12DDI_CPU_DESCRIPTOR_HANDLE D3d12Device::get_cpu_descriptor_handle_for_heap_start(D3D12DDI_HDESCRIPTORHEAP handle)
{
    AV_SCOPE("D3d12Device::get_cpu_descriptor_handle_for_heap_start");
    D3D12DDI_CPU_DESCRIPTOR_HANDLE start = {};
    start.ptr = reinterpret_cast<SIZE_T>(static_cast<uint8_t *>(handle.pDrvPrivate) + sizeof(DescriptorHeapRecord));
    return start;
}

D3D12DDI_GPU_DESCRIPTOR_HANDLE D3d12Device::get_gpu_descriptor_handle_for_heap_start(D3D12DDI_HDESCRIPTORHEAP handle)
{
    AV_SCOPE("D3d12Device::get_gpu_descriptor_handle_for_heap_start");
    D3D12DDI_GPU_DESCRIPTOR_HANDLE start = {};
    start.ptr = static_cast<UINT64>(id_of(handle.pDrvPrivate)) << gpu_heap_shift;
    return start;
}

d3d12proto::DescriptorRef D3d12Device::descriptor(D3D12DDI_CPU_DESCRIPTOR_HANDLE handle) const
{
    AV_SCOPE("D3d12Device::descriptor");
    DescriptorSlot slot = {};
    if (handle.ptr != 0u) __builtin_memcpy(&slot, reinterpret_cast<const void *>(handle.ptr), sizeof(slot));
    return d3d12proto::DescriptorRef{slot.heap, slot.index};
}

d3d12proto::DescriptorRef D3d12Device::descriptor(D3D12DDI_GPU_DESCRIPTOR_HANDLE handle) const
{
    AV_SCOPE("D3d12Device::descriptor");
    return d3d12proto::DescriptorRef{static_cast<uint32_t>(handle.ptr >> gpu_heap_shift),
                                     static_cast<uint32_t>((handle.ptr & gpu_offset_mask) / descriptor_bytes)};
}

/* A view's resource, or for a null view - a descriptor that reads zeros - one
 * slice of one sample, which chooses the plain dimension. */
const ResourceRecord &D3d12Device::viewed(D3D12DDI_HRESOURCE handle) const
{
    AV_SCOPE("D3d12Device::viewed");
    return handle.pDrvPrivate != nullptr ? *static_cast<const ResourceRecord *>(handle.pDrvPrivate) : null_resource_;
}

/* The API union members of each texture dimension, from the DDI's, which
 * folds arrays and multisampling into the resource: a view of a resource with
 * several slices is an array view, of a multisampled one a multisample view. */
bool D3d12Device::texture_view(uint32_t kind, const ResourceRecord &resource, D3D12DDI_RESOURCE_DIMENSION dimension,
                               const void *members, d3d12proto::ViewDesc &view) const
{
    AV_SCOPE("D3d12Device::texture_view");
    const uint32_t *const in = static_cast<const uint32_t *>(members);
    const bool            array = resource.depth_or_array > 1u && dimension != D3D12DDI_RD_TEXTURE3D;
    const bool            multisampled = resource.sample_count > 1u;
    const bool            srv = kind == static_cast<uint32_t>(ViewKind::shader_resource);
    float                 lod = 0.0f;
    switch (dimension) {
    case D3D12DDI_RD_TEXTURE1D:
        /* DDI: MostDetailedMip|MipSlice, FirstArraySlice, [MipLevels, ArraySize, LOD]. */
        view.dimension = array ? srv_1d_array : srv_1d;
        if (srv) {
            __builtin_memcpy(&lod, in + 4, sizeof(lod));
            const uint32_t words[4] = {in[0], in[2], in[1], in[3]};
            for (uint32_t index = 0u; index < 4u; index++) view.args[index] = words[index];
            view.min_lod = lod;
        } else {
            const uint32_t words[3] = {in[0], in[1], in[2]};
            for (uint32_t index = 0u; index < 3u; index++) view.args[index] = array ? words[index] : 0u;
            view.args[0] = in[0];
        }
        return true;
    case D3D12DDI_RD_TEXTURE2D:
        if (multisampled) {
            /* SRV: ..., FirstArraySlice, MipLevels, ArraySize; RTV: MipSlice,
             * FirstArraySlice, ArraySize. */
            view.dimension = array ? srv_2d_ms_array : srv_2d_ms;
            view.args[0] = in[1];
            view.args[1] = srv ? in[3] : in[2];
            return true;
        }
        view.dimension = array ? srv_2d_array : srv_2d;
        if (srv) {
            /* DDI: MostDetailedMip, FirstArraySlice, MipLevels, ArraySize, PlaneSlice, LOD. */
            __builtin_memcpy(&lod, in + 5, sizeof(lod));
            view.min_lod = lod;
            if (array) {
                const uint32_t words[5] = {in[0], in[2], in[1], in[3], in[4]};
                for (uint32_t index = 0u; index < 5u; index++) view.args[index] = words[index];
            } else {
                const uint32_t words[3] = {in[0], in[2], in[4]};
                for (uint32_t index = 0u; index < 3u; index++) view.args[index] = words[index];
            }
        } else if (array) {
            /* DDI: MipSlice, FirstArraySlice, ArraySize, PlaneSlice. */
            for (uint32_t index = 0u; index < 4u; index++) view.args[index] = in[index];
        } else {
            view.args[0] = in[0];
            view.args[1] = in[3];
        }
        return true;
    case D3D12DDI_RD_TEXTURE3D:
        view.dimension = srv_3d;
        if (srv) {
            /* DDI: MostDetailedMip, MipLevels, LOD. */
            view.args[0] = in[0];
            view.args[1] = in[1];
            __builtin_memcpy(&view.min_lod, in + 2, sizeof(float));
        } else {
            for (uint32_t index = 0u; index < 3u; index++) view.args[index] = in[index];
        }
        return true;
    case D3D12DDI_RD_TEXTURECUBE:
        if (srv) {
            /* DDI: MostDetailedMip, MipLevels, First2DArrayFace, NumCubes, LOD. */
            const bool cubes = in[3] > 1u || in[2] != 0u;
            view.dimension = cubes ? srv_cube_array : srv_cube;
            view.args[0] = in[0];
            view.args[1] = in[1];
            if (cubes) {
                view.args[2] = in[2];
                view.args[3] = in[3];
            }
            __builtin_memcpy(&view.min_lod, in + 4, sizeof(float));
            return true;
        }
        /* A render target of a cube is a 2D array view of its faces. */
        view.dimension = srv_2d_array;
        for (uint32_t index = 0u; index < 3u; index++) view.args[index] = in[index];
        return true;
    default: return false;
    }
}

VOID D3d12Device::create_shader_resource_view(const D3D12DDIARG_CREATE_SHADER_RESOURCE_VIEW_0002 *args,
                                              D3D12DDI_CPU_DESCRIPTOR_HANDLE at)
{
    AV_SCOPE("D3d12Device::create_shader_resource_view");
    if (args == nullptr) return;
    d3d12proto::ViewDesc view = {};
    view.resource = id_of(args->hDrvResource.pDrvPrivate);
    view.format = static_cast<uint32_t>(args->Format);
    view.mapping = args->Shader4ComponentMapping;
    if (args->ResourceDimension == D3D12DDI_RD_BUFFER) {
        view.dimension = view_buffer;
        view.first_element = args->Buffer.FirstElement;
        view.args[0] = args->Buffer.NumElements;
        view.args[1] = args->Buffer.StructureByteStride;
        view.flags = static_cast<uint32_t>(args->Buffer.Flags);
    } else if (!texture_view(static_cast<uint32_t>(ViewKind::shader_resource), viewed(args->hDrvResource),
                             args->ResourceDimension, &args->Tex2D, view)) {
        unsupported("a shader resource view of this dimension");
        return;
    }
    static_cast<void>(sent(d3d12proto::av_d3d12_CreateShaderResourceView12(wire(), descriptor(at), 1u, view),
                           "CreateShaderResourceView12"));
}

VOID D3d12Device::create_unordered_access_view(const D3D12DDIARG_CREATE_UNORDERED_ACCESS_VIEW_0002 *args,
                                               D3D12DDI_CPU_DESCRIPTOR_HANDLE at)
{
    AV_SCOPE("D3d12Device::create_unordered_access_view");
    if (args == nullptr) return;
    d3d12proto::ViewDesc view = {};
    view.resource = id_of(args->hDrvResource.pDrvPrivate);
    view.format = static_cast<uint32_t>(args->Format);
    if (args->ResourceDimension == D3D12DDI_RD_BUFFER) {
        view.dimension = view_buffer;
        view.first_element = args->Buffer.FirstElement;
        view.counter_resource = id_of(args->Buffer.hDrvCounterResource.pDrvPrivate);
        view.counter_offset = args->Buffer.CounterOffsetInBytes;
        view.args[0] = args->Buffer.NumElements;
        view.args[1] = args->Buffer.StructureByteStride;
        view.flags = static_cast<uint32_t>(args->Buffer.Flags);
    } else if (!texture_view(static_cast<uint32_t>(ViewKind::unordered_access), viewed(args->hDrvResource),
                             args->ResourceDimension, &args->Tex2D, view)) {
        unsupported("an unordered access view of this dimension");
        return;
    }
    static_cast<void>(sent(d3d12proto::av_d3d12_CreateUnorderedAccessView12(wire(), descriptor(at), 1u, view),
                           "CreateUnorderedAccessView12"));
}

VOID D3d12Device::create_render_target_view(const D3D12DDIARG_CREATE_RENDER_TARGET_VIEW_0002 *args,
                                            D3D12DDI_CPU_DESCRIPTOR_HANDLE at)
{
    AV_SCOPE("D3d12Device::create_render_target_view");
    if (args == nullptr) return;
    d3d12proto::ViewDesc view = {};
    view.resource = id_of(args->hDrvResource.pDrvPrivate);
    view.format = static_cast<uint32_t>(args->Format);
    if (args->ResourceDimension == D3D12DDI_RD_BUFFER) {
        view.dimension = view_buffer;
        view.first_element = args->Buffer.FirstElement;
        view.args[0] = args->Buffer.NumElements;
    } else if (!texture_view(static_cast<uint32_t>(ViewKind::render_target), viewed(args->hDrvResource),
                             args->ResourceDimension, &args->Tex2D, view)) {
        unsupported("a render target view of this dimension");
        return;
    }
    static_cast<void>(sent(d3d12proto::av_d3d12_CreateRenderTargetView12(wire(), descriptor(at), 1u, view),
                           "CreateRenderTargetView12"));
}

VOID D3d12Device::create_depth_stencil_view(const D3D12DDIARG_CREATE_DEPTH_STENCIL_VIEW *args,
                                            D3D12DDI_CPU_DESCRIPTOR_HANDLE at)
{
    AV_SCOPE("D3d12Device::create_depth_stencil_view");
    if (args == nullptr || args->hDrvResource.pDrvPrivate == nullptr) return;
    const ResourceRecord &resource = *static_cast<const ResourceRecord *>(args->hDrvResource.pDrvPrivate);
    d3d12proto::ViewDesc  view = {};
    view.resource = resource.header.id;
    view.format = static_cast<uint32_t>(args->Format);
    view.flags = static_cast<uint32_t>(args->Flags);
    const bool array = resource.depth_or_array > 1u || args->ResourceDimension == D3D12DDI_RD_TEXTURECUBE;
    /* DDI: MipSlice, FirstArraySlice, ArraySize for every dimension. */
    const uint32_t *const in = &args->Tex2D.MipSlice;
    if (resource.sample_count > 1u) {
        view.dimension = array ? dsv_2d_ms_array : dsv_2d_ms;
        view.args[0] = in[1];
        view.args[1] = in[2];
    } else if (args->ResourceDimension == D3D12DDI_RD_TEXTURE1D) {
        view.dimension = array ? dsv_1d_array : dsv_1d;
        for (uint32_t index = 0u; index < 3u; index++) view.args[index] = in[index];
    } else {
        view.dimension = array ? dsv_2d_array : dsv_2d;
        for (uint32_t index = 0u; index < 3u; index++) view.args[index] = in[index];
    }
    if (!array) view.args[1] = view.args[2] = 0u;
    static_cast<void>(sent(d3d12proto::av_d3d12_CreateDepthStencilView12(wire(), descriptor(at), 1u, view),
                           "CreateDepthStencilView12"));
}

VOID D3d12Device::create_constant_buffer_view(const D3D12DDI_CONSTANT_BUFFER_VIEW_DESC *args,
                                              D3D12DDI_CPU_DESCRIPTOR_HANDLE at)
{
    AV_SCOPE("D3d12Device::create_constant_buffer_view");
    if (args == nullptr) return;
    static_cast<void>(sent(d3d12proto::av_d3d12_CreateConstantBufferView12(wire(), descriptor(at), args->BufferLocation,
                                                                            args->SizeInBytes),
                           "CreateConstantBufferView12"));
}

VOID D3d12Device::create_sampler(const D3D12DDIARG_CREATE_SAMPLER_0096 *args, D3D12DDI_CPU_DESCRIPTOR_HANDLE at)
{
    AV_SCOPE("D3d12Device::create_sampler");
    if (args == nullptr || args->pSamplerDesc == nullptr) return;
    const D3D12DDI_SAMPLER_DESC_0096 &in = *args->pSamplerDesc;
    d3d12proto::SamplerDesc           desc = {};
    desc.filter = static_cast<uint32_t>(in.Filter);
    desc.address_u = static_cast<uint32_t>(in.AddressU);
    desc.address_v = static_cast<uint32_t>(in.AddressV);
    desc.address_w = static_cast<uint32_t>(in.AddressW);
    desc.mip_lod_bias = in.MipLODBias;
    desc.max_anisotropy = in.MaxAnisotropy;
    desc.comparison = static_cast<uint32_t>(in.ComparisonFunc);
    for (uint32_t channel = 0u; channel < 4u; channel++) desc.border_color[channel] = in.FloatBorderColor[channel];
    desc.min_lod = in.MinLOD;
    desc.max_lod = in.MaxLOD;
    static_cast<void>(sent(d3d12proto::av_d3d12_CreateSampler12(wire(), descriptor(at), desc), "CreateSampler12"));
}

VOID D3d12Device::copy_descriptors_simple(UINT count, D3D12DDI_CPU_DESCRIPTOR_HANDLE destination,
                                          D3D12DDI_CPU_DESCRIPTOR_HANDLE source, D3D12DDI_DESCRIPTOR_HEAP_TYPE)
{
    AV_SCOPE("D3d12Device::copy_descriptors_simple");
    if (count == 0u) return;
    static_cast<void>(sent(d3d12proto::av_d3d12_CopyDescriptors12(wire(), descriptor(destination), descriptor(source),
                                                                   count),
                           "CopyDescriptors12"));
}

/* The destination ranges and source ranges walked together, each run where
 * both stay inside one range copied at once - D3D12's CopyDescriptors. A
 * null size array is ranges of one. */
VOID D3d12Device::copy_descriptors(UINT destination_ranges, const D3D12DDI_CPU_DESCRIPTOR_HANDLE *destinations,
                                   const UINT *destination_sizes, UINT source_ranges,
                                   const D3D12DDI_CPU_DESCRIPTOR_HANDLE *sources, const UINT *source_sizes,
                                   D3D12DDI_DESCRIPTOR_HEAP_TYPE type)
{
    AV_SCOPE("D3d12Device::copy_descriptors");
    if (destinations == nullptr || sources == nullptr) return;
    UINT destination = 0u, source = 0u, destination_at = 0u, source_at = 0u;
    while (destination < destination_ranges && source < source_ranges) {
        const UINT destination_size = destination_sizes == nullptr ? 1u : destination_sizes[destination];
        const UINT source_size = source_sizes == nullptr ? 1u : source_sizes[source];
        const UINT left_destination = destination_size - destination_at;
        const UINT left_source = source_size - source_at;
        const UINT run = left_destination < left_source ? left_destination : left_source;
        D3D12DDI_CPU_DESCRIPTOR_HANDLE to = destinations[destination];
        D3D12DDI_CPU_DESCRIPTOR_HANDLE from = sources[source];
        to.ptr += static_cast<SIZE_T>(destination_at) * descriptor_bytes;
        from.ptr += static_cast<SIZE_T>(source_at) * descriptor_bytes;
        copy_descriptors_simple(run, to, from, type);
        destination_at += run;
        source_at += run;
        if (destination_at == destination_size) destination++, destination_at = 0u;
        if (source_at == source_size) source++, source_at = 0u;
    }
}

}  // namespace av::d3d12
