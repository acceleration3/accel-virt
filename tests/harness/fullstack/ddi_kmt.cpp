#include "harness/fullstack/ddi_kmt.hpp"

#include "core/ddi/dxgk_allocation_info_record.hpp"
#include "core/ddi/dxgk_build_paging_buffer_map_aperture_segment_record.hpp"
#include "core/ddi/dxgk_build_paging_buffer_unmap_aperture_segment_record.hpp"
#include "core/ddi/dxgk_open_allocation_info_record.hpp"
#include "core/ddi/dxgkarg_build_paging_buffer_record.hpp"
#include "core/ddi/dxgkarg_close_allocation_record.hpp"
#include "core/ddi/dxgkarg_control_interrupt2_record.hpp"
#include "core/ddi/dxgkarg_create_allocation_record.hpp"
#include "core/ddi/dxgkarg_create_context_record.hpp"
#include "core/ddi/dxgkarg_create_device_record.hpp"
#include "core/ddi/dxgkarg_create_hw_queue_record.hpp"
#include "core/ddi/dxgkarg_destroy_allocation_record.hpp"
#include "core/ddi/dxgkarg_escape_record.hpp"
#include "core/ddi/dxgkarg_open_allocation_record.hpp"
#include "core/ddi/dxgkarg_set_root_page_table_record.hpp"
#include "core/ddi/dxgkarg_submit_command_to_hw_queue_record.hpp"
#include "core/ddi/dxgkarg_submit_command_virtual_record.hpp"
#include "core/memory/aperture_segment.hpp"
#include "core/miniport/ddi_table.hpp"
#include "core/support/nt_status.hpp"
#include "guest/kmt/pci_identity.hpp"
#include "proto/resources/allocation_descriptor.hpp"
#include "proto/resources/shared_object_record.hpp"
#include "proto/resources/allocation_kind.hpp"
#include "harness/device/fake_virtio_gpu_function.hpp"
#include "harness/dxgkrnl/fake_dxgkrnl_adapter.hpp"
#include "harness/dxgkrnl/fake_dxgkrnl_fdo.hpp"
#include "harness/fullstack/interrupt_pump.hpp"
#include "harness/host/mock_host.hpp"
#include "harness/vidmm/fake_vidmm.hpp"
#include "harness/platform/mock_platform.hpp"

#include <cstdlib>
#include <cstring>
#include <sched.h>
#include <time.h>

