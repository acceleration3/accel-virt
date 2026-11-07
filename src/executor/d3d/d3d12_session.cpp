#include "executor/d3d/d3d12_session.hpp"

#include "executor/teardown_budget.hpp"

#include "executor/sharing/session_shares.hpp"
#include "executor/shim/d3d12_host_device.hpp"
#include "executor/source_memory.hpp"
#include "executor/import_table.hpp"

#include "core/log/log_line.hpp"
#include "core/trace/function_scope.hpp"
#include "executor/d3d/watched_blocks.hpp"
#include "executor/d3d/d3d12_heap_flags.hpp"
#include "executor/d3d/d3d12_indirect_signature.hpp"
#include "executor/d3d/d3d12_resource_flags.hpp"
#include "executor/d3d/d3d12_root_layout.hpp"
#include "core/platform/poll_budget.hpp"
#include "executor/shim/d3d12_host_device.hpp"
#include "executor/shim/d3d_headers.hpp"
#include "executor/shim/surface_identity.hpp"
#include "executor/d3d/shared_surfaces.hpp"
#include "executor/d3d/frame_stager.hpp"
#include "executor/d3d/staged_frame.hpp"
#include "proto/d3d/dxgi_format_layout.hpp"
#include "proto/d3d/heap_slice_support.hpp"
#include "proto/generated/d3d12/d3d12_command_names.h"
#include "proto/generated/d3d12/d3d12_protocol_ids.h"
#include "proto/resources/surface_point.hpp"
#include "proto/wire/wire_arena.hpp"



namespace av {

namespace {
constexpr uint32_t only_device = 1u;
constexpr uint32_t descriptor_heap_types = 4u;
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
constexpr uint64_t hung_after = 5000000000u;

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
    made.Flags = static_cast<D3D12_RESOURCE_FLAGS>(D3d12ResourceFlags(desc.flags).kept());
    return made;
}

const D3D12_CLEAR_VALUE *host_clear(const d3d12proto::ClearValue &clear, D3D12_CLEAR_VALUE &made)
{
    AV_SCOPE("host_clear");
    if (clear.present == 0u || !DxgiFormatLayout(clear.format).known()) return nullptr;
    made = {};
    made.Format = static_cast<DXGI_FORMAT>(clear.format);
    for (uint32_t channel = 0u; channel < 4u; channel++) made.Color[channel] = clear.color[channel];
    const bool depth = clear.format == DXGI_FORMAT_D32_FLOAT || clear.format == DXGI_FORMAT_D24_UNORM_S8_UINT ||
                       clear.format == DXGI_FORMAT_D16_UNORM || clear.format == DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
    if (depth) made.DepthStencil = {clear.depth, static_cast<UINT8>(clear.stencil)};
    return &made;
}

class PresentedResource final : public FrameStager {
public:
    PresentedResource(D3d12HostDevice &device, ID3D12CommandQueue *queue, ID3D12Resource *resource);

    StagedFrame *stage() override;

private:
    D3d12HostDevice    &device_;
    ID3D12CommandQueue *queue_;
    ID3D12Resource     *resource_;
};

PresentedResource::PresentedResource(D3d12HostDevice &device, ID3D12CommandQueue *queue, ID3D12Resource *resource)
    : device_(device), queue_(queue), resource_(resource)
{
    AV_SCOPE("PresentedResource::PresentedResource");
}

StagedFrame *PresentedResource::stage()
{
    AV_SCOPE("PresentedResource::stage");
    return device_.stage_frame(queue_, resource_);
}

}

D3d12Session::D3d12Session(const GuestBytes &guest, D3dObjectTable &objects, WakeSignal &tenant,
                           EventfdWake &fence_wake, WatchedBlocks &blocks, DriverLog &lines)
    : d3d12proto::ServerDispatch(guest), imports_(nullptr), device_(nullptr), shown_by_(nullptr), shares_(nullptr), tenant_(tenant),
      joined_(false), surfaces_(nullptr), objects_(objects), blocks_(blocks), addresses_(blocks),
      progress_(fence_wake, blocks), block_heaps_(blocks, progress_, lines), holds_(blocks), descriptors_(blocks, holds_), allocators_(blocks), queries_(blocks), views_(objects_, addresses_),
      pipelines_(objects_), compiles_(blocks), presentation_(lines), tile_update_(), retained_(), removed_(0u), lines_(lines), refusal_(lines_), clock_(), hang_ns_(hung_after), commands_(0u), polled_at_(0u), drain_commands_(0u), drain_mark_(0u), drain_marked_(false)
{
    AV_SCOPE("D3d12Session::D3d12Session");
}

