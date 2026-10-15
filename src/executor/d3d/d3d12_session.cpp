#include "executor/d3d/d3d12_session.hpp"

#include "executor/sharing/session_shares.hpp"
#include "executor/shim/d3d12_host_device.hpp"
#include "executor/source_memory.hpp"
#include "executor/imported_memories.hpp"

#include "core/log/log_line.hpp"
#include "core/trace/function_scope.hpp"
#include "executor/d3d/d3d12_indirect_signature.hpp"
#include "executor/d3d/d3d12_root_layout.hpp"
#include "executor/guest_region.hpp"
#include "executor/shim/d3d12_host_device.hpp"
#include "executor/shim/d3d_headers.hpp"
#include "executor/shim/surface_identity.hpp"
#include "executor/d3d/shared_surfaces.hpp"
#include "executor/d3d/staged_frame.hpp"
#include "proto/d3d/dxgi_format_layout.hpp"
#include "proto/generated/d3d12/d3d12_command_names.h"
#include "proto/generated/d3d12/d3d12_protocol_ids.h"
#include "proto/resources/surface_point.hpp"
#include "proto/wire/wire_arena.hpp"

#include <stdio.h>
#include <time.h>

#include <unistd.h>

namespace av {

namespace {

/* Direct3D 12's commands name no device: a session is its guest's one. */
constexpr uint32_t only_device = 1u;
constexpr uint32_t descriptor_heap_types = 4u;
/* D3D12_HEAP_TYPE and D3D12_CPU_PAGE_PROPERTY values the wire carries. */
constexpr uint32_t heap_default = 1u;
constexpr uint32_t heap_upload = 2u;
constexpr uint32_t heap_readback = 3u;
constexpr uint32_t heap_custom = 4u;
constexpr uint32_t page_not_available = 1u;
constexpr uint32_t most_heap_type = 4u;
constexpr uint32_t most_dimension = 4u;
constexpr uint32_t buffer_dimension = 1u;
constexpr uint32_t max_texture_dimension = 16384u;
constexpr uint64_t max_heap_bytes = uint64_t{1} << 34u;

bool cpu_visible(const d3d12proto::HeapProperties &properties)
{
    AV_SCOPE("cpu_visible");
    return properties.type == heap_upload || properties.type == heap_readback ||
           (properties.type == heap_custom && properties.cpu_page > page_not_available);
}

D3D12_HEAP_PROPERTIES host_properties(const d3d12proto::HeapProperties &properties)
{
    AV_SCOPE("host_properties");
    D3D12_HEAP_PROPERTIES made = {};
    made.Type = static_cast<D3D12_HEAP_TYPE>(properties.type);
    made.CPUPageProperty = static_cast<D3D12_CPU_PAGE_PROPERTY>(properties.cpu_page);
    made.MemoryPoolPreference = static_cast<D3D12_MEMORY_POOL>(properties.pool);
    made.CreationNodeMask = 1u;
    made.VisibleNodeMask = 1u;
    return made;
}

bool resource_holds(const d3d12proto::ResourceDesc &desc)
{
    AV_SCOPE("resource_holds");
    if (desc.dimension == 0u || desc.dimension > most_dimension || !DxgiFormatLayout(desc.format).known()) return false;
    if (desc.dimension == buffer_dimension) return desc.height == 1u && desc.depth_or_array == 1u;
    return desc.width != 0u && desc.width <= max_texture_dimension && desc.height <= max_texture_dimension &&
           desc.depth_or_array != 0u && desc.sample_count != 0u;
}

D3D12_RESOURCE_DESC host_desc(const d3d12proto::ResourceDesc &desc)
{
    AV_SCOPE("host_desc");
    D3D12_RESOURCE_DESC made = {};
    made.Dimension = static_cast<D3D12_RESOURCE_DIMENSION>(desc.dimension);
    made.Alignment = desc.alignment;
    made.Width = desc.width;
    made.Height = desc.height;
    made.DepthOrArraySize = static_cast<UINT16>(desc.depth_or_array);
    made.MipLevels = static_cast<UINT16>(desc.mip_levels);
    made.Format = static_cast<DXGI_FORMAT>(desc.format);
    made.SampleDesc = {desc.sample_count, desc.sample_quality};
    made.Layout = static_cast<D3D12_TEXTURE_LAYOUT>(desc.layout);
    made.Flags = static_cast<D3D12_RESOURCE_FLAGS>(desc.flags);
    return made;
}

/* The clear value, or null when there is none. */
const D3D12_CLEAR_VALUE *host_clear(const d3d12proto::ClearValue &clear, D3D12_CLEAR_VALUE &made)
{
    AV_SCOPE("host_clear");
    if (clear.present == 0u || !DxgiFormatLayout(clear.format).known()) return nullptr;
    made = {};
    made.Format = static_cast<DXGI_FORMAT>(clear.format);
    for (uint32_t channel = 0u; channel < 4u; channel++) made.Color[channel] = clear.color[channel];
    /* A depth format's clear value is its DepthStencil, which the union
     * overlays on Color's first two floats: written last, it wins where the
     * format is a depth one, and the runtime reads the member the format
     * names. */
    const bool depth = clear.format == DXGI_FORMAT_D32_FLOAT || clear.format == DXGI_FORMAT_D24_UNORM_S8_UINT ||
                       clear.format == DXGI_FORMAT_D16_UNORM || clear.format == DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
    if (depth) made.DepthStencil = {clear.depth, static_cast<UINT8>(clear.stencil)};
    return &made;
}

}  // namespace

D3d12Session::D3d12Session(GuestRegion &region, ImportedMemories &imported, D3dObjectTable &objects,
                           WakeSignal &tenant)
    : region_(region), imported_(imported), device_(nullptr), shown_by_(nullptr), shares_(nullptr), tenant_(tenant),
      joined_(false), surfaces_(nullptr), objects_(objects), addresses_(), fences_(region), wake_(), progress_(wake_), views_(objects_, addresses_),
      pipelines_(objects_), presentation_(), tile_update_(), arena_(nullptr), removed_(0u), sink_(stderr), lines_(sink_, "virtio-gpu-av: d3d12: "), refusal_(lines_)
{
    AV_SCOPE("D3d12Session::D3d12Session");
}

void D3d12Session::initialize(D3d12HostDevice *device, D3d11HostDevice *shown_by, SharedSurfaces *surfaces,
                              SessionShares *shares)
{
    AV_SCOPE("D3d12Session::initialize");
    device_ = device != nullptr && device->placed() ? device : nullptr;
    shown_by_ = shown_by;
    shares_ = shares;
    joined_ = false;
    surfaces_ = surfaces;
    views_.bind(device_ != nullptr ? device_->device() : nullptr);
    pipelines_.bind(device_);
    presentation_.bind(device_, shown_by, shares);
    removed_ = 0u;
}

void D3d12Session::shut_down()
{
    AV_SCOPE("D3d12Session::shut_down");
    drain();
    presentation_.bind(nullptr, nullptr, nullptr);
    fences_.clear();
    progress_.clear();
    wake_.unbind();
    addresses_.clear();
    objects_.remove_kinds(D3dObjectKind::heap12, D3dObjectKind::command_signature12);
    views_.bind(nullptr);
    pipelines_.bind(nullptr);
    if (joined_) device_->dismiss(tenant_);
    joined_ = false;
    device_ = nullptr;
    shown_by_ = nullptr;
    shares_ = nullptr;
}

bool D3d12Session::lost() const
{
    AV_SCOPE("D3d12Session::lost");
    return joined_ && device_->lost();
}

bool D3d12Session::knows(uint32_t command_id) const
{
    AV_SCOPE("D3d12Session::knows");
    return d3d12proto::knows_command(command_id);
}

bool D3d12Session::run(uint32_t command_id, WireReader &payload, WireWriter &reply, WireArena &arena)
{
    AV_SCOPE("D3d12Session::run");
    arena_ = &arena;
    const bool ran = d3d12proto::dispatch_command(*this, command_id, payload, reply, arena);
    arena_ = nullptr;
    if (ran) return true;
    if (!refusal_.removes(d3d12proto::command_name(command_id), only_device, removed_ != 0u)) return false;
    removed_ = static_cast<uint32_t>(DXGI_ERROR_DRIVER_INTERNAL_ERROR);
    return true;
}

bool D3d12Session::poll()
{
    AV_SCOPE("D3d12Session::poll");
    return fences_.update();
}

void D3d12Session::gpu_progress(uint64_t &marked, uint64_t &reached) const
{
    AV_SCOPE("D3d12Session::gpu_progress");
    marked = progress_.marked();
    reached = progress_.reached();
}

void D3d12Session::bind_signal(void (*signal)(void *context), void *context)
{
    AV_SCOPE("D3d12Session::bind_signal");
    wake_.bind(signal, context);
}

uint32_t D3d12Session::removed_reason() const
{
    AV_SCOPE("D3d12Session::removed_reason");
    return removed_;
}

const uint8_t *D3d12Session::guest_bytes(uint64_t offset, uint64_t size)
{
    AV_SCOPE("D3d12Session::guest_bytes");
    return region_.at(offset, size);
}

uint8_t *D3d12Session::guest_bytes_out(uint64_t offset, uint64_t size)
{
    AV_SCOPE("D3d12Session::guest_bytes_out");
    return region_.at(offset, size);
}

ID3D12GraphicsCommandList *D3d12Session::list(uint32_t id) const
{
    AV_SCOPE("D3d12Session::list");
    return objects_.find_as<ID3D12GraphicsCommandList>(id, D3dObjectKind::list12);
}

bool D3d12Session::cpu_handle(const d3d12proto::DescriptorRef &ref, uint32_t type, size_t &handle) const
{
    AV_SCOPE("D3d12Session::cpu_handle");
    ID3D12DescriptorHeap *const heap = objects_.find_as<ID3D12DescriptorHeap>(ref.heap, D3dObjectKind::descriptor_heap12);
    if (heap == nullptr || type >= descriptor_heap_types) return false;
    const D3D12_DESCRIPTOR_HEAP_DESC desc = heap->GetDesc();
    if (static_cast<uint32_t>(desc.Type) != type || ref.index >= desc.NumDescriptors) return false;
    handle = heap->GetCPUDescriptorHandleForHeapStart().ptr +
             static_cast<size_t>(ref.index) * device_->caps().descriptor_sizes[type];
    return true;
}

bool D3d12Session::gpu_handle(const d3d12proto::DescriptorRef &ref, uint64_t &handle) const
{
    AV_SCOPE("D3d12Session::gpu_handle");
    ID3D12DescriptorHeap *const heap = objects_.find_as<ID3D12DescriptorHeap>(ref.heap, D3dObjectKind::descriptor_heap12);
    if (heap == nullptr) return false;
    const D3D12_DESCRIPTOR_HEAP_DESC desc = heap->GetDesc();
    const uint32_t type = static_cast<uint32_t>(desc.Type);
    if ((desc.Flags & D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE) == 0 || ref.index >= desc.NumDescriptors ||
        type >= descriptor_heap_types)
        return false;
    handle = heap->GetGPUDescriptorHandleForHeapStart().ptr +
             static_cast<uint64_t>(ref.index) * device_->caps().descriptor_sizes[type];
    return true;
}

/* A buffer placed in a heap lies inside the buffer D3D12Core made over the
 * whole heap when the heap was created, whose range is registered already.
 * The host placed both in one heap at the same offsets, so an address of the
 * placed buffer translates through that range to exactly where the buffer
 * is, and it is not registered again. A range overlapping one that does not
 * translate so is refused, as before. */
bool D3d12Session::place(uint32_t id, IUnknown *made, uint64_t address, uint64_t width)
{
    AV_SCOPE("D3d12Session::place");
    ID3D12Resource *const resource = static_cast<ID3D12Resource *>(static_cast<void *>(made));
    const uint64_t        host = resource->GetGPUVirtualAddress();
    const bool            within = address != 0u && addresses_.translate(address, width) == host;
    if (address != 0u && !within && !addresses_.insert(address, width, host, id)) {
        made->Release();
        return false;
    }
    if (objects_.keep(id, D3dObjectKind::resource12, made)) return true;
    addresses_.remove(id);
    return false;
}

void D3d12Session::drain()
{
    AV_SCOPE("D3d12Session::drain");
    /* A lost device finishes nothing more, and is not waited five seconds for. */
    if (device_ == nullptr || lost()) return;
    if (!progress_.mark_all()) {
        if (removed_ == 0u)
            lines_.write("a queue refused the drain's signal; the device is removed");
        removed_ = static_cast<uint32_t>(DXGI_ERROR_DEVICE_HUNG);
        return;
    }
    /* The GPU usually finishes within a frame's time, and a blocked wait adds
     * a thread's wake on top of vkd3d-proton's own: the worker looks for a
     * while, then naps. A GPU that never finishes is a hung device, not a
     * worker parked for good - every other D3D stream waits on this one. */
    constexpr uint64_t spin_ns = 2000000u;
    constexpr uint64_t hung_ns = 5000000000u;
    constexpr long     nap_ns = 50000;
    timespec           began;
    clock_gettime(CLOCK_MONOTONIC, &began);
    for (;;) {
        if (progress_.reached() >= progress_.marked()) return;
        timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        const uint64_t spent = static_cast<uint64_t>(now.tv_sec - began.tv_sec) * 1000000000u +
                               static_cast<uint64_t>(now.tv_nsec) - static_cast<uint64_t>(began.tv_nsec);
        if (spent >= hung_ns) {
            if (removed_ == 0u) {
                lines_.write("the GPU did not finish in 5 s; the device is removed");
                progress_.log_stuck(lines_);
            }
            removed_ = static_cast<uint32_t>(DXGI_ERROR_DEVICE_HUNG);
            return;
        }
        if (spent < spin_ns) {
            __builtin_ia32_pause();
            continue;
        }
        const timespec nap = {0, nap_ns};
        nanosleep(&nap, nullptr);
    }
}

// ---------------------------------------------------------------- device

bool D3d12Session::on_OpenDevice12(uint32_t, uint32_t &hresult, d3d12proto::Caps12 &caps)
{
    AV_SCOPE("D3d12Session::on_OpenDevice12");
    caps = d3d12proto::Caps12{};
    if (device_ == nullptr) {
        hresult = static_cast<uint32_t>(DXGI_ERROR_UNSUPPORTED);
        return true;
    }
    if (!joined_) {
        /* A device the sessions before this one lost is made again for it,
         * once they are gone; the wait is for their workers to fault them. */
        constexpr uint32_t renew_wait_ms = 2000u;
        HostShares *const  host = shares_ != nullptr ? shares_->shares() : nullptr;
        if (!device_->renew(renew_wait_ms, host, shown_by_) || !device_->admit(tenant_)) {
            hresult = static_cast<uint32_t>(DXGI_ERROR_DEVICE_REMOVED);
            return true;
        }
        joined_ = true;
        views_.bind(device_->device());
        pipelines_.bind(device_);
        presentation_.bind(device_, shown_by_, shares_);
    }
    hresult = static_cast<uint32_t>(S_OK);
    caps = device_->caps();
    return true;
}

bool D3d12Session::on_QueryFormat12(uint32_t format, uint32_t samples, uint32_t &support1, uint32_t &support2,
                                    uint32_t &quality_levels)
{
    AV_SCOPE("D3d12Session::on_QueryFormat12");
    support1 = support2 = quality_levels = 0u;
    if (device_ == nullptr || !DxgiFormatLayout(format).known()) return device_ != nullptr;
    D3D12_FEATURE_DATA_FORMAT_SUPPORT support = {};
    support.Format = static_cast<DXGI_FORMAT>(format);
    if (SUCCEEDED(device_->device()->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT, &support, sizeof(support)))) {
        support1 = static_cast<uint32_t>(support.Support1);
        support2 = static_cast<uint32_t>(support.Support2);
    }
    D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS levels = {};
    levels.Format = support.Format;
    levels.SampleCount = samples;
    if (samples != 0u && SUCCEEDED(device_->device()->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS,
                                                                           &levels, sizeof(levels))))
        quality_levels = levels.NumQualityLevels;
    return true;
}

