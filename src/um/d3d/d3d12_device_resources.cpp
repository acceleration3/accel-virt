#include "core/log/log_line.hpp"
#include "core/trace/function_scope.hpp"
#include "core/log/keyed_log.hpp"
#include "guest/transport/host_allocation.hpp"
#include "proto/d3d/d3d9_present_format.hpp"
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
constexpr uint32_t row_major_alignment = 256u;
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
constexpr uint64_t suballocated_heap_bytes_most = uint64_t{4} << 20u;
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
    heap.backing = nullptr;
    heap.address = addresses_.reserve(args.ByteSize);
    if (heap.address == 0u) {
        LogLine(lines_).text("no guest address space for a heap of ").number(args.ByteSize).text(" bytes").write();
        return E_OUTOFMEMORY;
    }
    if (heap.type == heap_default) {
        const HRESULT charged = charge_heap(heap);
        if (FAILED(charged)) {
            addresses_.release(heap.address, args.ByteSize);
            heap.address = 0u;
            return charged;
        }
    }
    if (heap.type != heap_default) {
        const HRESULT memory = heap_memory(args, heap);
        if (FAILED(memory)) {
            addresses_.release(heap.address, args.ByteSize);
            heap.address = 0u;
            return memory;
        }
    } else if (committed) {
        return S_OK;
    }
    const HRESULT sent_heap = send_heap(heap, args.Alignment);
    if (FAILED(sent_heap)) {
        release_heap_memory(heap);
        uncharge_heap(heap);
        addresses_.release(heap.address, args.ByteSize);
        heap.address = 0u;
        return sent_heap;
    }
    heap.made = 1u;
    return S_OK;
}

HRESULT D3d12Device::send_heap(const HeapRecord &heap, uint64_t alignment)
{
    AV_SCOPE("D3d12Device::send_heap");
    const HeapMemory *const backing = heap.backing;
    const uint64_t          size = backing != nullptr ? backing->window_bytes(heap.size) : heap.size;
    const uint64_t          memory = backing != nullptr ? backing->handle() : 0u;
    const uint64_t          offset = backing != nullptr ? backing->offset() : 0u;
    if (heap.charged != 0u && heap.asked == 0u) {
        if (!sent(d3d12proto::av_d3d12_CreateHeapAsync12(wire(), heap.header.stamp.id(), properties_of(heap.type),
                                                          heap.flags, size, alignment, memory, offset),
                  "CreateHeapAsync12"))
            return DXGI_ERROR_DEVICE_REMOVED;
        return S_OK;
    }
    uint32_t hresult = 0u;
    if (!sent(d3d12proto::av_d3d12_CreateHeap12(wire(), heap.header.stamp.id(), properties_of(heap.type), heap.flags,
                                                 size, alignment, memory, offset, &hresult),
              "CreateHeap12"))
        return DXGI_ERROR_DEVICE_REMOVED;
    return static_cast<HRESULT>(hresult);
}

HRESULT D3d12Device::charge_heap(HeapRecord &heap)
{
    AV_SCOPE("D3d12Device::charge_heap");
    const d3d::D3d12Commitment::Admission admission = commitment_.charge(heap.size);
    if (admission == d3d::D3d12Commitment::Admission::refused) {
        LogLine(lines_).text("a heap of ").number(heap.size).text(" bytes is past the ").number(commitment_.budget())
            .text(" the host reports, with ").number(commitment_.committed()).text(" committed").write();
        return E_OUTOFMEMORY;
    }
    heap.charged = heap.size;
    heap.asked = admission == d3d::D3d12Commitment::Admission::ask ? 1u : 0u;
    return S_OK;
}

void D3d12Device::uncharge_heap(HeapRecord &heap)
{
    AV_SCOPE("D3d12Device::uncharge_heap");
    commitment_.release(heap.charged);
    heap.charged = 0u;
}

HRESULT D3d12Device::heap_memory(const D3D12DDIARG_CREATEHEAP_0001 &args, HeapRecord &heap)
{
    AV_SCOPE("D3d12Device::heap_memory");
    const HostMemoryRequest::Kind kind =
        heap.type == heap_upload ? HostMemoryRequest::Kind::upload : HostMemoryRequest::Kind::readback;
    const bool small = args.ByteSize <= suballocated_heap_bytes_most && args.Alignment <= buffer_alignment;
    if (heap.type == heap_readback && memories_.lend_pages(args.ByteSize, heap.backing) == Status::ok) return S_OK;
    if (small && memories_.take_slice(kind, args.ByteSize, heap.backing) == Status::ok) return S_OK;
    if (memories_.open_dedicated(kind, args.ByteSize, heap.backing) == Status::ok) return S_OK;
    lines_.write("no host memory for an upload or readback heap");
    return E_OUTOFMEMORY;
}