void D3d12Session::initialize(const ImportTable &imports, D3d12HostDevice *device, D3d11HostDevice *shown_by,
                              SharedSurfaces *surfaces, SessionShares *shares)
{
    AV_SCOPE("D3d12Session::initialize");
    imports_ = &imports;
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
    compiles_.stop();
    for (D3d12PipelineJob *job = compiles_.oldest(); job != nullptr; job = compiles_.oldest()) {
        ID3D12PipelineState *const made = compiles_.collect(*job);
        if (made != nullptr) made->Release();
    }
    compiles_.bind(nullptr);
    const bool settled = drain_marked_ && drain_commands_ == commands_ && progress_.marked() == drain_mark_ &&
                         progress_.reached() >= drain_mark_;
    drain_marked_ = false;
    retained_.clear();
    const bool drained = settled || drain();
    if (drained) {
        descriptors_.release_all();
        holds_.release_all();
    } else {
        descriptors_.forget_all();
        holds_.forget_all();
    }
    allocators_.clear();
    queries_.clear();
    presentation_.bind(nullptr, nullptr, nullptr);
    progress_.clear();
    addresses_.clear();
    objects_.remove_kinds(D3dObjectKind::heap12, D3dObjectKind::command_signature12);
    if (drained) block_heaps_.destroy_all();
    else block_heaps_.leak_all();
    views_.bind(nullptr);
    pipelines_.bind(nullptr);
    if (joined_) device_->dismiss(tenant_);
    joined_ = false;
    device_ = nullptr;
    shown_by_ = nullptr;
    shares_ = nullptr;
    imports_ = nullptr;
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
    commands_++;
    block_heaps_.reclaim();
    if (commands_ - polled_at_ >= poll_every) refresh_progress();
    const bool ran = d3d12proto::dispatch_command(*this, command_id, payload, reply, arena);
    if (ran) return true;
    if (!refusal_.removes(d3d12proto::command_name(command_id), only_device, removed_ != 0u)) return false;
    if (removed_ == 0u) removed_ = blocks_.refusal(static_cast<uint32_t>(DXGI_ERROR_DRIVER_INTERNAL_ERROR));
    return true;
}

void D3d12Session::end_batch()
{
    AV_SCOPE("D3d12Session::end_batch");
    refresh_progress();
}

void D3d12Session::refresh_progress()
{
    AV_SCOPE("D3d12Session::refresh_progress");
    polled_at_ = commands_;
    progress_.begin_poll();
    if (holds_.retired() != 0u) holds_.reap(progress_.reached_this_poll());
}

void D3d12Session::gpu_progress(uint64_t &marked, uint64_t &reached) const
{
    AV_SCOPE("D3d12Session::gpu_progress");
    marked = progress_.marked();
    reached = progress_.reached();
}

void D3d12Session::abandon(const SourceMemory &memory)
{
    AV_SCOPE("D3d12Session::abandon");
    block_heaps_.abandon(memory);
}

uint32_t D3d12Session::removed_reason() const
{
    AV_SCOPE("D3d12Session::removed_reason");
    return removed_;
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
             static_cast<size_t>(ref.index) * device_->capabilities().caps().descriptor_sizes[type];
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
             static_cast<uint64_t>(ref.index) * device_->capabilities().caps().descriptor_sizes[type];
    return true;
}

bool D3d12Session::place(uint32_t id, IUnknown *made, uint64_t address, uint64_t width)
{
    AV_SCOPE("D3d12Session::place");
    ID3D12Resource *const resource = static_cast<ID3D12Resource *>(static_cast<void *>(made));
    const uint64_t        host = resource->GetGPUVirtualAddress();
    const bool            within = address != 0u && addresses_.translate(address, width) == host;
    const bool            registering = address != 0u && !within;
    D3dObjectKind  taken = D3dObjectKind::none;
    const Status   registered = objects_.find_any(id, taken) != nullptr ? Status::conflict
                                : registering ? addresses_.insert(address, width, host, id)
                                              : Status::ok;
    if (registered != Status::ok) {
        LogLine(lines_).text("a buffer's address kept nowhere, status ").number(static_cast<uint32_t>(registered))
            .text(", ").number(addresses_.count()).text(" ranges held").write();
        made->Release();
        return false;
    }
    if (kept(id, D3dObjectKind::resource12, made)) return true;
    if (registering) addresses_.remove(id);
    return false;
}

bool D3d12Session::kept(uint32_t id, D3dObjectKind kind, IUnknown *made)
{
    AV_SCOPE("D3d12Session::kept");
    const Status kept = objects_.keep(id, kind, made);
    if (kept == Status::ok) return true;
    if (made != nullptr)
        LogLine(lines_).text("an object kept nowhere: id ").number(id).text(", status ")
            .number(static_cast<uint32_t>(kept)).text(", ").number(objects_.count()).text(" objects held").write();
    return false;
}

bool D3d12Session::retain(uint32_t list, IUnknown *object, D3d12BlockHeap *block)
{
    AV_SCOPE("D3d12Session::retain");
    if (retained_.covers(list, object, block)) {
        holds_.let_go_pin(block, 0u);
        return true;
    }
    const Status held = holds_.hold(list, object, block);
    if (held == Status::ok) {
        retained_.note(list, object, block != nullptr);
        return true;
    }
    LogLine(lines_).text("list ").number(list).text(" kept nothing alive, status ").number(static_cast<uint32_t>(held))
        .write();
    return false;
}