bool D3d12Session::on_Destroy12(uint32_t id)
{
    AV_SCOPE("D3d12Session::on_Destroy12");
    D3dObjectKind kind = D3dObjectKind::none;
    if (objects_.find_any(id, kind) == nullptr || kind < D3dObjectKind::heap12 ||
        kind > D3dObjectKind::command_signature12)
        return false;
    if (kind == D3dObjectKind::resource12) addresses_.remove(id);
    if (kind == D3dObjectKind::fence12) fences_.forget(id);
    if (kind == D3dObjectKind::queue12) progress_.forget(id);
    objects_.remove(id);
    return true;
}

bool D3d12Session::on_Finish12(uint32_t &hresult)
{
    AV_SCOPE("D3d12Session::on_Finish12");
    hresult = removed_;
    if (device_ == nullptr) return true;
    drain();
    if (hresult == 0u) hresult = static_cast<uint32_t>(device_->device()->GetDeviceRemovedReason());
    fences_.update();
    return true;
}

// ---------------------------------------------------------------- memory

/* A CPU-visible heap is host memory the guest opened for it and imported
 * (docs/host-memory.md), named by that import, which vkd3d-proton opens as a
 * heap by its dma-buf over the source's own mapping of it. */
bool D3d12Session::on_CreateHeap12(uint32_t heap, const d3d12proto::HeapProperties &properties, uint32_t flags,
                                   uint64_t size, uint64_t alignment, uint64_t memory)
{
    AV_SCOPE("D3d12Session::on_CreateHeap12");
    if (device_ == nullptr || properties.type == 0u || properties.type > most_heap_type || size == 0u ||
        size > max_heap_bytes)
        return false;
    if (cpu_visible(properties)) {
        const SourceMemory *const backing = imported_.find(memory);
        if (backing == nullptr || size > backing->size()) {
            LogLine(lines_).text("a heap of ").number(size).text(" bytes names no memory that holds it").write();
            return false;
        }
        const int          fd = backing->share();
        ID3D12Heap *const  made = device_->import_dmabuf_heap(backing->pointer(), fd, backing->size());
        if (fd >= 0) close(fd);
        if (made == nullptr) {
            LogLine(lines_).text("the host did not open a heap over ").number(backing->size()).text(" bytes").write();
            return false;
        }
        return objects_.keep(heap, D3dObjectKind::heap12, made);
    }
    D3D12_HEAP_DESC desc = {};
    desc.SizeInBytes = size;
    desc.Properties = host_properties(properties);
    desc.Alignment = alignment;
    desc.Flags = static_cast<D3D12_HEAP_FLAGS>(flags);
    ID3D12Heap *made = nullptr;
    if (FAILED(device_->device()->CreateHeap(&desc, __uuidof(ID3D12Heap), reinterpret_cast<void **>(&made))))
        return false;
    return objects_.keep(heap, D3dObjectKind::heap12, made);
}

