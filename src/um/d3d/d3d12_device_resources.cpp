/* Heaps and resources: every one made on the host under an id, at a guest
 * GPU virtual address this device hands out, and a CPU-visible heap placed in
 * the session's arena so that what the application maps is what the host's
 * GPU reads. */
#include "core/trace/function_scope.hpp"
#include "icd/common/log_sink.hpp"
#include "icd/d3d/d3d12_heap_arena.hpp"
#include "proto/generated/d3d12/d3d12_dispatch_client.h"
#include "um/d3d/d3d12_device.hpp"
#include "um/d3d/d3d_session.hpp"

namespace av::d3d12 {

namespace {

/* D3D12_HEAP_TYPE, D3D12_CPU_PAGE_PROPERTY, D3D12_MEMORY_POOL. */
constexpr uint32_t heap_default = 1u;
constexpr uint32_t heap_upload = 2u;
constexpr uint32_t heap_readback = 3u;
/* D3D12_HEAP_FLAG_DENY_* and D3D12_HEAP_FLAG_SHARED. */
constexpr uint32_t deny_buffers = 0x4u;
constexpr uint32_t deny_rt_ds_textures = 0x40u;
constexpr uint32_t deny_non_rt_ds_textures = 0x80u;
/* D3D12_RESOURCE_FLAG_*. */
constexpr uint32_t allow_render_target = 0x1u;
constexpr uint32_t allow_depth_stencil = 0x2u;
constexpr uint32_t allow_unordered_access = 0x4u;
constexpr uint32_t deny_shader_resource = 0x8u;
constexpr uint32_t allow_cross_adapter = 0x10u;
constexpr uint32_t allow_simultaneous_access = 0x20u;
/* D3D12_RESOURCE_STATES a CPU-visible heap's resources are made in, which
 * the host - like Direct3D 12 - insists on. */
constexpr uint32_t state_generic_read = 0xAC3u;
constexpr uint32_t state_copy_dest = 0x400u;
constexpr uint32_t buffer_dimension = 1u;
constexpr uint64_t buffer_alignment = uint64_t{64} * 1024u;
constexpr uint64_t refused_size = ~uint64_t{0};

/* D3D12_RESOURCE_STATES by D3D12DDI_BARRIER_LAYOUT, the layout D3D12Core hands
 * a driver that takes legacy barriers the state it was made in as. */
uint32_t state_of_layout(D3D12DDI_BARRIER_LAYOUT layout)
{
    AV_SCOPE("state_of_layout");
    switch (layout) {
    case D3D12DDI_BARRIER_LAYOUT_GENERIC_READ: return state_generic_read;
    case D3D12DDI_BARRIER_LAYOUT_RENDER_TARGET: return 0x4u;
    case D3D12DDI_BARRIER_LAYOUT_UNORDERED_ACCESS: return 0x8u;
    case D3D12DDI_BARRIER_LAYOUT_DEPTH_STENCIL_WRITE: return 0x10u;
    case D3D12DDI_BARRIER_LAYOUT_DEPTH_STENCIL_READ: return 0x20u;
    case D3D12DDI_BARRIER_LAYOUT_SHADER_RESOURCE: return 0xC0u;
    case D3D12DDI_BARRIER_LAYOUT_COPY_SOURCE: return 0x800u;
    case D3D12DDI_BARRIER_LAYOUT_COPY_DEST: return state_copy_dest;
    case D3D12DDI_BARRIER_LAYOUT_RESOLVE_SOURCE: return 0x2000u;
    case D3D12DDI_BARRIER_LAYOUT_RESOLVE_DEST: return 0x1000u;
    default: return 0u;
    }
}

uint32_t api_resource_flags(uint32_t ddi)
{
    AV_SCOPE("api_resource_flags");
    uint32_t flags = 0u;
    if ((ddi & D3D12DDI_RESOURCE_FLAG_0003_RENDER_TARGET) != 0u) flags |= allow_render_target;
    if ((ddi & D3D12DDI_RESOURCE_FLAG_0003_DEPTH_STENCIL) != 0u) flags |= allow_depth_stencil;
    if ((ddi & D3D12DDI_RESOURCE_FLAG_0022_UNORDERED_ACCESS) != 0u) flags |= allow_unordered_access;
    if ((ddi & D3D12DDI_RESOURCE_FLAG_0003_SHADER_RESOURCE) == 0u) flags |= deny_shader_resource;
    if ((ddi & D3D12DDI_RESOURCE_FLAG_0003_CROSS_ADAPTER) != 0u) flags |= allow_cross_adapter;
    if ((ddi & D3D12DDI_RESOURCE_FLAG_0003_SIMULTANEOUS_ACCESS) != 0u) flags |= allow_simultaneous_access;
    return flags;
}

/* The DDI says what a heap allows; the API, what it denies. */
uint32_t api_heap_flags(uint32_t ddi)
{
    AV_SCOPE("api_heap_flags");
    uint32_t flags = 0u;
    if ((ddi & D3D12DDI_HEAP_FLAG_BUFFERS) == 0u) flags |= deny_buffers;
    if ((ddi & D3D12DDI_HEAP_FLAG_RT_DS_TEXTURES) == 0u) flags |= deny_rt_ds_textures;
    if ((ddi & D3D12DDI_HEAP_FLAG_NON_RT_DS_TEXTURES) == 0u) flags |= deny_non_rt_ds_textures;
    return flags;
}

uint32_t heap_type_of(const D3D12DDIARG_CREATEHEAP_0001 &args)
{
    AV_SCOPE("heap_type_of");
    if (args.CPUPageProperty == D3D12DDI_CPU_PAGE_PROPERTY_WRITE_COMBINE) return heap_upload;
    if (args.CPUPageProperty == D3D12DDI_CPU_PAGE_PROPERTY_WRITE_BACK) return heap_readback;
    return heap_default;
}

d3d12proto::HeapProperties properties_of(uint32_t type)
{
    AV_SCOPE("properties_of");
    d3d12proto::HeapProperties made = {};
    made.type = type;
    return made;
}

}  // namespace

d3d12proto::ResourceDesc D3d12Device::resource_desc(const D3D12DDIARG_CREATERESOURCE_0109 &args) const
{
    AV_SCOPE("D3d12Device::resource_desc");
    d3d12proto::ResourceDesc desc = {};
    desc.dimension = static_cast<uint32_t>(args.ResourceType);
    desc.width = args.Width;
    desc.height = args.Height;
    desc.depth_or_array = args.DepthOrArraySize;
    desc.mip_levels = args.MipLevels;
    desc.format = static_cast<uint32_t>(args.Format);
    desc.sample_count = args.SampleDesc.Count;
    desc.sample_quality = args.SampleDesc.Quality;
    desc.layout = static_cast<uint32_t>(args.Layout);
    desc.flags = api_resource_flags(static_cast<uint32_t>(args.Flags));
    return desc;
}

uint32_t D3d12Device::initial_state(const D3D12DDIARG_CREATERESOURCE_0109 &args, uint32_t heap_type) const
{
    AV_SCOPE("D3d12Device::initial_state");
    if (heap_type == heap_upload) return state_generic_read;
    if (heap_type == heap_readback) return state_copy_dest;
    return state_of_layout(args.InitialBarrierLayout);
}

uint64_t D3d12Device::allocation_bytes(const d3d12proto::ResourceDesc &desc, uint64_t &alignment)
{
    AV_SCOPE("D3d12Device::allocation_bytes");
    alignment = buffer_alignment;
    if (desc.dimension == buffer_dimension) return (desc.width + buffer_alignment - 1u) & ~(buffer_alignment - 1u);
    uint64_t bytes = refused_size;
    if (!sent(d3d12proto::rvk_d3d12_ResourceAllocation12(wire(), desc, &bytes, &alignment), "ResourceAllocation12"))
        return refused_size;
    return bytes;
}

HRESULT D3d12Device::make_heap(const D3D12DDIARG_CREATEHEAP_0001 &args, HeapRecord &heap, bool committed)
{
    AV_SCOPE("D3d12Device::make_heap");
    heap = HeapRecord{};
    stamp(heap.header, HeapRecord::tag, next_id());
    heap.type = heap_type_of(args);
    heap.size = args.ByteSize;
    heap.flags = api_heap_flags(static_cast<uint32_t>(args.Flags));
    heap.arena_offset = HeapRecord::no_arena;
    heap.address = addresses_.reserve(args.ByteSize);
    if (heap.address == 0u) return E_OUTOFMEMORY;
    if (heap.type != heap_default) {
        if (heaps_.place(args.ByteSize, heap.arena_offset) != Status::ok) {
            heap.arena_offset = HeapRecord::no_arena;
            log_.write("rvkumd: no room in the session arena for an upload or readback heap");
            return E_OUTOFMEMORY;
        }
    } else if (committed) {
        /* A committed resource in the GPU's own memory is one host object; the
         * heap the runtime asked for with it names nothing there. */
        return S_OK;
    }
    const uint64_t at = heap.arena_offset == HeapRecord::no_arena ? 0u : heap.arena_offset;
    if (!sent(d3d12proto::rvk_d3d12_CreateHeap12(wire(), heap.header.id, properties_of(heap.type), heap.flags,
                                                 heap.size, args.Alignment, at),
              "CreateHeap12"))
        return E_OUTOFMEMORY;
    heap.made = 1u;
    return S_OK;
}

HRESULT D3d12Device::make_resource(const D3D12DDIARG_CREATERESOURCE_0109 &args, const D3D12DDI_CLEAR_VALUES *clear,
                                   HeapRecord &heap, uint64_t offset, ResourceRecord &resource)
{
    AV_SCOPE("D3d12Device::make_resource");
    const d3d12proto::ResourceDesc desc = resource_desc(args);
    resource = ResourceRecord{};
    stamp(resource.header, ResourceRecord::tag, next_id());
    resource.dimension = desc.dimension;
    resource.width = desc.width;
    resource.height = desc.height;
    resource.depth_or_array = desc.depth_or_array;
    resource.mip_levels = desc.mip_levels;
    resource.format = desc.format;
    resource.sample_count = desc.sample_count;
    resource.flags = desc.flags;
    resource.address = desc.dimension == buffer_dimension ? heap.address + offset : 0u;
    d3d12proto::ClearValue value = {};
    if (clear != nullptr) {
        value.present = 1u;
        value.format = static_cast<uint32_t>(clear->Format);
        for (uint32_t channel = 0u; channel < 4u; channel++) value.color[channel] = clear->Color[channel];
        value.depth = clear->DepthStencil.Depth;
        value.stencil = clear->DepthStencil.Stencil;
    }
    const uint32_t state = initial_state(args, heap.type);
    bool           made = false;
    if (heap.made == 0u) {
        made = d3d12proto::rvk_d3d12_CreateCommittedResource12(wire(), resource.header.id, properties_of(heap_default), 0u,
                                                               desc, state, value, resource.address);
    } else {
        resource.heap = heap.header.id;
        made = d3d12proto::rvk_d3d12_CreatePlacedResource12(wire(), resource.header.id, heap.header.id, offset, desc,
                                                             state, value, resource.address);
    }
    return sent(made, "CreateResource12") ? S_OK : E_OUTOFMEMORY;
}

D3D12DDI_HEAP_AND_RESOURCE_SIZES D3d12Device::calc_private_heap_and_resource_sizes(
    const D3D12DDIARG_CREATEHEAP_0001 *, const D3D12DDIARG_CREATERESOURCE_0109 *, D3D12DDI_HPROTECTEDRESOURCESESSION_0030)
{
    AV_SCOPE("D3d12Device::calc_private_heap_and_resource_sizes");
    return D3D12DDI_HEAP_AND_RESOURCE_SIZES{sizeof(HeapRecord), sizeof(ResourceRecord)};
}

/* Three calls in one: a heap alone (pResource null), a committed resource
 * with the heap made for it, and a resource placed in an existing heap
 * (pHeap null), at the offset D3D12Core puts in ReuseBufferGPUVA. */
HRESULT D3d12Device::create_heap_and_resource(const D3D12DDIARG_CREATEHEAP_0001 *heap_args, D3D12DDI_HHEAP heap_handle,
                                              D3D12DDI_HRTRESOURCE, const D3D12DDIARG_CREATERESOURCE_0109 *args,
                                              const D3D12DDI_CLEAR_VALUES *clear,
                                              D3D12DDI_HPROTECTEDRESOURCESESSION_0030, D3D12DDI_HRESOURCE handle)
{
    AV_SCOPE("D3d12Device::create_heap_and_resource");
    if (heap_handle.pDrvPrivate == nullptr) return E_INVALIDARG;
    HeapRecord &heap = *static_cast<HeapRecord *>(heap_handle.pDrvPrivate);
    if (heap_args != nullptr) {
        const HRESULT made = make_heap(*heap_args, heap, args != nullptr);
        if (FAILED(made)) return made;
    }
    if (args == nullptr) return S_OK;
    if (handle.pDrvPrivate == nullptr || heap.header.magic != HeapRecord::tag) return E_INVALIDARG;
    const uint64_t offset = heap_args != nullptr ? 0u : args->ReuseBufferGPUVA.BaseAddress.UMD.Offset;
    return make_resource(*args, clear, heap, offset, *static_cast<ResourceRecord *>(handle.pDrvPrivate));
}

VOID D3d12Device::destroy_heap_and_resource(D3D12DDI_HHEAP heap_handle, D3D12DDI_HRESOURCE handle)
{
    AV_SCOPE("D3d12Device::destroy_heap_and_resource");
    if (handle.pDrvPrivate != nullptr) {
        ResourceRecord &resource = *static_cast<ResourceRecord *>(handle.pDrvPrivate);
        if (resource.header.magic == ResourceRecord::tag)
            static_cast<void>(sent(d3d12proto::rvk_d3d12_Destroy12(wire(), resource.header.id), "Destroy12"));
        resource = ResourceRecord{};
    }
    if (heap_handle.pDrvPrivate == nullptr) return;
    HeapRecord &heap = *static_cast<HeapRecord *>(heap_handle.pDrvPrivate);
    if (heap.header.magic != HeapRecord::tag) return;
    if (heap.made != 0u) static_cast<void>(sent(d3d12proto::rvk_d3d12_Destroy12(wire(), heap.header.id), "Destroy12"));
    if (heap.arena_offset != HeapRecord::no_arena) static_cast<void>(heaps_.remove(heap.arena_offset));
    heap = HeapRecord{};
}

HRESULT D3d12Device::map_heap(D3D12DDI_HHEAP handle, void **data)
{
    AV_SCOPE("D3d12Device::map_heap");
    if (data == nullptr) return E_INVALIDARG;
    *data = nullptr;
    const HeapRecord &heap = *static_cast<const HeapRecord *>(handle.pDrvPrivate);
    uint8_t *const    view = session_.arena_view();
    if (heap.arena_offset == HeapRecord::no_arena || view == nullptr) return E_INVALIDARG;
    *data = view + heap.arena_offset;
    return S_OK;
}

VOID D3d12Device::unmap_heap(D3D12DDI_HHEAP)
{
    AV_SCOPE("D3d12Device::unmap_heap");
}

D3D12DDI_GPU_VIRTUAL_ADDRESS D3d12Device::check_resource_virtual_address(D3D12DDI_HRESOURCE handle)
{
    AV_SCOPE("D3d12Device::check_resource_virtual_address");
    return static_cast<const ResourceRecord *>(handle.pDrvPrivate)->address;
}

VOID D3d12Device::check_resource_allocation_info(const D3D12DDIARG_CREATERESOURCE_0109 *args,
                                                 D3D12DDI_RESOURCE_OPTIMIZATION_FLAGS, UINT32, UINT,
                                                 D3D12DDI_RESOURCE_ALLOCATION_INFO_0022 *info)
{
    AV_SCOPE("D3d12Device::check_resource_allocation_info");
    if (args == nullptr || info == nullptr) return;
    *info = D3D12DDI_RESOURCE_ALLOCATION_INFO_0022{};
    uint64_t alignment = buffer_alignment;
    info->ResourceDataSize = allocation_bytes(resource_desc(*args), alignment);
    info->ResourceDataAlignment = static_cast<UINT32>(alignment);
    info->Layout = args->Layout;
}

VOID D3d12Device::check_existing_resource_allocation_info(D3D12DDI_HRESOURCE handle,
                                                          D3D12DDI_RESOURCE_ALLOCATION_INFO_0022 *info)
{
    AV_SCOPE("D3d12Device::check_existing_resource_allocation_info");
    if (info == nullptr) return;
    *info = D3D12DDI_RESOURCE_ALLOCATION_INFO_0022{};
    const ResourceRecord    &resource = *static_cast<const ResourceRecord *>(handle.pDrvPrivate);
    d3d12proto::ResourceDesc desc = {};
    desc.dimension = resource.dimension;
    desc.width = resource.width;
    desc.height = resource.height;
    desc.depth_or_array = resource.depth_or_array;
    desc.mip_levels = resource.mip_levels;
    desc.format = resource.format;
    desc.sample_count = resource.sample_count;
    desc.flags = resource.flags;
    uint64_t alignment = buffer_alignment;
    info->ResourceDataSize = allocation_bytes(desc, alignment);
    info->ResourceDataAlignment = static_cast<UINT32>(alignment);
}

/* Only a texture in a CPU-visible heap has a layout the CPU sees, and this
 * driver makes none: every CPU-visible heap holds only buffers. */
VOID D3d12Device::check_subresource_info(D3D12DDI_HRESOURCE, UINT, D3D12DDI_SUBRESOURCE_INFO *info)
{
    AV_SCOPE("D3d12Device::check_subresource_info");
    if (info != nullptr) *info = D3D12DDI_SUBRESOURCE_INFO{};
}

VOID D3d12Device::get_mip_packing(D3D12DDI_HRESOURCE, UINT *packed, UINT *tiles)
{
    AV_SCOPE("D3d12Device::get_mip_packing");
    if (packed != nullptr) *packed = 0u;
    if (tiles != nullptr) *tiles = 0u;
}

}  // namespace av::d3d12