bool D3d12Session::retain_object(uint32_t list, uint32_t id, D3dObjectKind kind)
{
    AV_SCOPE("D3d12Session::retain_object");
    IUnknown *const object = objects_.find(id, kind);
    return object != nullptr && retain(list, object, nullptr);
}

bool D3d12Session::retain_resource(uint32_t list, uint32_t resource)
{
    AV_SCOPE("D3d12Session::retain_resource");
    IUnknown *const object = objects_.find(resource, D3dObjectKind::resource12);
    if (object == nullptr) return false;
    if (retained_.whole(list, object)) return true;
    if (!retain(list, object, block_heaps_.pin(resource))) return false;
    bool mapped = true;
    holds_.for_each_held(resource, [&](IUnknown *heap) { mapped = retain(list, heap, nullptr) && mapped; });
    if (mapped) retained_.note_whole(list, object);
    return mapped;
}

bool D3d12Session::retain_slot(uint32_t list, const d3d12proto::DescriptorRef &ref)
{
    AV_SCOPE("D3d12Session::retain_slot");
    IUnknown *const resource = descriptors_.resource_at(ref.heap, ref.index);
    return resource == nullptr || retain(list, resource, holds_.repinned(descriptors_.pin_at(ref.heap, ref.index)));
}

bool D3d12Session::retain_address(uint32_t list, uint64_t address)
{
    AV_SCOPE("D3d12Session::retain_address");
    if (address == 0u) return true;
    uint64_t       offset = 0u;
    const uint32_t owner = addresses_.owner(address, offset);
    return owner != 0u && retain_resource(list, owner);
}

void D3d12Session::release_holds(uint32_t list)
{
    AV_SCOPE("D3d12Session::release_holds");
    retained_.clear();
    if (holds_.release_list(list, progress_.reached()) == Status::ok) return;
    if (!drain()) return;
    static_cast<void>(holds_.release_list(list, progress_.reached()));
}

bool D3d12Session::noted(const d3d12proto::DescriptorRef &at, uint32_t resource, uint32_t counter)
{
    AV_SCOPE("D3d12Session::noted");
    IUnknown *const viewed = resource == 0u ? nullptr : objects_.find(resource, D3dObjectKind::resource12);
    IUnknown *const counted = counter == 0u ? nullptr : objects_.find(counter, D3dObjectKind::resource12);
    const D3d12DescriptorHolds::View view = {viewed, counted, viewed == nullptr ? nullptr : block_heaps_.pin(resource),
                                             counted == nullptr ? nullptr : block_heaps_.pin(counter)};
    const Status    kept = descriptors_.note(at.heap, at.index, view, progress_.marked(),
                                             [this]() { return progress_.reached_this_poll(); });
    if (kept == Status::ok) return true;
    LogLine(lines_).text("descriptor ").number(at.index).text(" of heap ").number(at.heap)
        .text(" kept nothing alive, status ").number(static_cast<uint32_t>(kept)).write();
    return false;
}

bool D3d12Session::noted_address(const d3d12proto::DescriptorRef &at, uint64_t address)
{
    AV_SCOPE("D3d12Session::noted_address");
    uint64_t offset = 0u;
    return noted(at, address == 0u ? 0u : addresses_.owner(address, offset), 0u);
}

void D3d12Session::retire(IUnknown *object)
{
    AV_SCOPE("D3d12Session::retire");
    if (object == nullptr) return;
    if (holds_.retire(object, progress_.marked(), progress_.reached()) == Status::ok) return;
    if (drain()) object->Release();
}

bool D3d12Session::follow_queue(uint32_t queue, ID3D12CommandQueue *on, IUnknown *first, IUnknown *second)
{
    AV_SCOPE("D3d12Session::follow_queue");
    if (!progress_.executed(queue, on, device_->device())) {
        LogLine(lines_).text("the progress of queue ").number(queue).text(" is not followed after its tile work; the device is removed").write();
        static_cast<void>(drain());
        return false;
    }
    IUnknown *const used[2] = {first, second};
    for (IUnknown *const object : used) {
        if (object == nullptr) continue;
        object->AddRef();
        retire(object);
    }
    return true;
}

bool D3d12Session::drain()
{
    AV_SCOPE("D3d12Session::drain");
    if (device_ == nullptr) return true;
    if (lost()) return false;
    if (!progress_.mark_all()) {
        if (removed_ == 0u)
            lines_.write("a queue refused the drain's signal; the device is removed");
        removed_ = static_cast<uint32_t>(DXGI_ERROR_DEVICE_HUNG);
        return false;
    }
    if (wait_for(progress_.marked())) return true;
    progress_.log_stuck(lines_);
    return false;
}