bool D3d12Session::on_CreateCommittedResource12(uint32_t resource, const d3d12proto::HeapProperties &properties,
                                                uint32_t heap_flags, const d3d12proto::ResourceDesc &desc,
                                                uint32_t state, const d3d12proto::ClearValue &clear,
                                                uint64_t address)
{
    AV_SCOPE("D3d12Session::on_CreateCommittedResource12");
    /* Memory the CPU sees is the guest's, so it is a heap of guest pages
     * (CreateHeap12) and a placed resource, never a heap of the host's own. */
    if (device_ == nullptr || properties.type != heap_default || !resource_holds(desc)) return false;
    const D3D12_HEAP_PROPERTIES heap = host_properties(properties);
    const D3D12_RESOURCE_DESC   made_desc = host_desc(desc);
    D3D12_CLEAR_VALUE           value = {};
    ID3D12Resource             *made = nullptr;
    if (FAILED(device_->device()->CreateCommittedResource(&heap, static_cast<D3D12_HEAP_FLAGS>(heap_flags), &made_desc,
                                                          static_cast<D3D12_RESOURCE_STATES>(state),
                                                          host_clear(clear, value), __uuidof(ID3D12Resource),
                                                          reinterpret_cast<void **>(&made))))
        return false;
    return place(resource, made, desc.dimension == buffer_dimension ? address : 0u, desc.width);
}

