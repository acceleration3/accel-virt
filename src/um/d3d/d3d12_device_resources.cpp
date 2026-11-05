#include "core/log/log_line.hpp"
#include "core/trace/function_scope.hpp"
#include "core/log/keyed_log.hpp"
#include "guest/transport/host_allocation.hpp"
#include "proto/resources/allocation_descriptor.hpp"
#include "proto/generated/d3d12/d3d12_dispatch_client.h"
#include "um/d3d/d3d12_device.hpp"
#include "um/d3d/d3d_session.hpp"

namespace av::d3d12 {

namespace {
constexpr uint32_t heap_default = 1u;
constexpr uint32_t heap_upload = 2u;
constexpr uint32_t heap_readback = 3u;
constexpr uint32_t heap_shared = 0x1u;
constexpr uint32_t deny_buffers = 0x4u;
constexpr uint32_t deny_rt_ds_textures = 0x40u;
constexpr uint32_t deny_non_rt_ds_textures = 0x80u;
constexpr uint32_t allow_render_target = 0x1u;
constexpr uint32_t allow_depth_stencil = 0x2u;
constexpr uint32_t allow_unordered_access = 0x4u;
constexpr uint32_t deny_shader_resource = 0x8u;
constexpr uint32_t allow_cross_adapter = 0x10u;
constexpr uint32_t allow_simultaneous_access = 0x20u;
constexpr uint32_t state_generic_read = 0xAC3u;
constexpr uint32_t state_copy_dest = 0x400u;
constexpr uint32_t buffer_dimension = 1u;
constexpr uint64_t buffer_alignment = uint64_t{64} * 1024u;
constexpr uint64_t refused_size = ~uint64_t{0};
constexpr uint64_t page_bytes = 4096u;
constexpr UINT32   no_additional_data_alignment = 1u;

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

}

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
        LogLine(lines_).text("no guest address space for a heap of ").number(args.ByteSize).text(" bytes").write();
        return E_OUTOFMEMORY;
    }
    if (heap.type != heap_default) {
        const HRESULT memory = heap_memory(args.ByteSize, heap);
        if (FAILED(memory)) return memory;
    } else if (committed) {
        return S_OK;
    }
    const uint64_t memory = heap.memory != nullptr ? heap.memory->handle() : 0u;
    if (!sent(d3d12proto::av_d3d12_CreateHeap12(wire(), heap.header.stamp.id(), properties_of(heap.type), heap.flags,
                                                 heap.size, args.Alignment, memory, 0u),
              "CreateHeap12"))
        return E_OUTOFMEMORY;
    heap.made = 1u;
    return S_OK;
}

HRESULT D3d12Device::heap_memory(uint64_t bytes, HeapRecord &heap)
{
    AV_SCOPE("D3d12Device::heap_memory");
    if (heap.type == heap_readback && lend_pages(bytes, heap) == Status::ok) return S_OK;
    const HostMemoryRequest memory(heap.type == heap_upload ? HostMemoryRequest::Kind::upload
                                                            : HostMemoryRequest::Kind::readback,
                                   0u);
    if (session_.open_memory(bytes, memory, heap.memory) == Status::ok) return S_OK;
    lines_.write("no host memory for an upload or readback heap");
    return E_OUTOFMEMORY;
}