bool D3d12Session::drain_within(TeardownBudget &budget)
{
    AV_SCOPE("D3d12Session::drain_within");
    if (device_ == nullptr) return true;
    if (lost()) return false;
    if (!compiles_.settle(budget)) return false;
    if (!drain_marked_ || drain_commands_ != commands_ || progress_.marked() != drain_mark_) {
        if (!progress_.mark_all()) return false;
        drain_mark_ = progress_.marked();
        drain_commands_ = commands_;
        drain_marked_ = true;
    }
    if (progress_.reached() >= drain_mark_) return true;
    constexpr uint64_t spin_ns = 2000000u;
    constexpr uint64_t nap_ns = 50000u;
    PollBudget         polls(clock_, spin_ns, nap_ns, budget.remaining());
    while (polls.next())
        if (progress_.reached() >= drain_mark_) return true;
    return false;
}

bool D3d12Session::wait_for(uint64_t mark)
{
    AV_SCOPE("D3d12Session::wait_for");
    if (device_ == nullptr || progress_.reached() >= mark) return true;
    if (lost()) return false;
    constexpr uint64_t spin_ns = 2000000u;
    constexpr uint64_t nap_ns = 50000u;
    PollBudget         polls(clock_, spin_ns, nap_ns, hang_ns_);
    while (polls.next())
        if (progress_.reached() >= mark) return true;
    if (removed_ == 0u) lines_.write("the GPU did not finish in 5 s; the device is removed");
    removed_ = static_cast<uint32_t>(DXGI_ERROR_DEVICE_HUNG);
    return false;
}

bool D3d12Session::on_OpenDevice12(uint32_t, uint32_t &hresult)
{
    AV_SCOPE("D3d12Session::on_OpenDevice12");
    if (device_ == nullptr) {
        hresult = static_cast<uint32_t>(DXGI_ERROR_UNSUPPORTED);
        return true;
    }
    if (!joined_) {
        constexpr uint32_t renew_wait_ms = 2000u;
        if (!device_->renew(renew_wait_ms) || !device_->admit(tenant_)) {
            hresult = static_cast<uint32_t>(DXGI_ERROR_DEVICE_REMOVED);
            return true;
        }
        joined_ = true;
        views_.bind(device_->device());
        pipelines_.bind(device_);
        compiles_.bind(device_->device());
        presentation_.bind(device_, shown_by_, shares_);
    }
    hresult = static_cast<uint32_t>(S_OK);
    return true;
}

bool D3d12Session::on_QueryCaps12(uint32_t &hresult, d3d12proto::Caps12 &caps, uint8_t *formats,
                                  uint64_t formats_capacity, uint64_t &formats_size)
{
    AV_SCOPE("D3d12Session::on_QueryCaps12");
    caps = d3d12proto::Caps12{};
    formats_size = 0u;
    if (device_ == nullptr) {
        hresult = static_cast<uint32_t>(DXGI_ERROR_UNSUPPORTED);
        return true;
    }
    hresult = static_cast<uint32_t>(S_OK);
    caps = device_->capabilities().caps();
    formats_size = device_->capabilities().formats().give(formats, formats_capacity);
    return true;
}

bool D3d12Session::on_Destroy12(uint32_t id)
{
    AV_SCOPE("D3d12Session::on_Destroy12");
    if (D3d12PipelineJob *const job = compiles_.pending(id)) static_cast<void>(settle(*job));
    D3dObjectKind kind = D3dObjectKind::none;
    if (objects_.find_any(id, kind) == nullptr || kind < D3dObjectKind::heap12 ||
        kind > D3dObjectKind::command_signature12)
        return false;
    if (kind == D3dObjectKind::resource12) {
        addresses_.remove(id);
        block_heaps_.detach(id);
    }
    if (kind == D3dObjectKind::heap12) block_heaps_.release_heap(id);
    if (kind == D3dObjectKind::queue12) progress_.forget(id);
    if (kind == D3dObjectKind::list12) {
        release_holds(id);
        allocators_.forget_list(id);
    }
    if (kind == D3dObjectKind::allocator12) allocators_.forget_allocator(id);
    if (kind == D3dObjectKind::query_heap12) queries_.forget(id);
    if (kind == D3dObjectKind::resource12) release_holds(id);
    if (kind == D3dObjectKind::descriptor_heap12) {
        const Status let = descriptors_.destroy_heap(id, objects_.find(id, kind), progress_.marked(),
                                                     [this]() { return progress_.reached_this_poll(); });
        if (let != Status::ok) drain();
    }
    retire(objects_.take(id));
    return true;
}

bool D3d12Session::on_Finish12(uint32_t &hresult)
{
    AV_SCOPE("D3d12Session::on_Finish12");
    settle_pipelines();
    hresult = removed_;
    if (device_ == nullptr) return true;
    drain();
    if (hresult == 0u) hresult = static_cast<uint32_t>(device_->device()->GetDeviceRemovedReason());
    return true;
}