bool D3d12Session::on_ShareResource12(uint32_t resource, uint64_t key)
{
    AV_SCOPE("D3d12Session::on_ShareResource12");
    ID3D12Resource *const shown = objects_.find_as<ID3D12Resource>(resource, D3dObjectKind::resource12);
    return shown != nullptr && presentation_.share(shown, key);
}

bool D3d12Session::on_ResourceAllocation12(const d3d12proto::ResourceDesc &desc, uint64_t &size,
                                           uint64_t &alignment)
{
    AV_SCOPE("D3d12Session::on_ResourceAllocation12");
    size = ~uint64_t{0};
    alignment = 0u;
    if (device_ == nullptr || !resource_holds(desc)) return true;
    const D3D12_RESOURCE_DESC made = host_desc(desc);
    const D3D12_RESOURCE_ALLOCATION_INFO info = device_->device()->GetResourceAllocationInfo(0u, 1u, &made);
    size = info.SizeInBytes;
    alignment = info.Alignment;
    return true;
}

bool D3d12Session::on_CreateReservedResource12(uint32_t resource, const d3d12proto::ResourceDesc &desc,
                                               uint32_t state, const d3d12proto::ClearValue &clear, uint64_t address)
{
    AV_SCOPE("D3d12Session::on_CreateReservedResource12");
    if (device_ == nullptr || !resource_holds(desc)) return false;
    const D3D12_RESOURCE_DESC made_desc = host_desc(desc);
    D3D12_CLEAR_VALUE         value = {};
    ID3D12Resource           *made = nullptr;
    if (FAILED(device_->device()->CreateReservedResource(&made_desc, static_cast<D3D12_RESOURCE_STATES>(state),
                                                         host_clear(clear, value), __uuidof(ID3D12Resource),
                                                         reinterpret_cast<void **>(&made))))
        return false;
    return place(resource, made, desc.dimension == buffer_dimension ? address : 0u, desc.width);
}