Status D3d12Device::lend_pages(uint64_t bytes, HeapRecord &heap)
{
    AV_SCOPE("D3d12Device::lend_pages");
    const uint64_t pages = (bytes + page_bytes - 1u) & ~(page_bytes - 1u);
    void *const    own = VirtualAlloc(nullptr, static_cast<SIZE_T>(pages), MEM_RESERVE | MEM_COMMIT | MEM_WRITE_WATCH,
                                      PAGE_READWRITE);
    if (own == nullptr) return Status::no_memory;
    const Status lent =
        session_.adopt_memory(own, pages, HostMemoryRequest(HostMemoryRequest::Kind::readback, 0u), heap.memory);
    if (lent != Status::ok) {
        static_cast<void>(VirtualFree(own, 0u, MEM_RELEASE));
        lines_.write("the device took no write-back heap's pages; its memory is the host's");
        return lent;
    }
    heap.own_pages = own;
    return Status::ok;
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

HRESULT D3d12Device::create_heap_and_resource(const D3D12DDIARG_CREATEHEAP_0001 *heap_args, D3D12DDI_HHEAP heap_handle,
                                              D3D12DDI_HRTRESOURCE runtime, const D3D12DDIARG_CREATERESOURCE_0109 *args,
                                              const D3D12DDI_CLEAR_VALUES *clear,
                                              D3D12DDI_HPROTECTEDRESOURCESESSION_0030, D3D12DDI_HRESOURCE handle)
{
    AV_SCOPE("D3d12Device::create_heap_and_resource");
    const HRESULT made = heap_and_resource(heap_args, heap_handle, runtime, args, clear, handle);
    if (FAILED(made)) refused_.failed("CreateHeapAndResource", made);
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
        D3D12DDIARG_CREATEHEAP_0001 sized = *heap_args;
        const bool unsized = sized.ByteSize == 0u || sized.ByteSize == refused_size;
        const bool committed = args != nullptr && (unsized || args->ResourceType != D3D12DDI_RT_BUFFER);
        if (committed && unsized) {
            uint64_t alignment = buffer_alignment;
            sized.ByteSize = allocation_bytes(resource_desc(*args), alignment);
            if (sized.Alignment < alignment) sized.Alignment = alignment;
        }
        const HRESULT made = make_heap(sized, heap, committed);
        if (FAILED(made) || args == nullptr) return made;
        if (handle.pDrvPrivate == nullptr) return E_INVALIDARG;
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

HRESULT D3d12Device::place_resource(D3D12DDI_HHEAP heap_handle, const D3D12DDIARG_CREATERESOURCE_0109 *args,
                                    const D3D12DDI_CLEAR_VALUES *clear, D3D12DDI_HRESOURCE handle)
{
    AV_SCOPE("D3d12Device::place_resource");
    if (args == nullptr || handle.pDrvPrivate == nullptr) return E_INVALIDARG;
    const D3D12DDIARG_HRESOURCE_PLACEMENT &at = args->ReuseBufferGPUVA.BaseAddress.UMD;
    const auto *const spanning = static_cast<const ResourceRecord *>(at.hResource.pDrvPrivate);
    if (heap_handle.pDrvPrivate == nullptr && spanning == nullptr)
        return reserve_resource(*args, clear, *static_cast<ResourceRecord *>(handle.pDrvPrivate));
    HeapRecord *const in = heap_handle.pDrvPrivate != nullptr ? static_cast<HeapRecord *>(heap_handle.pDrvPrivate)
                           : spanning != nullptr && spanning->header.stamp.holds(ResourceRecord::tag) ? spanning->placed_in
                                                                                                  : nullptr;
    if (in == nullptr || !in->header.stamp.holds(HeapRecord::tag)) return E_INVALIDARG;
    const uint64_t offset = (spanning != nullptr ? spanning->heap_offset : 0u) + at.Offset;
    return make_resource(*args, clear, *in, offset, *static_cast<ResourceRecord *>(handle.pDrvPrivate));
}

D3D12DDI_HEAP_AND_RESOURCE_SIZES D3d12Device::calc_private_opened_heap_and_resource_sizes(
    const D3D12DDIARG_OPENHEAP_0003 *, D3D12DDI_HPROTECTEDRESOURCESESSION_0030)
{
    AV_SCOPE("D3d12Device::calc_private_opened_heap_and_resource_sizes");
    return D3D12DDI_HEAP_AND_RESOURCE_SIZES{sizeof(HeapRecord), sizeof(ResourceRecord)};
}

HRESULT D3d12Device::open_heap_and_resource(const D3D12DDIARG_OPENHEAP_0003 *args, D3D12DDI_HHEAP heap_handle,
                                            D3D12DDI_HRTRESOURCE, D3D12DDI_HPROTECTEDRESOURCESESSION_0030,
                                            D3D12DDI_HRESOURCE handle)
{
    AV_SCOPE("D3d12Device::open_heap_and_resource");
    if (args == nullptr || heap_handle.pDrvPrivate == nullptr || handle.pDrvPrivate == nullptr ||
        args->NumAllocations != 1u || args->pOpenAllocationInfo == nullptr)
        return E_INVALIDARG;
    return open_existing(*args, *static_cast<HeapRecord *>(heap_handle.pDrvPrivate),
                         *static_cast<ResourceRecord *>(handle.pDrvPrivate));
}

HRESULT D3d12Device::open_existing(const D3D12DDIARG_OPENHEAP_0003 &args, HeapRecord &heap, ResourceRecord &resource)
{
    AV_SCOPE("D3d12Device::open_existing");
    const D3DDDI_OPENALLOCATIONINFO &opened = args.pOpenAllocationInfo[0];
    AllocationDescriptor             described;
    if (opened.pPrivateDriverData == nullptr ||
        described.decode(static_cast<const uint8_t *>(opened.pPrivateDriverData), opened.PrivateDriverDataSize) !=
            Status::ok ||
        described.kind() != AllocationKind::cross_adapter || !described.cpu_visible() || described.layout().height() != 1u ||
        described.layout().format() != 0u) {
        unsupported("pfnOpenHeapAndResource of a shared heap");
        return E_NOTIMPL;
    }
    heap = HeapRecord{};
    stamp(heap.header, HeapRecord::tag, next_id());
    heap.type = heap_readback;
    heap.size = described.size();
    heap.address = addresses_.reserve(heap.size);
    if (heap.address == 0u) return E_OUTOFMEMORY;
    if (lender_.lend(opened.hAllocation, heap.size, heap.memory) != Status::ok) {
        lines_.write("the device took no existing heap's pages");
        return E_OUTOFMEMORY;
    }
    if (!sent(d3d12proto::av_d3d12_CreateHeap12(wire(), heap.header.stamp.id(), properties_of(heap.type), 0u, heap.size,
                                                 buffer_alignment, heap.memory->handle(), 0u),
              "CreateHeap12"))
        return E_OUTOFMEMORY;
    heap.made = 1u;
    D3D12DDIARG_CREATERESOURCE_0109 spanning = {};
    spanning.ResourceType = D3D12DDI_RT_BUFFER;
    spanning.Width = heap.size;
    spanning.Height = 1u;
    spanning.DepthOrArraySize = 1u;
    spanning.MipLevels = 1u;
    spanning.Format = DXGI_FORMAT_UNKNOWN;
    spanning.SampleDesc.Count = 1u;
    spanning.Layout = D3D12DDI_TL_ROW_MAJOR;
    spanning.Flags = D3D12DDI_RESOURCE_FLAG_0003_SHADER_RESOURCE;
    return make_resource(spanning, nullptr, heap, 0u, resource);
}

void D3d12Device::give_back(const ResourceRecord &resource)
{
    AV_SCOPE("D3d12Device::give_back");
    if (resource.allocation == 0u) return;
    D3D12DDICB_DEALLOCATE_0022 deallocate = {};
    deallocate.hResource = resource.runtime_resource;
    const HRESULT freed = core_->pfnDeallocateCb(runtime_, &deallocate);
    if (FAILED(freed)) refused_.failed("pfnDeallocateCb for a shown resource", freed);
}

VOID D3d12Device::destroy_heap_and_resource(D3D12DDI_HHEAP heap_handle, D3D12DDI_HRESOURCE handle)
{
    AV_SCOPE("D3d12Device::destroy_heap_and_resource");
    if (handle.pDrvPrivate != nullptr) {
        ResourceRecord &resource = *static_cast<ResourceRecord *>(handle.pDrvPrivate);
        if (resource.header.stamp.holds(ResourceRecord::tag))
        {
            static_cast<void>(sent(d3d12proto::av_d3d12_Destroy12(wire(), resource.header.stamp.id()), "Destroy12"));
            give_back(resource);
        }
        resource = ResourceRecord{};
    }
    if (heap_handle.pDrvPrivate == nullptr) return;
    HeapRecord &heap = *static_cast<HeapRecord *>(heap_handle.pDrvPrivate);
    if (!heap.header.stamp.holds(HeapRecord::tag)) return;
    if (heap.made != 0u) static_cast<void>(sent(d3d12proto::av_d3d12_Destroy12(wire(), heap.header.stamp.id()), "Destroy12"));
    if (heap.memory != nullptr) session_.close_memory(*heap.memory);
    if (heap.own_pages != nullptr) static_cast<void>(VirtualFree(heap.own_pages, 0u, MEM_RELEASE));
    heap = HeapRecord{};
}

HRESULT D3d12Device::map_heap(D3D12DDI_HHEAP handle, void **data)
{
    AV_SCOPE("D3d12Device::map_heap");
    if (data == nullptr) return E_INVALIDARG;
    *data = nullptr;
    const HeapRecord &heap = *static_cast<const HeapRecord *>(handle.pDrvPrivate);
    if (heap.memory == nullptr) return E_INVALIDARG;
    *data = heap.memory->view_at(0u, heap.size);
    return *data != nullptr ? S_OK : E_INVALIDARG;
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

VOID D3d12Device::check_subresource_info(D3D12DDI_HRESOURCE, UINT, D3D12DDI_SUBRESOURCE_INFO *info)
{
    AV_SCOPE("D3d12Device::check_subresource_info");
    if (info != nullptr) *info = D3D12DDI_SUBRESOURCE_INFO{};
}


}