bool D3d12Session::on_QueryHeapSlices12(uint32_t &supported)
{
    AV_SCOPE("D3d12Session::on_QueryHeapSlices12");
    supported = static_cast<uint32_t>(device_ != nullptr ? HeapSliceSupport::windows : HeapSliceSupport::none);
    return true;
}

bool D3d12Session::window_heap(uint32_t heap, uint64_t size, uint64_t memory, uint64_t base)
{
    AV_SCOPE("D3d12Session::window_heap");
    SourceMemory *const backing = imports_ != nullptr ? imports_->named(memory) : nullptr;
    if (backing == nullptr) {
        LogLine(lines_).text("a heap of ").number(size).text(" bytes names no memory, handle ").number(memory).write();
        return false;
    }
    ID3D12Heap *const block = block_heaps_.open(*device_, *backing, heap, base, size);
    if (block == nullptr) return false;
    block->AddRef();
    if (kept(heap, D3dObjectKind::heap12, block)) return true;
    block_heaps_.release_heap(heap);
    return false;
}

bool D3d12Session::on_CreateHeap12(uint32_t heap, const d3d12proto::HeapProperties &properties, uint32_t flags,
                                   uint64_t size, uint64_t alignment, uint64_t memory, uint64_t offset,
                                   uint32_t &hresult)
{
    AV_SCOPE("D3d12Session::on_CreateHeap12");
    hresult = static_cast<uint32_t>(S_OK);
    if (properties.type == 0u || properties.type > most_heap_type || size == 0u || D3d12HeapFlags(flags).refused())
        return false;
    if (size > max_heap_bytes) {
        LogLine(lines_).text("a heap of ").number(size).text(" bytes is past the ").number(max_heap_bytes).text(" the host makes").write();
        hresult = static_cast<uint32_t>(E_OUTOFMEMORY);
        return true;
    }
    if (device_ == nullptr) return false;
    if (cpu_visible(properties)) return window_heap(heap, size, memory, offset);
    D3D12_HEAP_DESC desc = {};
    desc.SizeInBytes = size;
    desc.Properties = host_properties(properties);
    desc.Alignment = alignment;
    desc.Flags = static_cast<D3D12_HEAP_FLAGS>(D3d12HeapFlags(flags).kept());
    ID3D12Heap   *made = nullptr;
    const HRESULT result = device_->device()->CreateHeap(&desc, __uuidof(ID3D12Heap), reinterpret_cast<void **>(&made));
    if (FAILED(result)) {
        LogLine(lines_).text("the host made no heap of ").number(size).text(" bytes (").hex(static_cast<uint32_t>(result), 8u).text(")").write();
        hresult = static_cast<uint32_t>(result);
        return true;
    }
    return kept(heap, D3dObjectKind::heap12, made);
}

bool D3d12Session::on_CreateCommittedResource12(uint32_t resource, const d3d12proto::HeapProperties &properties,
                                                uint32_t heap_flags, const d3d12proto::ResourceDesc &desc,
                                                uint32_t state, const d3d12proto::ClearValue &clear,
                                                uint64_t address, uint32_t &hresult)
{
    AV_SCOPE("D3d12Session::on_CreateCommittedResource12");
    hresult = static_cast<uint32_t>(S_OK);
    if (device_ == nullptr || properties.type != heap_default || !resource_holds(desc) ||
        D3d12HeapFlags(heap_flags).refused())
        return false;
    const D3D12_HEAP_PROPERTIES heap = host_properties(properties);
    const D3D12_RESOURCE_DESC   made_desc = host_desc(desc);
    D3D12_CLEAR_VALUE           value = {};
    ID3D12Resource             *made = nullptr;
    const HRESULT result = device_->device()->CreateCommittedResource(
        &heap, static_cast<D3D12_HEAP_FLAGS>(D3d12HeapFlags(heap_flags).kept()), &made_desc,
        static_cast<D3D12_RESOURCE_STATES>(state), host_clear(clear, value), __uuidof(ID3D12Resource),
        reinterpret_cast<void **>(&made));
    if (FAILED(result)) {
        LogLine(lines_).text("the host made no resource of ").number(desc.width).text(" bytes (").hex(static_cast<uint32_t>(result), 8u).text(")").write();
        hresult = static_cast<uint32_t>(result);
        return true;
    }
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
    uint64_t              base = 0u;
    uint64_t              extent = heap_desc.SizeInBytes;
    const bool            windowed = block_heaps_.window(heap, base, extent);
    const bool            buffer = desc.dimension == buffer_dimension;
    uint64_t              bytes = desc.width;
    if (windowed && !buffer) {
        const D3D12_RESOURCE_DESC sized = host_desc(desc);
        bytes = device_->device()->GetResourceAllocationInfo(0u, 1u, &sized).SizeInBytes;
    }
    if (offset > extent || ((buffer || windowed) && bytes > extent - offset)) {
        LogLine(lines_).text("a resource placed past its heap: offset ").number(offset).text(", ").number(bytes)
            .text(" bytes wide, in a heap of ").number(extent).write();
        return false;
    }
    if (windowed && !block_heaps_.attach(resource, heap)) return false;
    const bool made = placed(resource, in, base + offset, desc, state, clear, address);
    if (windowed && !made) block_heaps_.detach(resource);
    return made;
}