bool D3d12Session::on_CreatePlacedResource12(uint32_t resource, uint32_t heap, uint64_t offset,
                                             const d3d12proto::ResourceDesc &desc, uint32_t state,
                                             const d3d12proto::ClearValue &clear, uint64_t address)
{
    AV_SCOPE("D3d12Session::on_CreatePlacedResource12");
    ID3D12Heap *const in = objects_.find_as<ID3D12Heap>(heap, D3dObjectKind::heap12);
    if (device_ == nullptr || in == nullptr || !resource_holds(desc)) return false;
    const D3D12_HEAP_DESC heap_desc = in->GetDesc();
    if (offset > heap_desc.SizeInBytes || (desc.dimension == buffer_dimension && desc.width > heap_desc.SizeInBytes - offset))
        return false;
    return placed(resource, in, offset, desc, state, clear, address);
}

bool D3d12Session::placed(uint32_t resource, ID3D12Heap *in, uint64_t offset, const d3d12proto::ResourceDesc &desc,
                          uint32_t state, const d3d12proto::ClearValue &clear, uint64_t address)
{
    AV_SCOPE("D3d12Session::placed");
    const D3D12_RESOURCE_DESC made_desc = host_desc(desc);
    D3D12_CLEAR_VALUE         value = {};
    ID3D12Resource           *made = nullptr;
    if (FAILED(device_->device()->CreatePlacedResource(in, offset, &made_desc, static_cast<D3D12_RESOURCE_STATES>(state),
                                                       host_clear(clear, value), __uuidof(ID3D12Resource),
                                                       reinterpret_cast<void **>(&made))))
        return false;
    return place(resource, made, desc.dimension == buffer_dimension ? address : 0u, desc.width);
}

// ---------------------------------------------------------------- descriptors

bool D3d12Session::on_CreateDescriptorHeap12(uint32_t heap, uint32_t type, uint32_t count, uint32_t flags)
{
    AV_SCOPE("D3d12Session::on_CreateDescriptorHeap12");
    constexpr uint32_t most_descriptors = 1000000u;
    if (device_ == nullptr || type >= descriptor_heap_types || count == 0u || count > most_descriptors) return false;
    /* vkd3d-proton makes a shader-visible sampler heap of at most the API's
     * 2048, while the guest's runtime has the driver report more than that
     * (D3d12HostCaps): an application that makes the heap it was told of - as
     * wgpu makes one of the size reported - gets 2048, and a sampler past them
     * is refused as one outside any heap is. Applications hand sampler slots
     * out from the start. */
    constexpr uint32_t visible_samplers = 2048u;   /* D3D12_MAX_SHADER_VISIBLE_SAMPLER_HEAP_SIZE */
    const bool         capped = type == D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER &&
                        (flags & D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE) != 0u && count > visible_samplers;
    D3D12_DESCRIPTOR_HEAP_DESC desc = {};
    desc.Type = static_cast<D3D12_DESCRIPTOR_HEAP_TYPE>(type);
    desc.NumDescriptors = capped ? visible_samplers : count;
    desc.Flags = static_cast<D3D12_DESCRIPTOR_HEAP_FLAGS>(flags);
    ID3D12DescriptorHeap *made = nullptr;
    if (FAILED(device_->device()->CreateDescriptorHeap(&desc, __uuidof(ID3D12DescriptorHeap),
                                                       reinterpret_cast<void **>(&made))))
        return false;
    return objects_.keep(heap, D3dObjectKind::descriptor_heap12, made);
}

bool D3d12Session::on_CreateShaderResourceView12(const d3d12proto::DescriptorRef &at, uint32_t has_desc,
                                                 const d3d12proto::ViewDesc &desc)
{
    AV_SCOPE("D3d12Session::on_CreateShaderResourceView12");
    size_t handle = 0u;
    return cpu_handle(at, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, handle) &&
           views_.shader_resource(handle, has_desc != 0u, desc);
}

bool D3d12Session::on_CreateUnorderedAccessView12(const d3d12proto::DescriptorRef &at, uint32_t has_desc,
                                                  const d3d12proto::ViewDesc &desc)
{
    AV_SCOPE("D3d12Session::on_CreateUnorderedAccessView12");
    size_t handle = 0u;
    return cpu_handle(at, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, handle) &&
           views_.unordered_access(handle, has_desc != 0u, desc);
}

bool D3d12Session::on_CreateRenderTargetView12(const d3d12proto::DescriptorRef &at, uint32_t has_desc,
                                               const d3d12proto::ViewDesc &desc)
{
    AV_SCOPE("D3d12Session::on_CreateRenderTargetView12");
    size_t handle = 0u;
    return cpu_handle(at, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, handle) &&
           views_.render_target(handle, has_desc != 0u, desc);
}

