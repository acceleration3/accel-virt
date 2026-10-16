/* Heaps and resources: every one made on the host under an id, at a guest
 * GPU virtual address this device hands out, and a CPU-visible heap placed in
 * the session's arena so that what the application maps is what the host's
 * GPU reads. */
#include "core/log/log_line.hpp"
#include "core/trace/function_scope.hpp"
#include "core/log/log_sink.hpp"
#include "guest/transport/host_allocation.hpp"
#include "proto/generated/d3d12/d3d12_dispatch_client.h"
#include "um/d3d/d3d12_device.hpp"
#include "um/d3d/d3d_session.hpp"

namespace av::d3d12 {

namespace {

/* D3D12_HEAP_TYPE, D3D12_CPU_PAGE_PROPERTY, D3D12_MEMORY_POOL. */
constexpr uint32_t heap_default = 1u;
constexpr uint32_t heap_upload = 2u;
constexpr uint32_t heap_readback = 3u;
/* D3D12_HEAP_FLAG_SHARED and D3D12_HEAP_FLAG_DENY_*. */
constexpr uint32_t heap_shared = 0x1u;
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
/* The runtime sums a resource's data, its additional data header and its
 * additional data, aligning the running size up to each part's alignment in
 * turn and taking any wrap as UINT64_MAX (d3d12core DDIRAIToAPIRAI). Aligned to
 * 0, size - 1 + 0 is below size: every resource of a driver that leaves the
 * alignments of parts it does not have at zero measures UINT64_MAX, and an
 * application that sizes a heap by it has CreateHeap refused. */
constexpr UINT32   no_additional_data_alignment = 1u;

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
    if (!sent(d3d12proto::av_d3d12_ResourceAllocation12(wire(), desc, &bytes, &alignment), "ResourceAllocation12"))
        return refused_size;
    /* The runtime hands an application this size as it is, and one that sizes
     * a heap by it gets E_INVALIDARG from CreateHeap with no word of why. */
    if (bytes == refused_size) log_unsized(desc);
    return bytes;
}

void D3d12Device::log_unsized(const d3d12proto::ResourceDesc &desc)
{
    AV_SCOPE("D3d12Device::log_unsized");
    LogLine(lines_)
        .text("the host sizes no resource: dimension ")
        .number(desc.dimension)
        .text(" format ")
        .number(desc.format)
        .text(" ")
        .number(static_cast<uint32_t>(desc.width))
        .text("x")
        .number(desc.height)
        .text("x")
        .number(desc.depth_or_array)
        .text(" mips ")
        .number(desc.mip_levels)
        .text(" samples ")
        .number(desc.sample_count)
        .text(" layout ")
        .number(desc.layout)
        .text(" flags ")
        .hex(desc.flags, 8u)
        .write();
}

HRESULT D3d12Device::make_heap(const D3D12DDIARG_CREATEHEAP_0001 &args, HeapRecord &heap, bool committed)
{
    AV_SCOPE("D3d12Device::make_heap");
    heap = HeapRecord{};
    stamp(heap.header, HeapRecord::tag, next_id());
    heap.type = heap_type_of(args);
    heap.size = args.ByteSize;
    heap.flags = api_heap_flags(static_cast<uint32_t>(args.Flags));
    heap.memory = nullptr;
    heap.address = addresses_.reserve(args.ByteSize);
    if (heap.address == 0u) {
        refused("heap address space, bytes", static_cast<uint32_t>(args.ByteSize));
        return E_OUTOFMEMORY;
    }
    if (heap.type != heap_default) {
        /* Written by the CPU and read by the GPU, or the other way about: the
         * memory the host gives either is the one native drivers use. */
        const HostMemoryRequest memory(heap.type == heap_upload ? HostMemoryRequest::Kind::upload
                                                                : HostMemoryRequest::Kind::readback,
                                       0u);
        if (session_.open_memory(args.ByteSize, memory, heap.memory) != Status::ok) {
            log_.write("avumd: no host memory for an upload or readback heap");
            return E_OUTOFMEMORY;
        }
    } else if (committed) {
        /* A committed resource in the GPU's own memory is one host object; the
         * heap the runtime asked for with it names nothing there. */
        return S_OK;
    }
    const uint64_t memory = heap.memory != nullptr ? heap.memory->handle() : 0u;
    if (!sent(d3d12proto::av_d3d12_CreateHeap12(wire(), heap.header.stamp.id(), properties_of(heap.type), heap.flags,
                                                 heap.size, args.Alignment, memory),
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
    record_resource(desc, resource);
    resource.address = desc.dimension == buffer_dimension ? heap.address + offset : 0u;
    const d3d12proto::ClearValue value = clear_value(clear);
    const uint32_t               state = initial_state(args, heap.type);
    bool           made = false;
    resource.placed_in = &heap;
    resource.heap_offset = offset;
    if (heap.made == 0u) {
        made = d3d12proto::av_d3d12_CreateCommittedResource12(wire(), resource.header.stamp.id(), properties_of(heap_default),
                                                               heap.flags & heap_shared, desc, state, value,
                                                               resource.address);
    } else {
        resource.heap = heap.header.stamp.id();
        made = d3d12proto::av_d3d12_CreatePlacedResource12(wire(), resource.header.stamp.id(), heap.header.stamp.id(), offset, desc,
                                                             state, value, resource.address);
    }
    return sent(made, "CreateResource12") ? S_OK : E_OUTOFMEMORY;
}

void D3d12Device::record_resource(const d3d12proto::ResourceDesc &desc, ResourceRecord &resource)
{
    AV_SCOPE("D3d12Device::record_resource");
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
}

d3d12proto::ClearValue D3d12Device::clear_value(const D3D12DDI_CLEAR_VALUES *clear) const
{
    AV_SCOPE("D3d12Device::clear_value");
    d3d12proto::ClearValue value = {};
    if (clear == nullptr) return value;
    value.present = 1u;
    value.format = static_cast<uint32_t>(clear->Format);
    for (uint32_t channel = 0u; channel < 4u; channel++) value.color[channel] = clear->Color[channel];
    value.depth = clear->DepthStencil.Depth;
    value.stencil = clear->DepthStencil.Stencil;
    return value;
}

D3D12DDI_HEAP_AND_RESOURCE_SIZES D3d12Device::calc_private_heap_and_resource_sizes(
    const D3D12DDIARG_CREATEHEAP_0001 *, const D3D12DDIARG_CREATERESOURCE_0109 *, D3D12DDI_HPROTECTEDRESOURCESESSION_0030)
{
    AV_SCOPE("D3d12Device::calc_private_heap_and_resource_sizes");
    return D3D12DDI_HEAP_AND_RESOURCE_SIZES{sizeof(HeapRecord), sizeof(ResourceRecord)};
}

/* Four calls in one: a heap alone (pResource null), a committed resource
 * with the heap made for it, a resource placed in an existing heap (pHeap
 * null), at the offset D3D12Core puts in ReuseBufferGPUVA - and a reserved
 * resource, with neither a heap nor a buffer to be placed in. */
HRESULT D3d12Device::create_heap_and_resource(const D3D12DDIARG_CREATEHEAP_0001 *heap_args, D3D12DDI_HHEAP heap_handle,
                                              D3D12DDI_HRTRESOURCE runtime, const D3D12DDIARG_CREATERESOURCE_0109 *args,
                                              const D3D12DDI_CLEAR_VALUES *clear,
                                              D3D12DDI_HPROTECTEDRESOURCESESSION_0030, D3D12DDI_HRESOURCE handle)
{
    AV_SCOPE("D3d12Device::create_heap_and_resource");
    const HRESULT made = heap_and_resource(heap_args, heap_handle, runtime, args, clear, handle);
    if (FAILED(made)) refused("CreateHeapAndResource", static_cast<uint32_t>(made));
    return made;
}

HRESULT D3d12Device::heap_and_resource(const D3D12DDIARG_CREATEHEAP_0001 *heap_args, D3D12DDI_HHEAP heap_handle,
                                       D3D12DDI_HRTRESOURCE runtime, const D3D12DDIARG_CREATERESOURCE_0109 *args,
                                       const D3D12DDI_CLEAR_VALUES *clear, D3D12DDI_HRESOURCE handle)
{
    AV_SCOPE("D3d12Device::heap_and_resource");
    if (heap_args != nullptr && heap_handle.pDrvPrivate == nullptr) return E_INVALIDARG;
    if (heap_args != nullptr) {
        HeapRecord &heap = *static_cast<HeapRecord *>(heap_handle.pDrvPrivate);
        /* A committed resource's heap comes unsized - D3D12Core passes all
         * ones - since its size is the resource's; a shared one's (a swap
         * chain buffer's) comes sized. */
        D3D12DDIARG_CREATEHEAP_0001 sized = *heap_args;
        const bool unsized = sized.ByteSize == 0u || sized.ByteSize == refused_size;
        /* An explicit heap arrives with a resource too - the buffer D3D12Core
         * makes over all of it, which placed resources then name - but sized,
         * and a buffer: only a committed resource's heap is one host object
         * with it. */
        const bool committed = args != nullptr && (unsized || args->ResourceType != D3D12DDI_RT_BUFFER);
        if (committed && unsized) {
            uint64_t alignment = buffer_alignment;
            sized.ByteSize = allocation_bytes(resource_desc(*args), alignment);
            if (sized.Alignment < alignment) sized.Alignment = alignment;
        }
        const HRESULT made = make_heap(sized, heap, committed);
        if (FAILED(made) || args == nullptr) return made;
        if (handle.pDrvPrivate == nullptr) return E_INVALIDARG;
        /* A committed texture whose heap D3D12Core sized itself is one made
         * shared - a swap chain buffer, which DXGI makes for DWM to compose.
         * The heap's flags say nothing of sharing or display (a driver whose
         * heaps are all kernel allocations need not know); a shared heap's
         * size is what D3D12Core asked the driver for, and a committed one's
         * is left to the driver. The host makes it shared, and it gets the
         * kernel allocation DWM opens it by. */
        const bool shown = committed && !unsized && args->ResourceType == D3D12DDI_RT_TEXTURE2D;
        if (shown) heap.flags |= heap_shared;
        ResourceRecord &resource = *static_cast<ResourceRecord *>(handle.pDrvPrivate);
        const HRESULT   resource_made = make_resource(*args, clear, heap, 0u, resource);
        if (FAILED(resource_made) || !shown) return resource_made;
        const HRESULT shown_made = show(resource, runtime, *args);
        heap.allocation = resource.allocation;
        return shown_made;
    }
    return place_resource(heap_handle, args, clear, handle);
}

/* A resource placed in an existing heap: D3D12Core names the heap by the
 * buffer it made over all of it when the heap was created, and the offset
 * into that buffer, and passes no heap handle. */
HRESULT D3d12Device::place_resource(D3D12DDI_HHEAP heap_handle, const D3D12DDIARG_CREATERESOURCE_0109 *args,
                                    const D3D12DDI_CLEAR_VALUES *clear, D3D12DDI_HRESOURCE handle)
{
    AV_SCOPE("D3d12Device::place_resource");
    if (args == nullptr || handle.pDrvPrivate == nullptr) return E_INVALIDARG;
    const D3D12DDIARG_HRESOURCE_PLACEMENT &at = args->ReuseBufferGPUVA.BaseAddress.UMD;
    const auto *const spanning = static_cast<const ResourceRecord *>(at.hResource.pDrvPrivate);
    /* No heap, and no buffer of one to be placed in: a reserved resource. */
    if (heap_handle.pDrvPrivate == nullptr && spanning == nullptr)
        return reserve_resource(*args, clear, *static_cast<ResourceRecord *>(handle.pDrvPrivate));
    HeapRecord *const in = heap_handle.pDrvPrivate != nullptr ? static_cast<HeapRecord *>(heap_handle.pDrvPrivate)
                           : spanning != nullptr && spanning->header.stamp.holds(ResourceRecord::tag) ? spanning->placed_in
                                                                                                  : nullptr;
    if (in == nullptr || !in->header.stamp.holds(HeapRecord::tag)) return E_INVALIDARG;
    const uint64_t offset = (spanning != nullptr ? spanning->heap_offset : 0u) + at.Offset;
    return make_resource(*args, clear, *in, offset, *static_cast<ResourceRecord *>(handle.pDrvPrivate));
}

VOID D3d12Device::destroy_heap_and_resource(D3D12DDI_HHEAP heap_handle, D3D12DDI_HRESOURCE handle)
{
    AV_SCOPE("D3d12Device::destroy_heap_and_resource");
    if (handle.pDrvPrivate != nullptr) {
        ResourceRecord &resource = *static_cast<ResourceRecord *>(handle.pDrvPrivate);
        if (resource.header.stamp.holds(ResourceRecord::tag))
            static_cast<void>(sent(d3d12proto::av_d3d12_Destroy12(wire(), resource.header.stamp.id()), "Destroy12"));
        resource = ResourceRecord{};
    }
    if (heap_handle.pDrvPrivate == nullptr) return;
    HeapRecord &heap = *static_cast<HeapRecord *>(heap_handle.pDrvPrivate);
    if (!heap.header.stamp.holds(HeapRecord::tag)) return;
    if (heap.made != 0u) static_cast<void>(sent(d3d12proto::av_d3d12_Destroy12(wire(), heap.header.stamp.id()), "Destroy12"));
    if (heap.memory != nullptr) session_.close_memory(*heap.memory);
    heap = HeapRecord{};
}

HRESULT D3d12Device::map_heap(D3D12DDI_HHEAP handle, void **data)
{
    AV_SCOPE("D3d12Device::map_heap");
    if (data == nullptr) return E_INVALIDARG;
    *data = nullptr;
    const HeapRecord &heap = *static_cast<const HeapRecord *>(handle.pDrvPrivate);
    if (heap.memory == nullptr) return E_INVALIDARG;
    *data = heap.memory->view();
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
    info->AdditionalDataHeaderAlignment = no_additional_data_alignment;
    info->AdditionalDataAlignment = no_additional_data_alignment;
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
    info->AdditionalDataHeaderAlignment = no_additional_data_alignment;
    info->AdditionalDataAlignment = no_additional_data_alignment;
}

/* Only a texture in a CPU-visible heap has a layout the CPU sees, and this
 * driver makes none: every CPU-visible heap holds only buffers. */
VOID D3d12Device::check_subresource_info(D3D12DDI_HRESOURCE, UINT, D3D12DDI_SUBRESOURCE_INFO *info)
{
    AV_SCOPE("D3d12Device::check_subresource_info");
    if (info != nullptr) *info = D3D12DDI_SUBRESOURCE_INFO{};
}


}  // namespace av::d3d12