bool D3d12Session::placed(uint32_t resource, ID3D12Heap *in, uint64_t offset, const d3d12proto::ResourceDesc &desc,
                          uint32_t state, const d3d12proto::ClearValue &clear, uint64_t address)
{
    AV_SCOPE("D3d12Session::placed");
    const D3D12_RESOURCE_DESC made_desc = host_desc(desc);
    D3D12_CLEAR_VALUE         value = {};
    ID3D12Resource           *made = nullptr;
    const HRESULT made_result =
        device_->device()->CreatePlacedResource(in, offset, &made_desc, static_cast<D3D12_RESOURCE_STATES>(state),
                                                host_clear(clear, value), __uuidof(ID3D12Resource),
                                                reinterpret_cast<void **>(&made));
    if (FAILED(made_result)) {
        const D3D12_HEAP_DESC heap_desc = in->GetDesc();
        LogLine(lines_).text("a placed resource refused (").hex(static_cast<uint32_t>(made_result), 8u)
            .text("): dimension ").number(desc.dimension).text(" format ").number(desc.format).text(" ")
            .number(desc.width).text("x").number(desc.height).text("x").number(desc.depth_or_array).text(" mips ")
            .number(desc.mip_levels).text(" samples ").number(desc.sample_count).text(" layout ").number(desc.layout)
            .text(" flags ").hex(desc.flags, 8u).text(" alignment ").number(desc.alignment).text(" at ").number(offset)
            .text(" of a heap of ").number(heap_desc.SizeInBytes).text(" type ").number(heap_desc.Properties.Type)
            .text(" flags ").hex(heap_desc.Flags, 8u).write();
        return false;
    }
    return place(resource, made, desc.dimension == buffer_dimension ? address : 0u, desc.width);
}

bool D3d12Session::on_CreateDescriptorHeap12(uint32_t heap, uint32_t type, uint32_t count, uint32_t flags)
{
    AV_SCOPE("D3d12Session::on_CreateDescriptorHeap12");
    constexpr uint32_t most_descriptors = 1000000u;
    if (device_ == nullptr || type >= descriptor_heap_types || count == 0u || count > most_descriptors) return false;
    constexpr uint32_t visible_samplers = 2048u;
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
    return kept(heap, D3dObjectKind::descriptor_heap12, made);
}

bool D3d12Session::on_CreateShaderResourceView12(const d3d12proto::DescriptorRef &at, uint32_t has_desc,
                                                 const d3d12proto::ViewDesc &desc)
{
    AV_SCOPE("D3d12Session::on_CreateShaderResourceView12");
    size_t handle = 0u;
    return cpu_handle(at, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, handle) &&
           views_.shader_resource(handle, has_desc != 0u, desc) && noted(at, desc.resource, 0u);
}

bool D3d12Session::on_CreateUnorderedAccessView12(const d3d12proto::DescriptorRef &at, uint32_t has_desc,
                                                  const d3d12proto::ViewDesc &desc)
{
    AV_SCOPE("D3d12Session::on_CreateUnorderedAccessView12");
    size_t handle = 0u;
    return cpu_handle(at, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, handle) &&
           views_.unordered_access(handle, has_desc != 0u, desc) && noted(at, desc.resource, desc.counter_resource);
}

bool D3d12Session::on_CreateRenderTargetView12(const d3d12proto::DescriptorRef &at, uint32_t has_desc,
                                               const d3d12proto::ViewDesc &desc)
{
    AV_SCOPE("D3d12Session::on_CreateRenderTargetView12");
    size_t handle = 0u;
    return cpu_handle(at, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, handle) &&
           views_.render_target(handle, has_desc != 0u, desc) && noted(at, desc.resource, 0u);
}

bool D3d12Session::on_CreateDepthStencilView12(const d3d12proto::DescriptorRef &at, uint32_t has_desc,
                                               const d3d12proto::ViewDesc &desc)
{
    AV_SCOPE("D3d12Session::on_CreateDepthStencilView12");
    size_t handle = 0u;
    return cpu_handle(at, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, handle) &&
           views_.depth_stencil(handle, has_desc != 0u, desc) && noted(at, desc.resource, 0u);
}