bool D3d12Session::on_CreateDepthStencilView12(const d3d12proto::DescriptorRef &at, uint32_t has_desc,
                                               const d3d12proto::ViewDesc &desc)
{
    AV_SCOPE("D3d12Session::on_CreateDepthStencilView12");
    size_t handle = 0u;
    return cpu_handle(at, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, handle) &&
           views_.depth_stencil(handle, has_desc != 0u, desc);
}

bool D3d12Session::on_CreateConstantBufferView12(const d3d12proto::DescriptorRef &at, uint64_t address,
                                                 uint32_t size)
{
    AV_SCOPE("D3d12Session::on_CreateConstantBufferView12");
    size_t handle = 0u;
    return cpu_handle(at, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, handle) &&
           views_.constant_buffer(handle, address, size);
}

bool D3d12Session::on_CreateSampler12(const d3d12proto::DescriptorRef &at, const d3d12proto::SamplerDesc &desc)
{
    AV_SCOPE("D3d12Session::on_CreateSampler12");
    size_t handle = 0u;
    return cpu_handle(at, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, handle) && views_.sampler(handle, desc);
}

bool D3d12Session::on_CopyDescriptors12(const d3d12proto::DescriptorRef &destination,
                                        const d3d12proto::DescriptorRef &source, uint32_t count)
{
    AV_SCOPE("D3d12Session::on_CopyDescriptors12");
    ID3D12DescriptorHeap *const from = objects_.find_as<ID3D12DescriptorHeap>(source.heap, D3dObjectKind::descriptor_heap12);
    if (device_ == nullptr || from == nullptr || count == 0u) return false;
    const uint32_t type = static_cast<uint32_t>(from->GetDesc().Type);
    const d3d12proto::DescriptorRef last_from = {source.heap, source.index + count - 1u};
    const d3d12proto::DescriptorRef last_to = {destination.heap, destination.index + count - 1u};
    size_t to_handle = 0u, from_handle = 0u, ignored = 0u;
    if (source.index + count < source.index || destination.index + count < destination.index ||
        !cpu_handle(destination, type, to_handle) || !cpu_handle(source, type, from_handle) ||
        !cpu_handle(last_from, type, ignored) || !cpu_handle(last_to, type, ignored))
        return false;
    device_->device()->CopyDescriptorsSimple(count, D3D12_CPU_DESCRIPTOR_HANDLE{to_handle},
                                             D3D12_CPU_DESCRIPTOR_HANDLE{from_handle},
                                             static_cast<D3D12_DESCRIPTOR_HEAP_TYPE>(type));
    return true;
}

// ---------------------------------------------------------------- pipelines

bool D3d12Session::on_CreateRootSignature12(uint32_t root, uint32_t flags, uint32_t parameter_count,
                                            const d3d12proto::RootParameter *parameters, uint32_t range_count,
                                            const d3d12proto::DescriptorRange *ranges, uint32_t sampler_count,
                                            const d3d12proto::StaticSampler *samplers)
{
    AV_SCOPE("D3d12Session::on_CreateRootSignature12");
    return objects_.keep(root, D3dObjectKind::root_signature12,
                pipelines_.root_signature(flags, parameters, parameter_count, ranges, range_count, samplers,
                                          sampler_count));
}

/* The layout a root argument is judged against is the one kept on the root
 * signature when it was made (D3d12RootLayout). */
bool D3d12Session::on_CreateCommandSignature12(uint32_t signature, uint32_t root, uint32_t byte_stride,
                                               uint32_t argument_count, const d3d12proto::IndirectArgument *arguments)
{
    AV_SCOPE("D3d12Session::on_CreateCommandSignature12");
    ID3D12RootSignature *const rooted =
        root == 0u ? nullptr : objects_.find_as<ID3D12RootSignature>(root, D3dObjectKind::root_signature12);
    D3d12RootLayout layout;
    if (device_ == nullptr || (root != 0u && (rooted == nullptr || !layout.read(rooted)))) return false;
    const D3d12IndirectSignature made(arguments, argument_count, byte_stride);
    if (!made.holds(rooted != nullptr ? &layout : nullptr)) return false;
    return objects_.keep(signature, D3dObjectKind::command_signature12, made.make(device_->device(), rooted));
}

bool D3d12Session::on_CreateGraphicsPipeline12(uint32_t pipeline, uint32_t root, const d3d12proto::GraphicsState &state,
                                               uint32_t element_count, const d3d12proto::InputElement *elements,
                                               uint32_t signature_count, const d3d12proto::SignatureEntry *signatures,
                                               const uint8_t *vs, uint64_t vs_size, const uint8_t *hs,
                                               uint64_t hs_size, const uint8_t *ds, uint64_t ds_size,
                                               const uint8_t *gs, uint64_t gs_size, const uint8_t *ps,
                                               uint64_t ps_size)
{
    AV_SCOPE("D3d12Session::on_CreateGraphicsPipeline12");
    if (arena_ == nullptr) return false;
    const uint8_t *const code[D3d12PipelineBuilder::stage_count] = {vs, hs, ds, gs, ps};
    const uint64_t sizes[D3d12PipelineBuilder::stage_count] = {vs_size, hs_size, ds_size, gs_size, ps_size};
    return objects_.keep(pipeline, D3dObjectKind::pipeline12,
                pipelines_.graphics(root, state, elements, element_count, signatures, signature_count, code, sizes,
                                    *arena_));
}