namespace av {

namespace {
/* DXGK_CREATEALLOCATIONFLAGS::Resource, and DXGK_DESTROYALLOCATIONFLAGS::
 * DestroyResource: each is bit 0 of its own Value. */
constexpr std::uint32_t resource_flag = 1u;
constexpr std::uint32_t destroy_resource_flag = 1u;
}  // namespace

namespace {

constexpr std::int32_t  map_aperture_segment = 5;     /* DXGK_OPERATION_MAP_APERTURE_SEGMENT */
constexpr std::int32_t  unmap_aperture_segment = 6;   /* DXGK_OPERATION_UNMAP_APERTURE_SEGMENT */
constexpr std::uint32_t accessed_physically = 0x8000u; /* DXGK_ALLOCATIONINFOFLAGS */
constexpr std::uint32_t virtual_addressing = 0x4u;     /* DXGK_CREATECONTEXTFLAGS */
/* What dxgkrnl sets for a context a process may hang a hardware queue off,
 * which is every context here: the rig's processes all ask for a doorbell. */
constexpr std::uint32_t hw_queue_supported = 0x10u;
constexpr std::uint32_t first_engine = 1u;
constexpr std::uint32_t segment_aperture = 0x1u;              /* DXGK_SEGMENTFLAGS */
constexpr std::uint32_t segment_from_system_memory = 0x40u;
/* VidSch's root for a context: the implicit system memory segment, one page
 * of 512 entries (FakeVidSch::set_root_page_table). */
constexpr std::uint32_t root_segment = 2u;
constexpr std::uint64_t root_offset = 0x7000u;
constexpr std::uint32_t root_entries = 512u;
constexpr std::int32_t  crtc_vsync = 3;              /* DXGK_INTERRUPT_CRTC_VSYNC */
constexpr std::int32_t  vsync_enable = 0;            /* DXGK_CRTC_VSYNC_STATE */
constexpr std::int32_t  vsync_disable_no_phase = 2;
/* The mock clock's tick is 100 ns; a blank at 60 Hz is 166 666 of them, and a
 * step of a tenth of a millisecond measures it within a fraction of a percent. */
constexpr std::uint64_t timer_step = 1000u;
constexpr std::uint64_t blank_patience = 10000000u;
constexpr std::uint16_t subsystem_vendor_offset = 0x2Cu;
constexpr std::uint16_t subsystem_offset = 0x2Eu;

std::uint64_t now_ms()
{
    timespec time = {};
    clock_gettime(CLOCK_MONOTONIC, &time);
    return static_cast<std::uint64_t>(time.tv_sec) * 1000u + static_cast<std::uint64_t>(time.tv_nsec) / 1000000u;
}

std::uint16_t config_u16(const std::uint8_t *config, std::uint16_t offset)
{
    return static_cast<std::uint16_t>(config[offset] | config[offset + 1u] << 8u);
}

}  // namespace

DdiKmt::DdiKmt(MockPlatform &platform, const DdiTable &table, FakeDxgkrnlFdo &fdo, FakeDxgkrnlAdapter &dxgkrnl,
               FakeVirtioGpuFunction &function, InterruptPump &pump, MockHost &host)
    : platform_(platform), table_(table), fdo_(fdo), dxgkrnl_(dxgkrnl), function_(function), pump_(pump), host_(host),
      handles_(), fences_(), devices_{}, shared_{}, next_shared_handle_(first_shared_handle),
      allocations_{}, contexts_{}, queues_{}, hardware_queues_{}, doorbells_{},
      fabric_(platform), hardware_scheduling_(false), kernel_copy_{},
      next_handle_(first_handle), next_aperture_page_(0u), next_gpu_address_(first_gpu_address), paging_fence_(0u),
      next_chunk_address_(chunk_ram_address), last_code_(0u)
{
    host_.add_guest_ram(chunk_ram_address, chunk_ram_bytes);
    /* A doorbell and its queue are the adapter's, and dxgkrnl gives them back
     * before the adapter stops. */
    fdo_.on_stopping(
        [](void *context) {
            DdiKmt &kmt = *static_cast<DdiKmt *>(context);
            for (HardwareQueue &queue : kmt.hardware_queues_)
                if (queue.handle != 0u) static_cast<void>(kmt.destroy_hw_queue(queue.handle));
        },
        this);
    dxgkrnl_.on_interrupt(
        [](void *context, const DxgkargcbNotifyInterruptDataRecord &data) {
            static_cast<DdiKmt *>(context)->fences_.observe(data);
        },
        this);
}

/* Whatever the process still held after the adapter went away is only memory. */
DdiKmt::~DdiKmt()
{
    dxgkrnl_.on_interrupt(nullptr, nullptr);
    for (Allocation &slot : allocations_)
        if (slot.handle != 0u) release(slot);
}

template <typename Routine>
Routine DdiKmt::routine(DdiSlot slot) const
{
    platform_.set_irql(Irql::passive);
    return reinterpret_cast<Routine>(table_.routine(slot));
}

Status DdiKmt::answer(std::int32_t code)
{
    last_code_ = static_cast<std::uint32_t>(code);
    return NtStatus(code).status();
}

Status DdiKmt::refuse(Status status)
{
    last_code_ = static_cast<std::uint32_t>(NtStatus(status).value());
    return status;
}

DdiKmt::Device *DdiKmt::find_device(std::uint32_t handle)
{
    for (Device &device : devices_)
        if (handle != 0u && device.handle == handle) return &device;
    return nullptr;
}

DdiKmt::Allocation *DdiKmt::find_allocation(std::uint32_t handle)
{
    for (Allocation &allocation : allocations_)
        if (handle != 0u && allocation.handle == handle) return &allocation;
    return nullptr;
}

DdiKmt::Context *DdiKmt::find_context(std::uint32_t handle)
{
    for (Context &context : contexts_)
        if (handle != 0u && context.handle == handle) return &context;
    return nullptr;
}

DdiKmt::Queue *DdiKmt::find_queue(std::uint32_t handle)
{
    for (Queue &queue : queues_)
        if (handle != 0u && queue.handle == handle) return &queue;
    return nullptr;
}

DdiKmt::HardwareQueue *DdiKmt::find_hardware_queue(std::uint32_t handle)
{
    for (HardwareQueue &queue : hardware_queues_)
        if (handle != 0u && queue.handle == handle) return &queue;
    return nullptr;
}

DdiKmt::Doorbell *DdiKmt::find_doorbell(std::uint32_t handle)
{
    for (Doorbell &doorbell : doorbells_)
        if (handle != 0u && doorbell.handle == handle) return &doorbell;
    return nullptr;
}

std::uint32_t DdiKmt::slot_of(const Context &context) const
{
    return static_cast<std::uint32_t>(&context - contexts_);
}

Status DdiKmt::enumerate_adapters(std::uint32_t *adapters, std::uint32_t capacity, std::uint32_t &count)
{
    count = fdo_.started() ? 1u : 0u;
    if (count != 0u && capacity != 0u && adapters != nullptr) adapters[0] = adapter_handle;
    return refuse(Status::ok);
}

/* KMTQAITYPE_PHYSICALADAPTERDEVICEIDS: dxgkrnl reads the PCI identity from the
 * function's configuration space. */
Status DdiKmt::query_identity(std::uint32_t adapter, PciIdentity &identity)
{
    if (adapter != adapter_handle) return refuse(Status::invalid_parameter);
    const std::uint8_t *const config = function_.config_space();
    identity = PciIdentity(config_u16(config, 0u), config_u16(config, 2u), config_u16(config, subsystem_vendor_offset),
                           config_u16(config, subsystem_offset));
    return refuse(Status::ok);
}

/* KMT_DRIVERVERSION from DRIVERCAPS.WDDMVersion: 0x3200 is 3200. */
Status DdiKmt::query_driver_version(std::uint32_t adapter, std::uint32_t &version)
{
    version = 0u;
    if (adapter != adapter_handle) return refuse(Status::invalid_parameter);
    const auto wddm = static_cast<std::uint32_t>(fdo_.dxgadapter().driver_caps().WDDMVersion);
    version = (wddm >> 12u & 0xFu) * 1000u + (wddm >> 8u & 0xFu) * 100u;
    return refuse(Status::ok);
}

Status DdiKmt::close_adapter(std::uint32_t adapter)
{
    return refuse(adapter == adapter_handle ? Status::ok : Status::invalid_parameter);
}

Status DdiKmt::adapter_luid(std::uint32_t adapter, std::uint64_t &luid)
{
    luid = adapter == adapter_handle ? rig_luid : 0u;
    return refuse(adapter == adapter_handle ? Status::ok : Status::invalid_parameter);
}

Status DdiKmt::query_video_memory(std::uint32_t adapter, MemorySegmentGroup group, std::uint64_t &budget,
                                  std::uint64_t &usage)
{
    budget = 0u;
    usage = 0u;
    const bool local = group == MemorySegmentGroup::local;
    if (adapter != adapter_handle || (!local && group != MemorySegmentGroup::non_local))
        return refuse(Status::invalid_parameter);
    FakeVidMm          segments(fdo_.dxgadapter());
    const std::int32_t queried = segments.create_segments(fdo_.context());
    if (queried < 0) return answer(queried);
    for (std::uint32_t index = 0u; index < segments.answer().NbSegment; index++) {
        const DxgkSegmentDescriptor4Record descriptor = segments.segment(index);
        const bool in_system_memory = (descriptor.Flags & (segment_aperture | segment_from_system_memory)) != 0u;
        if (in_system_memory != local) budget += descriptor.Size;
    }
    if (!local)
        for (const Allocation &slot : allocations_) usage += slot.handle != 0u ? slot.bytes : 0u;
    return refuse(Status::ok);
}

Status DdiKmt::create_device(std::uint32_t adapter, std::uint32_t &device)
{
    device = 0u;
    if (adapter != adapter_handle) return refuse(Status::invalid_parameter);
    for (Device &free : devices_) {
        if (free.handle != 0u) continue;
        const std::uint32_t handle = next_handle_++;
        DxgkargCreateDeviceRecord request = {};
        request.hDevice = reinterpret_cast<void *>(static_cast<std::uintptr_t>(handle));
        const Status created = answer(routine<std::int32_t (*)(void *, DxgkargCreateDeviceRecord *)>(
            DdiSlot::DxgkDdiCreateDevice)(fdo_.context(), &request));
        if (created != Status::ok) return created;
        free = Device{handle, request.hDevice};
        device = handle;
        return created;
    }
    return refuse(Status::no_space);
}

Status DdiKmt::destroy_device(std::uint32_t device)
{
    Device *const found = find_device(device);
    if (found == nullptr) return refuse(Status::invalid_parameter);
    const Status destroyed =
        answer(routine<std::int32_t (*)(void *)>(DdiSlot::DxgkDdiDestroyDevice)(found->driver));
    *found = Device{};
    return destroyed;
}

Status DdiKmt::create_allocation(std::uint32_t device, const std::uint8_t *descriptor, std::uint32_t size,
                                 std::uint32_t &allocation)
{
    return create_with(device, descriptor, size, 0u, allocation);
}

Status DdiKmt::create_with(std::uint32_t device, const std::uint8_t *descriptor, std::uint32_t size,
                           std::uint32_t flags, std::uint32_t &allocation)
{
    allocation = 0u;
    Device *const owner = find_device(device);
    if (owner == nullptr || descriptor == nullptr || size > max_private_data) return refuse(Status::invalid_parameter);
    for (Allocation &slot : allocations_) {
        if (slot.handle != 0u || slot.block != nullptr) continue;
        std::memcpy(slot.private_data, descriptor, size);
        slot.private_size = size;
        DxgkAllocationInfoRecord info = {};
        info.pPrivateDriverData = slot.private_data;
        info.PrivateDriverDataSize = size;
        DxgkargCreateAllocationRecord request = {};
        request.NumAllocations = 1u;
        request.pAllocationInfo = &info;
        request.Flags = flags;
        const Status created = answer(routine<std::int32_t (*)(void *, DxgkargCreateAllocationRecord *)>(
            DdiSlot::DxgkDdiCreateAllocation)(fdo_.context(), &request));
        if (created != Status::ok) return created;
        slot.resource = request.hResource;
        /* What dxgkrnl does before the open it runs inside every create: the
         * caller's own bytes, back over whatever the driver wrote. A rig that
         * skips this passes a driver that writes the record at create time,
         * which no caller would ever read (research.md §15). */
        std::memcpy(slot.private_data, descriptor, size);
        slot.driver = info.hAllocation;
        slot.bytes = info.Size;
        slot.aperture = info.PreferredSegment == ApertureSegment::segment_id && (info.Flags & accessed_physically) != 0u;
        const Status admitted = admit(slot, device, owner->driver);
        if (admitted == Status::ok) allocation = slot.handle;
        return admitted;
    }
    return refuse(Status::no_space);
}

/* VidMm backs the allocation and dxgkrnl opens it on the device that made it;
 * a failure destroys what the driver made, as dxgkrnl unwinds a create. */
Status DdiKmt::admit(Allocation &slot, std::uint32_t device, void *driver_device)
{
    const std::uint64_t pages = slot.bytes / page_bytes;
    Status status = slot.bytes == 0u || slot.bytes % page_bytes != 0u || pages > max_pages ? Status::no_memory
                                                                                           : Status::ok;
    if (status == Status::ok) status = back(slot);
    /* The frame list outlives the paging call that attaches it: the MDL the
     * driver keeps names it until the allocation is freed. */
    if (status == Status::ok) {
        slot.frames = static_cast<std::uint64_t *>(std::malloc(pages * sizeof *slot.frames));
        status = slot.frames != nullptr ? Status::ok : Status::no_memory;
    }
    if (status == Status::ok) {
        if (slot.guest_ram == 0u) platform_.zero(slot.block, slot.bytes);
        const std::uint64_t physical = slot.guest_ram != 0u ? slot.guest_ram : platform_.physical_address(slot.block);
        for (std::uint64_t index = 0u; index < pages; index++) slot.frames[index] = physical + index * page_bytes;
        slot.handle = handles_.insert(slot.driver);
        status = slot.handle != 0u ? Status::ok : Status::no_space;
    }
    slot.device = device;
    slot.offset_in_pages = next_aperture_page_;
    next_aperture_page_ += pages;
    if (status == Status::ok) status = open_on(slot, driver_device);
    if (status == Status::ok) return refuse(Status::ok);
    release(slot);
    return refuse(status);
}

/* The host GPU imports a host-visible chunk through a second mapping of its
 * pages, which only file-backed guest RAM allows - what the runner's shared
 * memory backend gives QEMU. Every other allocation stays in the platform's
 * memory, where the other tests read it. Chunk RAM is handed out once and never
 * reused, so it is still the zero pages the file began as. */
Status DdiKmt::back(Allocation &slot)
{
    AllocationDescriptor descriptor;
    const bool chunk = descriptor.decode(slot.private_data, AllocationDescriptor::encoded_size) == Status::ok &&
                       descriptor.kind() == AllocationKind::host_visible_memory;
    if (!chunk) {
        slot.block = static_cast<std::uint8_t *>(platform_.allocate_contiguous(slot.bytes));
        return slot.block != nullptr ? Status::ok : Status::no_memory;
    }
    if (slot.bytes > chunk_ram_address + chunk_ram_bytes - next_chunk_address_) return Status::no_memory;
    slot.guest_ram = next_chunk_address_;
    slot.block = host_.guest_ram(slot.guest_ram);
    next_chunk_address_ += slot.bytes;
    return Status::ok;
}

Status DdiKmt::open_on(Allocation &slot, void *driver_device)
{
    DxgkOpenAllocationInfoRecord info = {};
    info.hAllocation = slot.handle;
    info.pPrivateDriverData = slot.private_data;
    /* The length the create carried, not the buffer's: the miniport takes 48
     * bytes or 88 and refuses every other, so a shared allocation could not be
     * opened at all if this named the whole buffer (feature 053). */
    info.PrivateDriverDataSize = slot.private_size;
    DxgkargOpenAllocationRecord request = {};
    request.NumAllocations = 1u;
    request.pOpenAllocation = &info;
    const Status opened = answer(routine<std::int32_t (*)(void *, DxgkargOpenAllocationRecord *)>(
        DdiSlot::DxgkDdiOpenAllocation)(driver_device, &request));
    slot.opened = opened == Status::ok ? info.hDeviceSpecificAllocation : nullptr;
    return opened;
}

/* Everything the harness holds for the slot, and the driver's allocation if
 * it still has one; what the driver refuses to destroy stays its own. */
void DdiKmt::release(Allocation &slot)
{
    if (slot.driver != nullptr && slot.opened == nullptr && fdo_.started()) {
        void *const                    list[1] = {slot.driver};
        DxgkargDestroyAllocationRecord request = {};
        request.NumAllocations = 1u;
        request.pAllocationList = list;
        static_cast<void>(routine<std::int32_t (*)(void *, const DxgkargDestroyAllocationRecord *)>(
            DdiSlot::DxgkDdiDestroyAllocation)(fdo_.context(), &request));
    }
    if (slot.handle != 0u) handles_.remove(slot.handle);
    if (slot.block != nullptr && slot.guest_ram == 0u) platform_.release_contiguous(slot.block, slot.bytes);
    std::free(slot.frames);
    std::memset(static_cast<void *>(&slot), 0, sizeof slot);
}

Status DdiKmt::page(Allocation &slot, std::int32_t operation)
{
    std::uint8_t                   dma[64] = {};
    DxgkargBuildPagingBufferRecord request = {};
    request.pDmaBuffer = dma;
    request.DmaSize = sizeof dma;
    request.Operation = operation;
    const std::uint64_t pages = slot.bytes / page_bytes;
    if (operation == map_aperture_segment) {
        DxgkBuildPagingBufferMapApertureSegmentRecord map = {};
        map.hAllocation = slot.driver;
        map.SegmentId = ApertureSegment::segment_id;
        map.OffsetInPages = slot.offset_in_pages;
        map.NumberOfPages = pages;
        map.pMdl = platform_.attach_page_frames(slot.frames, static_cast<std::uint32_t>(pages));
        std::memcpy(request.Reserved, &map, sizeof map);
    } else {
        DxgkBuildPagingBufferUnmapApertureSegmentRecord unmap = {};
        unmap.hAllocation = slot.driver;
        unmap.SegmentId = ApertureSegment::segment_id;
        unmap.OffsetInPages = slot.offset_in_pages;
        unmap.NumberOfPages = pages;
        std::memcpy(request.Reserved, &unmap, sizeof unmap);
    }
    return answer(routine<std::int32_t (*)(void *, DxgkargBuildPagingBufferRecord *)>(
        DdiSlot::DxgkDdiBuildPagingBuffer)(fdo_.context(), &request));
}

Status DdiKmt::destroy_allocation(std::uint32_t device, std::uint32_t allocation)
{
    const Allocation *const named = find_allocation(allocation);
    /* The list D3DKMTDestroyAllocation2 walks holds allocation handles: a
     * resource named in it is a handle type mismatch, and the caller wanted
     * destroy_shared_allocation (research.md §17). */
    if (named != nullptr && named->resource != nullptr) return refuse(Status::invalid_parameter);
    return destroy_slot(device, allocation, nullptr);
}

Status DdiKmt::destroy_slot(std::uint32_t device, std::uint32_t allocation, void *resource)
{
    Allocation *const found = find_allocation(allocation);
    Device *const     owner = find_device(device);
    if (found == nullptr || owner == nullptr || found->device != device) return refuse(Status::invalid_parameter);
    if (found->resident) {
        const Status unmapped = page(*found, unmap_aperture_segment);
        if (unmapped != Status::ok) return unmapped;
        found->resident = false;
    }
    void *const                  opened[1] = {found->opened};
    DxgkargCloseAllocationRecord close = {};
    close.NumAllocations = 1u;
    close.pOpenHandleList = opened;
    const Status closed = answer(routine<std::int32_t (*)(void *, const DxgkargCloseAllocationRecord *)>(
        DdiSlot::DxgkDdiCloseAllocation)(owner->driver, &close));
    if (closed != Status::ok) return closed;
    found->opened = nullptr;
    if (found->opened_only) {
        /* A second device's open of somebody else's allocation: closing it is
         * all this slot ever owned. */
        found->driver = nullptr;
        release(*found);
        return refuse(Status::ok);
    }
    void *const                    list[1] = {found->driver};
    DxgkargDestroyAllocationRecord request = {};
    request.NumAllocations = 1u;
    request.pAllocationList = list;
    /* A resource is taken by naming it, with every allocation in it listed:
     * the driver refuses one that would leave the resource behind. */
    request.hResource = resource;
    request.Flags = resource != nullptr ? destroy_resource_flag : 0u;
    const Status destroyed = answer(routine<std::int32_t (*)(void *, const DxgkargDestroyAllocationRecord *)>(
        DdiSlot::DxgkDdiDestroyAllocation)(fdo_.context(), &request));
    if (destroyed != Status::ok) return destroyed;
    found->driver = nullptr;
    release(*found);
    return refuse(Status::ok);
}

Status DdiKmt::lock(std::uint32_t device, std::uint32_t allocation, void *&view)
{
    view = nullptr;
    Allocation *const found = find_allocation(allocation);
    if (found == nullptr || found->device != device) return refuse(Status::invalid_parameter);
    view = found->block;
    return refuse(Status::ok);
}

Status DdiKmt::unlock(std::uint32_t device, std::uint32_t allocation)
{
    const Allocation *const found = find_allocation(allocation);
    return refuse(found != nullptr && found->device == device ? Status::ok : Status::invalid_parameter);
}

Status DdiKmt::create_paging_queue(std::uint32_t device, std::uint32_t &queue, std::uint32_t &fence_object)
{
    queue = 0u;
    fence_object = 0u;
    if (find_device(device) == nullptr) return refuse(Status::invalid_parameter);
    for (Queue &slot : queues_) {
        if (slot.handle != 0u) continue;
        slot = Queue{next_handle_, device, next_handle_ + 1u};
        next_handle_ += 2u;
        queue = slot.handle;
        fence_object = slot.fence_object;
        return refuse(Status::ok);
    }
    return refuse(Status::no_space);
}

Status DdiKmt::destroy_paging_queue(std::uint32_t queue)
{
    Queue *const found = find_queue(queue);
    if (found == nullptr) return refuse(Status::invalid_parameter);
    *found = Queue{};
    return refuse(Status::ok);
}

/* Only the aperture segment is the driver's to map: VidMm keeps an allocation
 * in its own system memory segment without asking. */
Status DdiKmt::make_resident(std::uint32_t queue, std::uint32_t allocation, std::uint64_t &paging_fence)
{
    paging_fence = 0u;
    const Queue *const found_queue = find_queue(queue);
    Allocation *const  found = find_allocation(allocation);
    if (found_queue == nullptr || found == nullptr || found->device != found_queue->device)
        return refuse(Status::invalid_parameter);
    if (found->aperture && !found->resident) {
        const Status mapped = page(*found, map_aperture_segment);
        if (mapped != Status::ok) return mapped;
        found->resident = true;
    }
    paging_fence = ++paging_fence_;
    return refuse(Status::ok);
}

Status DdiKmt::map_gpu_virtual_address(std::uint32_t queue, std::uint32_t allocation, std::uint64_t pages,
                                       std::uint64_t &address, std::uint64_t &paging_fence)
{
    address = 0u;
    paging_fence = 0u;
    const Queue *const found_queue = find_queue(queue);
    Allocation *const  found = find_allocation(allocation);
    if (found_queue == nullptr || found == nullptr || found->device != found_queue->device || pages == 0u ||
        pages > found->bytes / page_bytes || found->gpu_address != 0u)
        return refuse(Status::invalid_parameter);
    found->gpu_address = next_gpu_address_;
    /* A guard page between ranges, so an address past one is in none. */
    next_gpu_address_ += (pages + 1u) * page_bytes;
    address = found->gpu_address;
    paging_fence = ++paging_fence_;
    return refuse(Status::ok);
}

Status DdiKmt::free_gpu_virtual_address(std::uint32_t adapter, std::uint64_t address, std::uint64_t)
{
    if (adapter != adapter_handle || address == 0u) return refuse(Status::invalid_parameter);
    for (Allocation &slot : allocations_) {
        if (slot.handle == 0u || slot.gpu_address != address) continue;
        slot.gpu_address = 0u;
        return refuse(Status::ok);
    }
    return refuse(Status::invalid_parameter);
}

Status DdiKmt::wait_for_fence(std::uint32_t device, std::uint32_t fence_object, std::uint64_t fence)
{
    bool known = false;
    for (const Queue &queue : queues_) known = known || (fence_object != 0u && queue.fence_object == fence_object &&
                                                        queue.device == device);
    return refuse(known && fence <= paging_fence_ ? Status::ok : Status::invalid_parameter);
}

Status DdiKmt::create_context(std::uint32_t device, std::uint32_t &context)
{
    return create_context_on(device, nullptr, 0u, false, context);
}

Status DdiKmt::create_context_virtual(std::uint32_t device, const std::uint8_t *private_data, std::uint32_t size,
                                      std::uint32_t &context)
{
    if (private_data == nullptr && size != 0u) return refuse(Status::invalid_parameter);
    return create_context_on(device, private_data, size, false, context);
}

/* Hardware scheduling is the OS's decision, and dxgkrnl gives a process a
 * context a queue may run on only where it is on (feature 049). */
Status DdiKmt::create_hw_queue_context(std::uint32_t device, const std::uint8_t *private_data, std::uint32_t size,
                                       std::uint32_t &context)
{
    context = 0u;
    if (private_data == nullptr && size != 0u) return refuse(Status::invalid_parameter);
    if (!hardware_scheduling_) return refuse(Status::not_supported);
    return create_context_on(device, private_data, size, true, context);
}

/* dxgkrnl copies the private data into its own memory before the driver sees
 * it, and VidSch gives the context its root before anything is submitted. */
Status DdiKmt::create_context_on(std::uint32_t device, const std::uint8_t *private_data, std::uint32_t size,
                                 bool hardware_queue, std::uint32_t &context)
{
    context = 0u;
    const Device *const owner = find_device(device);
    if (owner == nullptr || size > max_private_data) return refuse(Status::invalid_parameter);
    for (Context &slot : contexts_) {
        if (slot.handle != 0u) continue;
        if (size != 0u) std::memcpy(kernel_copy_, private_data, size);
        DxgkargCreateContextRecord request = {};
        request.EngineAffinity = first_engine;
        request.Flags = hardware_queue ? (virtual_addressing | hw_queue_supported) : virtual_addressing;
        request.pPrivateDriverData = size != 0u ? kernel_copy_ : nullptr;
        request.PrivateDriverDataSize = size;
        const Status created = answer(routine<std::int32_t (*)(void *, DxgkargCreateContextRecord *)>(
            DdiSlot::DxgkDdiCreateContext)(owner->driver, &request));
        if (created != Status::ok) return created;
        const DxgkargSetRootPageTableRecord root = {request.hContext, root_segment, 0u, root_offset, root_entries, 0u};
        routine<void (*)(void *, const DxgkargSetRootPageTableRecord *)>(DdiSlot::DxgkDdiSetRootPageTable)(
            fdo_.context(), &root);
        slot = Context{next_handle_++, device, request.hContext, request.ContextInfoDmaBufferPrivateDataSize};
        fences_.forget(slot_of(slot));
        context = slot.handle;
        return created;
    }
    return refuse(Status::no_space);
}

Status DdiKmt::destroy_context(std::uint32_t context)
{
    Context *const found = find_context(context);
    if (found == nullptr) return refuse(Status::invalid_parameter);
    const Status destroyed =
        answer(routine<std::int32_t (*)(void *)>(DdiSlot::DxgkDdiDestroyContext)(found->driver));
    fences_.forget(slot_of(*found));
    *found = Context{};
    pump_.deliver();
    return destroyed;
}

/* DXGCONTEXT::SubmitCommand refuses private data larger than the context asked
 * for before anything reaches the driver; whether a context can still take a
 * submission is the driver's answer. */
Status DdiKmt::submit(std::uint32_t context, std::uint64_t commands, std::uint64_t length,
                      const std::uint8_t *private_data, std::uint32_t private_size)
{
    Context *const found = find_context(context);
    if (found == nullptr || private_size > found->private_capacity || private_size > max_private_data ||
        (private_data == nullptr && private_size != 0u))
        return refuse(Status::invalid_parameter);
    if (private_size != 0u) std::memcpy(kernel_copy_, private_data, private_size);
    DxgkargSubmitCommandVirtualRecord request = {};
    request.hContext = found->driver;
    request.DmaBufferVirtualAddress = commands;
    request.DmaBufferSize = static_cast<std::uint32_t>(length);
    request.pDmaBufferPrivateData = private_size != 0u ? kernel_copy_ : nullptr;
    request.DmaBufferPrivateDataSize = private_size;
    request.SubmissionFenceId = fences_.next();
    const Status submitted = answer(routine<std::int32_t (*)(void *, const DxgkargSubmitCommandVirtualRecord *)>(
        DdiSlot::DxgkDdiSubmitCommandVirtual)(fdo_.context(), &request));
    if (submitted == Status::ok) fences_.commit(slot_of(*found));
    pump_.deliver();
    return submitted;
}

Status DdiKmt::wait_for_context(std::uint32_t device, std::uint32_t context)
{
    const Context *const found = find_context(context);
    if (found == nullptr || found->device != device) return refuse(Status::invalid_parameter);
    const std::uint64_t deadline = now_ms() + patience_ms;
    for (;;) {
        /* A store the process made reaches the device here, as it reaches it
         * on the bus while the process waits. */
        static_cast<void>(fabric_.deliver());
        pump_.deliver();
        /* Everything this context has outstanding, on both paths: what it
         * queued through a doorbell is answered by the queue's progress, what
         * it submitted through the driver by the node's fences. */
        const Status settled = fences_.settled(slot_of(*found));
        if (hardware_queue_idle(context) == Status::ok && settled != Status::not_found) return refuse(settled);
        if (now_ms() >= deadline) break;
        sched_yield();
    }
    last_code_ = timed_out;
    return Status::not_found;
}

/* What this context queued through a doorbell that is still connected is not
 * in the driver's fences, so what dxgkrnl waits on for it is the queue's
 * progress against the value user mode queued. A doorbell that was
 * disconnected is not waited on: what it queued is behind whatever the process
 * submitted through the kernel instead, and the device finishes a ring in
 * order. */
Status DdiKmt::hardware_queue_idle(std::uint32_t context)
{
    for (const Doorbell &doorbell : doorbells_) {
        if (!waiting_on(doorbell, context)) continue;
        const HardwareQueue *const on = find_hardware_queue(doorbell.queue);
        if (on != nullptr && on->progress < doorbell.queued) return Status::not_found;
    }
    return Status::ok;
}

bool DdiKmt::waiting_on(const Doorbell &doorbell, std::uint32_t context)
{
    if (doorbell.handle == 0u || doorbell.queued == 0u ||
        doorbell.status != static_cast<std::uint32_t>(DoorbellStatus::connected))
        return false;
    const HardwareQueue *const on = find_hardware_queue(doorbell.queue);
    return on != nullptr && on->context == context;
}

/* dxgkrnl turns the blank on for a waiter and off once it is woken; the
 * driver's timer keeps its phase between, and the mock clock only moves here. */
Status DdiKmt::wait_for_vertical_blank(std::uint32_t adapter, std::uint32_t device, std::uint32_t source)
{
    if (adapter != adapter_handle || find_device(device) == nullptr || source != 0u)
        return refuse(Status::invalid_parameter);
    const auto control = routine<std::int32_t (*)(void *, DxgkargControlInterrupt2Record)>(
        DdiSlot::DxgkDdiControlInterrupt2);
    const Status enabled = answer(control(fdo_.context(), DxgkargControlInterrupt2Record{crtc_vsync, vsync_enable}));
    if (enabled != Status::ok) return enabled;
    const std::uint32_t before = fences_.vertical_blanks();
    for (std::uint64_t waited = 0u; fences_.vertical_blanks() == before && waited < blank_patience;
         waited += timer_step)
        platform_.run_timers(timer_step);
    platform_.set_irql(Irql::passive);
    const Status disabled =
        answer(control(fdo_.context(), DxgkargControlInterrupt2Record{crtc_vsync, vsync_disable_no_phase}));
    if (fences_.vertical_blanks() == before) {
        last_code_ = timed_out;
        return Status::not_found;
    }
    return disabled;
}

/* D3DKMTEscape as dxgkrnl makes it: the handles translated to the driver's, the
 * data copied into the kernel and the copy copied back out whatever the driver
 * answered, as DxgkEscape does. */
Status DdiKmt::escape(std::uint32_t adapter, std::uint32_t device, std::uint32_t context, std::uint8_t *data,
                      std::uint32_t size)
{
    const Device *const  owner = find_device(device);
    const Context *const found = find_context(context);
    if (adapter != adapter_handle || owner == nullptr || found == nullptr || found->device != device ||
        data == nullptr || size == 0u || size > max_private_data)
        return refuse(Status::invalid_parameter);
    std::memcpy(kernel_copy_, data, size);
    DxgkargEscapeRecord request = {};
    request.hDevice = owner->driver;
    request.pPrivateDriverData = kernel_copy_;
    request.PrivateDriverDataSize = size;
    request.hContext = found->driver;
    const Status escaped = answer(routine<std::int32_t (*)(void *, const DxgkargEscapeRecord *)>(
        DdiSlot::DxgkDdiEscape)(fdo_.context(), &request));
    std::memcpy(data, kernel_copy_, size);
    pump_.deliver();
    return escaped;
}

std::uint64_t DdiKmt::performance_counter(std::uint64_t &frequency) const
{
    return platform_.performance_counter(frequency);
}

std::uint32_t DdiKmt::last_code() const { return last_code_; }

void DdiKmt::end_process()
{
    /* A process's hardware queues go before its contexts, as dxgkrnl's do: a
     * queue names the context it runs on. */
    for (HardwareQueue &queue : hardware_queues_)
        if (queue.handle != 0u) static_cast<void>(destroy_hw_queue(queue.handle));
    for (Context &context : contexts_)
        if (context.handle != 0u) static_cast<void>(destroy_context(context.handle));
    for (Allocation &allocation : allocations_)
        if (allocation.handle != 0u) static_cast<void>(destroy_allocation(allocation.device, allocation.handle));
    for (Queue &queue : queues_) queue = Queue{};
    for (Device &device : devices_)
        if (device.handle != 0u) static_cast<void>(destroy_device(device.handle));
}

const NodeFences &DdiKmt::fences() const { return fences_; }
FakeHandleTable  &DdiKmt::handles() { return handles_; }

std::uint32_t DdiKmt::open_objects() const
{
    std::uint32_t open = 0u;
    for (const Device &device : devices_) open += device.handle != 0u ? 1u : 0u;
    for (const Allocation &allocation : allocations_) open += allocation.handle != 0u ? 1u : 0u;
    for (const Context &context : contexts_) open += context.handle != 0u ? 1u : 0u;
    return open;
}


Status DdiKmt::create_hw_queue(std::uint32_t device, std::uint32_t context, GuestHwQueue &made)
{
    made = GuestHwQueue{};
    Context *const on = find_context(context);
    if (on == nullptr || on->device != device) return refuse(Status::invalid_parameter);
    if (!hardware_scheduling_) return refuse(Status::not_supported);
    for (HardwareQueue &slot : hardware_queues_) {
        if (slot.handle != 0u) continue;
        slot.progress = 0u;
        DxgkargCreateHwQueueRecord request = {};
        request.HwQueueProgressFenceCPUVirtualAddress = &slot.progress;
        const Status created = answer(routine<std::int32_t (*)(void *, DxgkargCreateHwQueueRecord *)>(
            DdiSlot::DxgkDdiCreateHwQueue)(on->driver, &request));
        if (created != Status::ok) return created;
        slot.handle = next_handle_++;
        slot.context = context;
        slot.driver = request.hHwQueue;
        /* Read-only to the process, as dxgkrnl maps it; the monitored fence it
         * is reported on is named by the queue itself here. */
        made = GuestHwQueue(slot.handle, slot.handle, &slot.progress);
        return Status::ok;
    }
    return refuse(Status::no_space);
}

/* What a hardware-scheduled context submits through, as dxgkrnl carries it:
 * DxgkDdiSubmitCommandToHwQueue with the descriptor as private data and the
 * progress fence value the queue reaches when the device has run it. */
Status DdiKmt::submit_to_hw_queue(std::uint32_t queue, std::uint64_t commands, std::uint64_t length,
                                  const std::uint8_t *private_data, std::uint32_t private_size,
                                  std::uint64_t progress_fence_id)
{
    HardwareQueue *const on = find_hardware_queue(queue);
    if (on == nullptr || private_size > max_private_data || (private_data == nullptr) != (private_size == 0u))
        return refuse(Status::invalid_parameter);
    if (private_size != 0u) std::memcpy(kernel_copy_, private_data, private_size);
    DxgkargSubmitCommandToHwQueueRecord request = {};
    request.hHwQueue = on->driver;
    request.HwQueueProgressFenceId = progress_fence_id;
    request.DmaBufferVirtualAddress = commands;
    request.DmaBufferSize = static_cast<std::uint32_t>(length);
    request.pDmaBufferPrivateData = private_size != 0u ? kernel_copy_ : nullptr;
    request.DmaBufferPrivateDataSize = private_size;
    request.HwQueueProgressFenceCpuVa = &on->progress;
    const Status submitted = answer(routine<std::int32_t (*)(void *, const DxgkargSubmitCommandToHwQueueRecord *)>(
        DdiSlot::DxgkDdiSubmitCommandToHwQueue)(fdo_.context(), &request));
    pump_.deliver();
    return submitted;
}

/* Until the queue's progress fence reaches `value`, which the driver writes
 * when the device has finished what the queue carried. */
Status DdiKmt::wait_for_hw_queue(std::uint32_t device, std::uint32_t progress_fence, std::uint64_t value)
{
    const HardwareQueue *const on = find_hardware_queue(progress_fence);
    const Context *const       context = on != nullptr ? find_context(on->context) : nullptr;
    if (context == nullptr || context->device != device) return refuse(Status::invalid_parameter);
    const std::uint64_t deadline = now_ms() + patience_ms;
    for (;;) {
        static_cast<void>(fabric_.deliver());
        pump_.deliver();
        if (on->progress >= value) return refuse(Status::ok);
        if (now_ms() >= deadline) break;
        sched_yield();
    }
    last_code_ = timed_out;
    return Status::not_found;
}

Status DdiKmt::destroy_hw_queue(std::uint32_t queue)
{
    HardwareQueue *const found = find_hardware_queue(queue);
    if (found == nullptr) return refuse(Status::invalid_parameter);
    /* A queue destroyed takes its doorbells, as dxgkrnl's does: the driver is
     * told about the doorbell first, because it is the thing that names the
     * queue. */
    for (Doorbell &doorbell : doorbells_)
        if (doorbell.handle != 0u && doorbell.queue == queue) static_cast<void>(destroy_doorbell(doorbell.handle));
    const Status destroyed =
        answer(routine<std::int32_t (*)(void *)>(DdiSlot::DxgkDdiDestroyHwQueue)(found->driver));
    *found = HardwareQueue{};
    return destroyed;
}

Status DdiKmt::create_doorbell(std::uint32_t queue, std::uint32_t ring, std::uint32_t &doorbell, void *&page,
                              const std::uint32_t *&status, std::uint64_t *&queued)
{
    doorbell = 0u;
    page = nullptr;
    status = nullptr;
    queued = nullptr;
    HardwareQueue *const on = find_hardware_queue(queue);
    const Allocation *const buffer = find_allocation(ring);
    if (on == nullptr || buffer == nullptr) return refuse(Status::invalid_parameter);
    for (Doorbell &slot : doorbells_) {
        if (slot.handle != 0u) continue;
        DxgkargCreateDoorbellRecord request = {};
        request.hHwQueue = on->driver;
        request.hRingBuffer = buffer->opened;
        const Status created = answer(routine<std::int32_t (*)(DxgkargCreateDoorbellRecord *)>(
            DdiSlot::DxgkDdiCreateDoorbell)(&request));
        if (created != Status::ok) return created;
        slot.handle = next_handle_++;
        slot.queue = queue;
        slot.driver = request.hDoorbell;
        slot.page = fabric_.reserve();
        if (slot.page == nullptr) return refuse(Status::no_space);
        /* Created is not connected: what the process reads says to submit the
         * other way until it asks for a connection. */
        slot.status = static_cast<std::uint32_t>(DoorbellStatus::disconnected_retry);
        slot.queued = 0u;
        doorbell = slot.handle;
        page = slot.page;
        status = &slot.status;
        queued = &slot.queued;
        return Status::ok;
    }
    return refuse(Status::no_space);
}

Status DdiKmt::connect_doorbell(std::uint32_t doorbell)
{
    Doorbell *const found = find_doorbell(doorbell);
    if (found == nullptr) return refuse(Status::invalid_parameter);
    DxgkargConnectDoorbellRecord request = {};
    request.hDoorbell = found->driver;
    const Status connected = answer(routine<std::int32_t (*)(DxgkargConnectDoorbellRecord *)>(
        DdiSlot::DxgkDdiConnectDoorbell)(&request));
    if (connected != Status::ok) return connected;
    /* Connected, the page the process holds carries to the device page the
     * driver just named. */
    fabric_.aim(found->page, request.KernelCpuVirtualAddress);
    found->status = request.Status;
    return Status::ok;
}

Status DdiKmt::disconnect_doorbell(std::uint32_t doorbell, DoorbellStatus says)
{
    Doorbell *const found = find_doorbell(doorbell);
    if (found == nullptr || says == DoorbellStatus::connected || says == DoorbellStatus::connected_notify_kmd)
        return refuse(Status::invalid_parameter);
    const DxgkargDisconnectDoorbellRecord request = {found->driver, 0u, 0u};
    const Status                          gone = answer(routine<std::int32_t (*)(const DxgkargDisconnectDoorbellRecord *)>(
        DdiSlot::DxgkDdiDisconnectDoorbell)(&request));
    if (gone != Status::ok) return gone;
    /* The page goes with the connection: a store the process makes after this
     * reaches nothing, which is what dxgkrnl's dummy page is for on a machine
     * whose device needs one (research.md §10). */
    fabric_.blind(found->page);
    found->status = static_cast<std::uint32_t>(says);
    return Status::ok;
}

Status DdiKmt::destroy_doorbell(std::uint32_t doorbell)
{
    Doorbell *const found = find_doorbell(doorbell);
    if (found == nullptr) return refuse(Status::invalid_parameter);
    const DxgkargDestroyDoorbellRecord request = {found->driver};
    const Status                       destroyed = answer(routine<std::int32_t (*)(const DxgkargDestroyDoorbellRecord *)>(
        DdiSlot::DxgkDdiDestroyDoorbell)(&request));
    fabric_.release(found->page);
    *found = Doorbell{};
    return destroyed;
}

Status DdiKmt::notify_work_submission(std::uint32_t doorbell)
{
    const Doorbell *const found = find_doorbell(doorbell);
    if (found == nullptr) return refuse(Status::invalid_parameter);
    const HardwareQueue *const on = find_hardware_queue(found->queue);
    if (on == nullptr) return refuse(Status::invalid_parameter);
    const DxgkargNotifyWorkSubmissionRecord request = {on->driver, 0u, 0u};
    return answer(routine<std::int32_t (*)(const DxgkargNotifyWorkSubmissionRecord *)>(
        DdiSlot::DxgkDdiNotifyWorkSubmission)(&request));
}

std::uint32_t DdiKmt::doorbell_handle(std::uint32_t index)
{
    std::uint32_t seen = 0u;
    for (const Doorbell &doorbell : doorbells_) {
        if (doorbell.handle == 0u) continue;
        if (seen++ == index) return doorbell.handle;
    }
    return 0u;
}

void *DdiKmt::doorbell_page(std::uint32_t doorbell)
{
    const Doorbell *const found = find_doorbell(doorbell);
    return found != nullptr ? found->page : nullptr;
}

std::uint64_t *DdiKmt::queued_fence(std::uint32_t doorbell)
{
    Doorbell *const found = find_doorbell(doorbell);
    return found != nullptr ? &found->queued : nullptr;
}

void DdiKmt::allow_hardware_scheduling()
{
    hardware_scheduling_ = true;
}

DoorbellFabric &DdiKmt::fabric()
{
    return fabric_;
}

/* Feature 053. The miniport is real here - this rig drives DxgkDdiCreateAllocation
 * and DxgkDdiOpenAllocation - so the identity that comes back is the driver's
 * own. What is modelled is dxgkrnl's NT handle table, which is dxgkrnl's and
 * not the driver's, and which this class is standing in for. Nothing about the
 * miniport's half is faked: an open here really calls the driver with the
 * creating device's own bytes, which is the whole thing under test. */
Status DdiKmt::create_shared_allocation(std::uint32_t device, std::uint8_t *private_data, std::uint32_t size,
                                        std::uint32_t &resource)
{
    resource = 0u;
    if (private_data == nullptr || size > max_private_data) return refuse(Status::invalid_parameter);
    /* Asked for as a resource, because that is the only thing a share will
     * name (research.md §16). */
    std::uint32_t allocation = 0u;
    const Status  created = create_with(device, private_data, size, resource_flag, allocation);
    if (created != Status::ok) return created;
    /* The open dxgkrnl ran inside the create wrote the minted id into the
     * slot's copy; the caller keeps the bytes an opening process would be
     * given (research.md §15). */
    const Allocation *const slot = find_allocation(allocation);
    if (slot == nullptr || slot->resource == nullptr) return refuse(Status::invalid_state);
    std::memcpy(private_data, slot->private_data, size);
    resource = allocation;
    return refuse(Status::ok);
}

Status DdiKmt::destroy_shared_allocation(std::uint32_t device, std::uint32_t resource)
{
    /* One carrier is one resource holding one allocation, so this rig names
     * them by the same number; what reaches the driver names the resource. */
    const Allocation *const slot = find_allocation(resource);
    if (slot == nullptr || slot->resource == nullptr) return refuse(Status::invalid_parameter);
    return destroy_slot(device, resource, slot->resource);
}

DdiKmt::SharedHandle *DdiKmt::find_shared(std::uint64_t handle)
{
    if (handle == 0u) return nullptr;
    for (SharedHandle &row : shared_)
        if (row.handle == handle) return &row;
    return nullptr;
}

Status DdiKmt::share_object(std::uint32_t device, std::uint32_t resource, std::uint64_t &handle)
{
    handle = 0u;
    const std::uint32_t     allocation = resource;
    const Allocation *const slot = find_allocation(allocation);
    if (slot == nullptr || slot->device != device) return refuse(Status::invalid_parameter);
    /* dxgkrnl's handle-table type check: an allocation that is not a resource
     * is refused, and refused with no message (research.md §16). */
    if (slot->resource == nullptr) return refuse(Status::not_supported);
    if (slot->private_size != AllocationDescriptor::encoded_size + SharedObjectRecord::encoded_size)
        return refuse(Status::not_supported);
    for (SharedHandle &row : shared_) {
        if (row.handle != 0u) continue;
        row.handle = next_shared_handle_;
        next_shared_handle_ += 4u;
        row.allocation = allocation;
        handle = row.handle;
        return refuse(Status::ok);
    }
    return refuse(Status::no_space);
}

Status DdiKmt::open_shared_object(std::uint32_t device, std::uint64_t handle, std::uint32_t &resource,
                                  std::uint8_t *private_data, std::uint32_t capacity, std::uint32_t &written)
{
    resource = 0u;
    written = 0u;
    const SharedHandle *const row = find_shared(handle);
    if (row == nullptr || private_data == nullptr) return refuse(Status::invalid_parameter);
    Device *const     opener = find_device(device);
    Allocation *const created = find_allocation(row->allocation);
    if (opener == nullptr || created == nullptr) return refuse(Status::invalid_parameter);
    if (created->private_size > capacity) return refuse(Status::invalid_parameter);
    for (Allocation &slot : allocations_) {
        if (slot.handle != 0u || slot.block != nullptr) continue;
        /* A second handle on the same driver allocation, opened on the other
         * device with the *creating* device's bytes - which is exactly what
         * dxgkrnl carries between two processes. */
        slot.driver = created->driver;
        slot.bytes = created->bytes;
        slot.private_size = created->private_size;
        slot.opened_only = true;
        std::memcpy(slot.private_data, created->private_data, created->private_size);
        slot.handle = handles_.insert(slot.driver);
        if (slot.handle == 0u) {
            std::memset(static_cast<void *>(&slot), 0, sizeof slot);
            return refuse(Status::no_space);
        }
        slot.device = device;
        const Status opened = open_on(slot, opener->driver);
        if (opened != Status::ok) {
            handles_.remove(slot.handle);
            std::memset(static_cast<void *>(&slot), 0, sizeof slot);
            return opened;
        }
        std::memcpy(private_data, created->private_data, created->private_size);
        written = created->private_size;
        /* An open answers a resource of the opening process, as the kernel's
         * does; this rig names it by the same number as the allocation in it. */
        slot.resource = created->resource;
        resource = slot.handle;
        return refuse(Status::ok);
    }
    return refuse(Status::no_space);
}

Status DdiKmt::close_shared_object(std::uint64_t handle)
{
    SharedHandle *const row = find_shared(handle);
    if (row == nullptr) return refuse(Status::invalid_parameter);
    *row = SharedHandle{};
    return refuse(Status::ok);
}

Status DdiKmt::query_native_gpu_fences(std::uint32_t, bool &supported)
{
    supported = true;
    return Status::ok;
}

}  // namespace av