bool D3d12Session::on_CreateConstantBufferView12(const d3d12proto::DescriptorRef &at, uint64_t address,
                                                 uint32_t size)
{
    AV_SCOPE("D3d12Session::on_CreateConstantBufferView12");
    size_t handle = 0u;
    return cpu_handle(at, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, handle) &&
           views_.constant_buffer(handle, address, size) && noted_address(at, address);
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
    return descriptors_.copy(destination.heap, destination.index, source.heap, source.index, count, progress_.marked(),
                             [this]() { return progress_.reached_this_poll(); }) == Status::ok;
}

bool D3d12Session::on_CreateRootSignature12(uint32_t root, uint32_t flags, uint32_t parameter_count,
                                            const d3d12proto::RootParameter *parameters, uint32_t range_count,
                                            const d3d12proto::DescriptorRange *ranges, uint32_t sampler_count,
                                            const d3d12proto::StaticSampler *samplers)
{
    AV_SCOPE("D3d12Session::on_CreateRootSignature12");
    return kept(root, D3dObjectKind::root_signature12,
                pipelines_.root_signature(flags, parameters, parameter_count, ranges, range_count, samplers,
                                          sampler_count));
}

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
    return kept(signature, D3dObjectKind::command_signature12, made.make(device_->device(), rooted));
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
    const uint8_t *const code[D3d12PipelineBuilder::stage_count] = {vs, hs, ds, gs, ps};
    const uint64_t sizes[D3d12PipelineBuilder::stage_count] = {vs_size, hs_size, ds_size, gs_size, ps_size};
    D3d12PipelineJob *const job = vacant_job();
    if (job == nullptr) return false;
    if (!pipelines_.graphics(pipeline, root, state, elements, element_count, signatures, signature_count, code, sizes,
                             *job)) {
        job->clear();
        return false;
    }
    compiles_.start(*job);
    return true;
}

bool D3d12Session::on_CreateComputePipeline12(uint32_t pipeline, uint32_t root, const uint8_t *cs, uint64_t cs_size)
{
    AV_SCOPE("D3d12Session::on_CreateComputePipeline12");
    D3d12PipelineJob *const job = vacant_job();
    if (job == nullptr) return false;
    if (!pipelines_.compute(pipeline, root, cs, cs_size, *job)) {
        job->clear();
        return false;
    }
    compiles_.start(*job);
    return true;
}

ID3D12PipelineState *D3d12Session::pipeline(uint32_t id)
{
    AV_SCOPE("D3d12Session::pipeline");
    if (D3d12PipelineJob *const job = compiles_.pending(id)) static_cast<void>(settle(*job));
    return objects_.find_as<ID3D12PipelineState>(id, D3dObjectKind::pipeline12);
}

D3d12PipelineJob *D3d12Session::vacant_job()
{
    AV_SCOPE("D3d12Session::vacant_job");
    for (D3d12PipelineJob *job = compiles_.vacant(); job == nullptr; job = compiles_.vacant()) {
        D3d12PipelineJob *const oldest = compiles_.oldest();
        if (oldest == nullptr) return nullptr;
        static_cast<void>(settle(*oldest));
    }
    return compiles_.vacant();
}

bool D3d12Session::settle(D3d12PipelineJob &job)
{
    AV_SCOPE("D3d12Session::settle");
    const uint32_t id = job.id();
    const uint32_t command = job.graphics() ? d3d12proto::kCmd_CreateGraphicsPipeline12
                                            : d3d12proto::kCmd_CreateComputePipeline12;
    if (kept(id, D3dObjectKind::pipeline12, compiles_.collect(job))) return true;
    if (refusal_.removes(d3d12proto::command_name(command), only_device, removed_ != 0u) && removed_ == 0u)
        removed_ = static_cast<uint32_t>(DXGI_ERROR_DRIVER_INTERNAL_ERROR);
    return false;
}

void D3d12Session::settle_pipelines()
{
    AV_SCOPE("D3d12Session::settle_pipelines");
    for (D3d12PipelineJob *job = compiles_.oldest(); job != nullptr; job = compiles_.oldest())
        static_cast<void>(settle(*job));
}

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
    return kept(queue, D3dObjectKind::queue12, made);
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
    return kept(allocator, D3dObjectKind::allocator12, made);
}

bool D3d12Session::on_ResetCommandAllocator12(uint32_t allocator)
{
    AV_SCOPE("D3d12Session::on_ResetCommandAllocator12");
    ID3D12CommandAllocator *const made = objects_.find_as<ID3D12CommandAllocator>(allocator, D3dObjectKind::allocator12);
    return made != nullptr && wait_for(allocators_.mark_of(allocator)) && SUCCEEDED(made->Reset());
}