bool D3d12Session::on_CreateComputePipeline12(uint32_t pipeline, uint32_t root, const uint8_t *cs, uint64_t cs_size)
{
    AV_SCOPE("D3d12Session::on_CreateComputePipeline12");
    if (arena_ == nullptr) return false;
    return objects_.keep(pipeline, D3dObjectKind::pipeline12, pipelines_.compute(root, cs, cs_size, *arena_));
}

// ---------------------------------------------------------------- queues, lists, fences

bool D3d12Session::on_CreateCommandQueue12(uint32_t queue, uint32_t type, uint32_t priority, uint32_t flags)
{
    AV_SCOPE("D3d12Session::on_CreateCommandQueue12");
    constexpr uint32_t most_list_type = 3u;
    if (device_ == nullptr || type > most_list_type) return false;
    D3D12_COMMAND_QUEUE_DESC desc = {};
    desc.Type = static_cast<D3D12_COMMAND_LIST_TYPE>(type);
    desc.Priority = static_cast<INT>(priority);
    desc.Flags = static_cast<D3D12_COMMAND_QUEUE_FLAGS>(flags);
    ID3D12CommandQueue *made = nullptr;
    if (FAILED(device_->device()->CreateCommandQueue(&desc, __uuidof(ID3D12CommandQueue),
                                                     reinterpret_cast<void **>(&made))))
        return false;
    return objects_.keep(queue, D3dObjectKind::queue12, made);
}

bool D3d12Session::on_CreateCommandAllocator12(uint32_t allocator, uint32_t type)
{
    AV_SCOPE("D3d12Session::on_CreateCommandAllocator12");
    constexpr uint32_t most_list_type = 3u;
    if (device_ == nullptr || type > most_list_type) return false;
    ID3D12CommandAllocator *made = nullptr;
    if (FAILED(device_->device()->CreateCommandAllocator(static_cast<D3D12_COMMAND_LIST_TYPE>(type),
                                                         __uuidof(ID3D12CommandAllocator),
                                                         reinterpret_cast<void **>(&made))))
        return false;
    return objects_.keep(allocator, D3dObjectKind::allocator12, made);
}

bool D3d12Session::on_ResetCommandAllocator12(uint32_t allocator)
{
    AV_SCOPE("D3d12Session::on_ResetCommandAllocator12");
    ID3D12CommandAllocator *const made = objects_.find_as<ID3D12CommandAllocator>(allocator, D3dObjectKind::allocator12);
    return made != nullptr && SUCCEEDED(made->Reset());
}

bool D3d12Session::on_CreateCommandList12(uint32_t id, uint32_t allocator, uint32_t type, uint32_t pipeline)
{
    AV_SCOPE("D3d12Session::on_CreateCommandList12");
    ID3D12CommandAllocator *const with = objects_.find_as<ID3D12CommandAllocator>(allocator, D3dObjectKind::allocator12);
    ID3D12PipelineState    *const initial = objects_.find_as<ID3D12PipelineState>(pipeline, D3dObjectKind::pipeline12);
    if (device_ == nullptr || with == nullptr || (pipeline != 0u && initial == nullptr)) return false;
    ID3D12GraphicsCommandList *made = nullptr;
    if (FAILED(device_->device()->CreateCommandList(0u, static_cast<D3D12_COMMAND_LIST_TYPE>(type), with, initial,
                                                    __uuidof(ID3D12GraphicsCommandList),
                                                    reinterpret_cast<void **>(&made))))
        return false;
    return objects_.keep(id, D3dObjectKind::list12, made);
}

bool D3d12Session::on_ResetCommandList12(uint32_t id, uint32_t allocator, uint32_t pipeline)
{
    AV_SCOPE("D3d12Session::on_ResetCommandList12");
    ID3D12GraphicsCommandList *const recorded = list(id);
    ID3D12CommandAllocator    *const with = objects_.find_as<ID3D12CommandAllocator>(allocator, D3dObjectKind::allocator12);
    ID3D12PipelineState       *const initial = objects_.find_as<ID3D12PipelineState>(pipeline, D3dObjectKind::pipeline12);
    if (recorded == nullptr || with == nullptr || (pipeline != 0u && initial == nullptr)) return false;
    return SUCCEEDED(recorded->Reset(with, initial));
}

bool D3d12Session::on_CloseCommandList12(uint32_t id)
{
    AV_SCOPE("D3d12Session::on_CloseCommandList12");
    ID3D12GraphicsCommandList *const recorded = list(id);
    return recorded != nullptr && SUCCEEDED(recorded->Close());
}

bool D3d12Session::on_CreateFence12(uint32_t fence, uint64_t initial, uint32_t flags)
{
    AV_SCOPE("D3d12Session::on_CreateFence12");
    if (device_ == nullptr) return false;
    ID3D12Fence *made = nullptr;
    if (FAILED(device_->device()->CreateFence(initial, static_cast<D3D12_FENCE_FLAGS>(flags), __uuidof(ID3D12Fence),
                                              reinterpret_cast<void **>(&made))))
        return false;
    return objects_.keep(fence, D3dObjectKind::fence12, made);
}

/* The frame a blt reads is copied on the presenting queue here, behind the
 * work that drew it and ahead of whatever the queue is given next; only for a
 * surface a blt has read before (SharedSurfaces). One the host cannot stage
 * is a frame not shown, never a removed device. */