void D3d12Device::release_heap_memory(HeapRecord &heap)
{
    AV_SCOPE("D3d12Device::release_heap_memory");
    if (heap.backing != nullptr) heap.backing->release();
    heap.backing = nullptr;
}

void D3d12Device::trim_memory()
{
    AV_SCOPE("D3d12Device::trim_memory");
    session_.shared_memory().reclaim();
    session_.shared_memory().trim();
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
    uint32_t       hresult = 0u;
    resource.placed_in = &heap;
    resource.heap_offset = offset;
    if (heap.made == 0u && heap.asked == 0u) {
        made = d3d12proto::av_d3d12_CreateCommittedResourceAsync12(wire(), resource.header.stamp.id(),
                                                                    properties_of(heap_default), heap.flags & heap_shared,
                                                                    desc, state, value, resource.address);
    } else if (heap.made == 0u) {
        made = d3d12proto::av_d3d12_CreateCommittedResource12(wire(), resource.header.stamp.id(), properties_of(heap_default),
                                                               heap.flags & heap_shared, desc, state, value,
                                                               resource.address, &hresult);
    } else {
        resource.heap = heap.header.stamp.id();
        made = d3d12proto::av_d3d12_CreatePlacedResource12(wire(), resource.header.stamp.id(), heap.header.stamp.id(), offset, desc,
                                                             state, value, resource.address);
    }
    if (!sent(made, "CreateResource12")) return DXGI_ERROR_DEVICE_REMOVED;
    return heap.made == 0u ? static_cast<HRESULT>(hresult) : S_OK;
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
        if (FAILED(resource_made) && heap.made == 0u) {
            uncharge_heap(heap);
            addresses_.release(heap.address, sized.ByteSize);
            heap.address = 0u;
        }
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
        described.kind() != AllocationKind::cross_adapter || !described.cpu_visible()) {
        unsupported("pfnOpenHeapAndResource of a shared heap");
        return E_INVALIDARG;
    }
    const SurfaceLayout &surface = described.layout();
    const bool           rows = surface.height() == 1u && surface.format() == 0u;
    D3d9PresentFormat    presented(0u);
    if (!rows && (!surface.is_surface() || !presented.take_ddi(surface.format()) || surface.pitch() % row_major_alignment != 0u ||
                  surface.bytes() > described.size())) {
        unsupported("pfnOpenHeapAndResource of a shared heap");
        return E_INVALIDARG;
    }
    heap = HeapRecord{};
    stamp(heap.header, HeapRecord::tag, next_id());
    heap.type = heap_readback;
    heap.size = described.size();
    heap.address = addresses_.reserve(heap.size);
    if (heap.address == 0u) return E_OUTOFMEMORY;
    HostAllocation *lent = nullptr;
    if (lender_.lend(opened.hAllocation, heap.size, lent) != Status::ok) {
        lines_.write("the device took no existing heap's pages");
        addresses_.release(heap.address, heap.size);
        heap.address = 0u;
        return E_OUTOFMEMORY;
    }
    if (memories_.adopt(*lent, heap.backing) != Status::ok) {
        lender_.close(*lent);
        addresses_.release(heap.address, heap.size);
        heap.address = 0u;
        return E_OUTOFMEMORY;
    }
    const HRESULT sent_heap = send_heap(heap, buffer_alignment);
    if (FAILED(sent_heap)) {
        release_heap_memory(heap);
        addresses_.release(heap.address, heap.size);
        heap.address = 0u;
        return sent_heap;
    }
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
    const HRESULT made = make_resource(spanning, nullptr, heap, 0u, resource);
    if (FAILED(made) || rows) return made;
    resource.row_pitch = surface.pitch();
    resource.width = surface.width();
    resource.height = surface.height();
    resource.format = presented.dxgi();
    return made;
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
    release_heap_memory(heap);
    uncharge_heap(heap);
    session_.shared_memory().reclaim();
    heap = HeapRecord{};
}

HRESULT D3d12Device::map_heap(D3D12DDI_HHEAP handle, void **data)
{
    AV_SCOPE("D3d12Device::map_heap");
    if (data == nullptr) return E_INVALIDARG;
    *data = nullptr;
    const HeapRecord &heap = *static_cast<const HeapRecord *>(handle.pDrvPrivate);
    if (heap.backing != nullptr) *data = heap.backing->view(heap.size);
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