bool D3d12Session::on_CreateCommandList12(uint32_t id, uint32_t allocator, uint32_t type, uint32_t pipeline)
{
    AV_SCOPE("D3d12Session::on_CreateCommandList12");
    ID3D12CommandAllocator *const with = objects_.find_as<ID3D12CommandAllocator>(allocator, D3dObjectKind::allocator12);
    ID3D12PipelineState    *const initial = pipeline == 0u ? nullptr : this->pipeline(pipeline);
    if (device_ == nullptr || with == nullptr || (pipeline != 0u && initial == nullptr)) return false;
    ID3D12GraphicsCommandList *made = nullptr;
    if (FAILED(device_->device()->CreateCommandList(0u, static_cast<D3D12_COMMAND_LIST_TYPE>(type), with, initial,
                                                    __uuidof(ID3D12GraphicsCommandList),
                                                    reinterpret_cast<void **>(&made))))
        return false;
    if (!kept(id, D3dObjectKind::list12, made)) return false;
    if (allocators_.bind(id, allocator) != Status::ok) return false;
    return retain(id, with, nullptr) && (initial == nullptr || retain(id, initial, nullptr));
}

bool D3d12Session::on_ResetCommandList12(uint32_t id, uint32_t allocator, uint32_t pipeline)
{
    AV_SCOPE("D3d12Session::on_ResetCommandList12");
    ID3D12GraphicsCommandList *const recorded = list(id);
    ID3D12CommandAllocator    *const with = objects_.find_as<ID3D12CommandAllocator>(allocator, D3dObjectKind::allocator12);
    ID3D12PipelineState       *const initial = pipeline == 0u ? nullptr : this->pipeline(pipeline);
    if (recorded == nullptr || with == nullptr || (pipeline != 0u && initial == nullptr)) return false;
    if (FAILED(recorded->Reset(with, initial))) return false;
    release_holds(id);
    if (allocators_.bind(id, allocator) != Status::ok) return false;
    return retain(id, with, nullptr) && (initial == nullptr || retain(id, initial, nullptr));
}

bool D3d12Session::on_CloseCommandList12(uint32_t id)
{
    AV_SCOPE("D3d12Session::on_CloseCommandList12");
    ID3D12GraphicsCommandList *const recorded = list(id);
    return recorded != nullptr && SUCCEEDED(recorded->Close());
}

bool D3d12Session::on_PresentResource12(uint32_t queue, uint32_t resource, uint32_t point)
{
    AV_SCOPE("D3d12Session::on_PresentResource12");
    ID3D12CommandQueue *const from = objects_.find_as<ID3D12CommandQueue>(queue, D3dObjectKind::queue12);
    ID3D12Resource *const     shown = objects_.find_as<ID3D12Resource>(resource, D3dObjectKind::resource12);
    if (from == nullptr || shown == nullptr || !presentation_.present(from)) return false;
    if (surfaces_ == nullptr) return true;
    PresentedResource presented(*device_, from, shown);
    surfaces_->stage(SurfacePoint(SurfaceIdentity(shown).surface(), point), SharedSurfaces::Staging::once_blt_read,
                     presented);
    return follow_queue(queue, from, shown, nullptr);
}

bool D3d12Session::on_ExecuteCommandLists12(uint32_t queue, uint32_t count, const uint32_t *lists)
{
    AV_SCOPE("D3d12Session::on_ExecuteCommandLists12");
    constexpr uint32_t most_lists = 64u;
    ID3D12CommandQueue *const on = objects_.find_as<ID3D12CommandQueue>(queue, D3dObjectKind::queue12);
    if (on == nullptr || count > most_lists) return false;
    if (removed_ != 0u) return true;
    ID3D12CommandList *executed[most_lists] = {};
    for (uint32_t index = 0u; index < count; index++) {
        ID3D12GraphicsCommandList *const recorded = list(lists[index]);
        if (recorded == nullptr) return false;
        executed[index] = recorded;
    }
    if (count == 0u) return true;
    if (!progress_.prepare(queue, on, device_->device()) || !progress_.after_siblings(queue, on)) return false;
    const uint64_t stamp = progress_.marked() + 1u;
    for (uint32_t index = 0u; index < count; index++) {
        holds_.submitted(lists[index], stamp);
        if (allocators_.submitted(lists[index], stamp) != Status::ok) return false;
    }
    on->ExecuteCommandLists(count, executed);
    if (progress_.signal(queue, on)) return true;
    LogLine(lines_).text("the progress of queue ").number(queue).text(" is not followed after its lists ran; the device is removed").write();
    if (removed_ == 0u) removed_ = static_cast<uint32_t>(DXGI_ERROR_DRIVER_INTERNAL_ERROR);
    static_cast<void>(drain());
    return false;
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
    if (queries_.remember(heap, type, count) != Status::ok) {
        made->Release();
        return false;
    }
    return kept(heap, D3dObjectKind::query_heap12, made);
}

bool D3d12Session::invariants_hold() const
{
    AV_SCOPE("D3d12Session::invariants_hold");
    return objects_.invariants_hold() && addresses_.invariants_hold() && compiles_.invariants_hold() &&
           holds_.invariants_hold() && descriptors_.invariants_hold() &&
           (device_ != nullptr ||
            objects_.count_kinds(D3dObjectKind::heap12, D3dObjectKind::command_signature12) == 0u);
}

}