bool D3d12Session::on_PresentResource12(uint32_t queue, uint32_t resource, uint32_t point)
{
    AV_SCOPE("D3d12Session::on_PresentResource12");
    ID3D12CommandQueue *const from = objects_.find_as<ID3D12CommandQueue>(queue, D3dObjectKind::queue12);
    ID3D12Resource *const     shown = objects_.find_as<ID3D12Resource>(resource, D3dObjectKind::resource12);
    if (from == nullptr || shown == nullptr || !presentation_.present(from)) return false;
    const SurfacePoint frame(SurfaceIdentity(shown).surface(), point);
    if (surfaces_ == nullptr || !surfaces_->presented(frame, SharedSurfaces::Staging::once_blt_read)) return true;
    StagedFrame *const staged = device_->stage_frame(from, shown);
    surfaces_->hold(frame, staged);
    return true;
}

bool D3d12Session::on_ExecuteCommandLists12(uint32_t queue, uint32_t count, const uint32_t *lists)
{
    AV_SCOPE("D3d12Session::on_ExecuteCommandLists12");
    constexpr uint32_t most_lists = 64u;
    ID3D12CommandQueue *const on = objects_.find_as<ID3D12CommandQueue>(queue, D3dObjectKind::queue12);
    if (on == nullptr || count > most_lists) return false;
    /* A refused command left a list short of what the guest recorded into it
     * - a descriptor heap never bound, a view never written - and the GPU
     * would read through the hole: the host GPU faults, and every context on
     * it is reset with this one. Nothing runs on a removed device. */
    if (removed_ != 0u) return true;
    ID3D12CommandList *executed[most_lists] = {};
    for (uint32_t index = 0u; index < count; index++) {
        ID3D12GraphicsCommandList *const recorded = list(lists[index]);
        if (recorded == nullptr) return false;
        executed[index] = recorded;
    }
    if (count == 0u) return true;
    if (!progress_.after_siblings(queue, on)) return false;
    on->ExecuteCommandLists(count, executed);
    return progress_.executed(queue, on, device_->device());
}

bool D3d12Session::on_SignalFence12(uint32_t queue, uint32_t fence, uint64_t value)
{
    AV_SCOPE("D3d12Session::on_SignalFence12");
    ID3D12Fence *const signalled = objects_.find_as<ID3D12Fence>(fence, D3dObjectKind::fence12);
    if (signalled == nullptr) return false;
    if (queue == 0u) return SUCCEEDED(signalled->Signal(value));
    ID3D12CommandQueue *const on = objects_.find_as<ID3D12CommandQueue>(queue, D3dObjectKind::queue12);
    if (on == nullptr || FAILED(on->Signal(signalled, value))) return false;
    fences_.expect(fence, value);
    return true;
}

bool D3d12Session::on_WaitFence12(uint32_t queue, uint32_t fence, uint64_t value)
{
    AV_SCOPE("D3d12Session::on_WaitFence12");
    ID3D12Fence        *const waited = objects_.find_as<ID3D12Fence>(fence, D3dObjectKind::fence12);
    ID3D12CommandQueue *const on = objects_.find_as<ID3D12CommandQueue>(queue, D3dObjectKind::queue12);
    return waited != nullptr && on != nullptr && SUCCEEDED(on->Wait(waited, value));
}

bool D3d12Session::on_FenceValue12(uint32_t fence, uint32_t wait, uint64_t value, uint64_t &completed)
{
    AV_SCOPE("D3d12Session::on_FenceValue12");
    completed = 0u;
    ID3D12Fence *const asked = objects_.find_as<ID3D12Fence>(fence, D3dObjectKind::fence12);
    if (asked == nullptr) return false;
    if (wait != 0u && asked->GetCompletedValue() < value) asked->SetEventOnCompletion(value, nullptr);
    completed = asked->GetCompletedValue();
    return true;
}

bool D3d12Session::on_PublishFence12(uint32_t fence, uint64_t offset)
{
    AV_SCOPE("D3d12Session::on_PublishFence12");
    ID3D12Fence *const published = objects_.find_as<ID3D12Fence>(fence, D3dObjectKind::fence12);
    if (published == nullptr && offset != D3d12FencePublisher::stop) return false;
    return fences_.publish(fence, published, offset);
}

bool D3d12Session::on_CreateQueryHeap12(uint32_t heap, uint32_t type, uint32_t count)
{
    AV_SCOPE("D3d12Session::on_CreateQueryHeap12");
    constexpr uint32_t most_query_heap_type = 5u;
    constexpr uint32_t most_queries = 65536u;
    if (device_ == nullptr || type > most_query_heap_type || count == 0u || count > most_queries) return false;
    D3D12_QUERY_HEAP_DESC desc = {};
    desc.Type = static_cast<D3D12_QUERY_HEAP_TYPE>(type);
    desc.Count = count;
    ID3D12QueryHeap *made = nullptr;
    if (FAILED(device_->device()->CreateQueryHeap(&desc, __uuidof(ID3D12QueryHeap), reinterpret_cast<void **>(&made))))
        return false;
    return objects_.keep(heap, D3dObjectKind::query_heap12, made);
}

bool D3d12Session::invariants_hold() const
{
    AV_SCOPE("D3d12Session::invariants_hold");
    return objects_.invariants_hold() && addresses_.invariants_hold() && fences_.invariants_hold() &&
           (device_ != nullptr ||
            objects_.count_kinds(D3dObjectKind::heap12, D3dObjectKind::command_signature12) == 0u);
}

}  // namespace av
