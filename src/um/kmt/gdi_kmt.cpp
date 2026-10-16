#include "um/kmt/gdi_kmt.hpp"


#include "core/support/nt_status.hpp"
#include "core/trace/function_scope.hpp"
#include "guest/kmt/beam_position.hpp"
#include "guest/kmt/pci_location.hpp"
#include "guest/kmt/pci_identity.hpp"
#include "proto/escape/umd_adapter_info.hpp"

#include <windows.h>
typedef _Return_type_success_(return >= 0) LONG NTSTATUS;
#include <d3dkmthk.h>

/* The offsets dxgkrnl writes its answers at, so a header subset that widened a
 * union cannot make a zero handle look like the runtime's answer:
 * DxgkCreateDeviceImpl writes the device handle at +0x0c of D3DKMT_CREATEDEVICE. */
/* A 32-bit process has the x86 layout, which dxgkrnl's WoW64 path reads as
 * such. */
#if defined(_M_IX86)
static_assert(__builtin_offsetof(D3DKMT_CREATEDEVICE, hAdapter) == 0x00u);
static_assert(__builtin_offsetof(D3DKMT_CREATEDEVICE, Flags) == 0x04u);
static_assert(__builtin_offsetof(D3DKMT_CREATEDEVICE, hDevice) == 0x08u);
static_assert(__builtin_offsetof(D3DKMT_CREATEALLOCATION, hDevice) == 0x00u);
static_assert(__builtin_offsetof(D3DKMT_CREATEALLOCATION, NumAllocations) == 0x1Cu);
static_assert(__builtin_offsetof(D3DKMT_CREATEALLOCATION, pAllocationInfo2) == 0x20u);
static_assert(__builtin_offsetof(D3DDDI_ALLOCATIONINFO2, hAllocation) == 0x00u);
static_assert(__builtin_offsetof(D3DDDI_ALLOCATIONINFO2, pPrivateDriverData) == 0x08u);
static_assert(__builtin_offsetof(D3DDDI_ALLOCATIONINFO2, PrivateDriverDataSize) == 0x0Cu);
static_assert(__builtin_offsetof(D3DKMT_LOCK2, hDevice) == 0x00u);
static_assert(__builtin_offsetof(D3DKMT_LOCK2, hAllocation) == 0x04u);
static_assert(__builtin_offsetof(D3DKMT_LOCK2, pData) == 0x0Cu);
static_assert(__builtin_offsetof(D3DKMT_UNLOCK2, hAllocation) == 0x04u);
static_assert(__builtin_offsetof(D3DKMT_WAITFORVERTICALBLANKEVENT, hAdapter) == 0x00u);
static_assert(__builtin_offsetof(D3DKMT_WAITFORVERTICALBLANKEVENT, hDevice) == 0x04u);
static_assert(__builtin_offsetof(D3DKMT_WAITFORVERTICALBLANKEVENT, VidPnSourceId) == 0x08u);
static_assert(__builtin_offsetof(D3DKMT_ADAPTERINFO, AdapterLuid) == 0x04u);
static_assert(__builtin_offsetof(D3DKMT_QUERYVIDEOMEMORYINFO, hAdapter) == 0x04u);
static_assert(__builtin_offsetof(D3DKMT_QUERYVIDEOMEMORYINFO, MemorySegmentGroup) == 0x08u);
static_assert(__builtin_offsetof(D3DKMT_QUERYVIDEOMEMORYINFO, Budget) == 0x10u);
static_assert(__builtin_offsetof(D3DKMT_QUERYVIDEOMEMORYINFO, CurrentUsage) == 0x18u);
static_assert(__builtin_offsetof(D3DKMT_ESCAPE, Type) == 0x08u);
static_assert(__builtin_offsetof(D3DKMT_ESCAPE, pPrivateDriverData) == 0x10u);
static_assert(__builtin_offsetof(D3DKMT_ESCAPE, PrivateDriverDataSize) == 0x14u);
static_assert(__builtin_offsetof(D3DKMT_ESCAPE, hContext) == 0x18u);
#else
static_assert(__builtin_offsetof(D3DKMT_CREATEDEVICE, hAdapter) == 0x00u);
static_assert(__builtin_offsetof(D3DKMT_CREATEDEVICE, Flags) == 0x08u);
static_assert(__builtin_offsetof(D3DKMT_CREATEDEVICE, hDevice) == 0x0Cu);
static_assert(__builtin_offsetof(D3DKMT_CREATEALLOCATION, hDevice) == 0x00u);
static_assert(__builtin_offsetof(D3DKMT_CREATEALLOCATION, NumAllocations) == 0x2Cu);
static_assert(__builtin_offsetof(D3DKMT_CREATEALLOCATION, pAllocationInfo2) == 0x30u);
static_assert(__builtin_offsetof(D3DDDI_ALLOCATIONINFO2, hAllocation) == 0x00u);
static_assert(__builtin_offsetof(D3DDDI_ALLOCATIONINFO2, pPrivateDriverData) == 0x10u);
static_assert(__builtin_offsetof(D3DDDI_ALLOCATIONINFO2, PrivateDriverDataSize) == 0x18u);
static_assert(__builtin_offsetof(D3DKMT_LOCK2, hDevice) == 0x00u);
static_assert(__builtin_offsetof(D3DKMT_LOCK2, hAllocation) == 0x04u);
static_assert(__builtin_offsetof(D3DKMT_LOCK2, pData) == 0x10u);
static_assert(__builtin_offsetof(D3DKMT_UNLOCK2, hAllocation) == 0x04u);
static_assert(__builtin_offsetof(D3DKMT_WAITFORVERTICALBLANKEVENT, hAdapter) == 0x00u);
static_assert(__builtin_offsetof(D3DKMT_WAITFORVERTICALBLANKEVENT, hDevice) == 0x04u);
static_assert(__builtin_offsetof(D3DKMT_WAITFORVERTICALBLANKEVENT, VidPnSourceId) == 0x08u);
static_assert(__builtin_offsetof(D3DKMT_ADAPTERINFO, AdapterLuid) == 0x04u);
static_assert(__builtin_offsetof(D3DKMT_QUERYVIDEOMEMORYINFO, hAdapter) == 0x08u);
static_assert(__builtin_offsetof(D3DKMT_QUERYVIDEOMEMORYINFO, MemorySegmentGroup) == 0x0Cu);
static_assert(__builtin_offsetof(D3DKMT_QUERYVIDEOMEMORYINFO, Budget) == 0x10u);
static_assert(__builtin_offsetof(D3DKMT_QUERYVIDEOMEMORYINFO, CurrentUsage) == 0x18u);
static_assert(__builtin_offsetof(D3DKMT_ESCAPE, Type) == 0x08u);
static_assert(__builtin_offsetof(D3DKMT_ESCAPE, pPrivateDriverData) == 0x10u);
static_assert(__builtin_offsetof(D3DKMT_ESCAPE, PrivateDriverDataSize) == 0x18u);
static_assert(__builtin_offsetof(D3DKMT_ESCAPE, hContext) == 0x1Cu);
#endif

namespace av {

namespace {

/* OBJECT_ATTRIBUTES, which D3DKMTShareObjects takes a pointer to and which
 * d3dkmthk.h only forward declares. The header that defines it for user mode
 * is winternl.h, and the pinned kit's um closure does not carry it; ntdef.h,
 * which does, redefines two dozen types windows.h already has. So the six
 * fields are written out here and their offsets proven below, exactly as this
 * file proves the offsets dxgkrnl writes its answers at (feature 053).
 *
 * Nothing but Length is ever set: an object with no name is one no other
 * process can open by name, which is what a shared object wants. */
struct ShareAttributes {
    unsigned long length;
    void         *root_directory;
    void         *object_name;
    unsigned long attributes;
    void         *security_descriptor;
    void         *security_quality_of_service;
};
#if defined(_M_IX86)
static_assert(sizeof(ShareAttributes) == 24u);
static_assert(__builtin_offsetof(ShareAttributes, root_directory) == 0x04u);
static_assert(__builtin_offsetof(ShareAttributes, object_name) == 0x08u);
static_assert(__builtin_offsetof(ShareAttributes, attributes) == 0x0Cu);
static_assert(__builtin_offsetof(ShareAttributes, security_descriptor) == 0x10u);
static_assert(__builtin_offsetof(ShareAttributes, security_quality_of_service) == 0x14u);
#else
static_assert(sizeof(ShareAttributes) == 48u);
static_assert(__builtin_offsetof(ShareAttributes, root_directory) == 0x08u);
static_assert(__builtin_offsetof(ShareAttributes, object_name) == 0x10u);
static_assert(__builtin_offsetof(ShareAttributes, attributes) == 0x18u);
static_assert(__builtin_offsetof(ShareAttributes, security_descriptor) == 0x20u);
static_assert(__builtin_offsetof(ShareAttributes, security_quality_of_service) == 0x28u);
#endif

constexpr uint64_t page_bytes = 4096u;
constexpr size_t   max_descriptor_bytes = 256u;
/* STATUS_IO_TIMEOUT: a failure, where STATUS_TIMEOUT would read as success. */
constexpr uint32_t timed_out = 0xC00000B5u;
/* Node 1, the hardware-scheduled one (feature 049). */
constexpr uint32_t hardware_queue_node = 1u;

}  // namespace

GdiKmt::GdiKmt() : clock_(), event_(nullptr), last_code_(0u), known_adapters_{}, known_luids_{}, known_(0u)
{
    AV_SCOPE("GdiKmt::GdiKmt");
}

GdiKmt::~GdiKmt()
{
    AV_SCOPE("GdiKmt::~GdiKmt");
    if (event_ != nullptr) CloseHandle(event_);
}

Status GdiKmt::initialize()
{
    AV_SCOPE("GdiKmt::initialize");
    if (event_ != nullptr) return Status::invalid_state;
    event_ = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    return event_ != nullptr ? Status::ok : Status::no_memory;
}

Status GdiKmt::answer(int32_t code)
{
    AV_SCOPE("GdiKmt::answer");
    const NtStatus status(code);
    last_code_ = static_cast<uint32_t>(status.value());
    return status.status();
}

Status GdiKmt::await_event()
{
    AV_SCOPE("GdiKmt::await_event");
    if (WaitForSingleObject(event_, wait_milliseconds) == WAIT_OBJECT_0) return Status::ok;
    last_code_ = timed_out;
    return NtStatus(timed_out).status();
}

Status GdiKmt::enumerate_adapters(uint32_t *adapters, uint32_t capacity, uint32_t &count)
{
    AV_SCOPE("GdiKmt::enumerate_adapters");
    D3DKMT_ADAPTERINFO   infos[max_adapters] = {};
    D3DKMT_ENUMADAPTERS2 enumeration = {};
    enumeration.NumAdapters = max_adapters;
    enumeration.pAdapters = infos;
    const Status status = answer(D3DKMTEnumAdapters2(&enumeration));
    count = status == Status::ok ? enumeration.NumAdapters : 0u;
    for (uint32_t index = 0u; index < count && index < max_adapters; index++) {
        const uint64_t luid = static_cast<uint64_t>(static_cast<uint32_t>(infos[index].AdapterLuid.HighPart)) << 32u |
                              infos[index].AdapterLuid.LowPart;
        remember(infos[index].hAdapter, luid);
        if (index < capacity) adapters[index] = infos[index].hAdapter;
    }
    return status;
}

void GdiKmt::remember(uint32_t adapter, uint64_t luid)
{
    AV_SCOPE("GdiKmt::remember");
    for (uint32_t index = 0u; index < known_; index++) {
        if (known_adapters_[index] != adapter) continue;
        known_luids_[index] = luid;
        return;
    }
    if (known_ == known_capacity) return;
    known_adapters_[known_] = adapter;
    known_luids_[known_] = luid;
    known_++;
}

void GdiKmt::forget(uint32_t adapter)
{
    AV_SCOPE("GdiKmt::forget");
    for (uint32_t index = 0u; index < known_; index++) {
        if (known_adapters_[index] != adapter) continue;
        known_--;
        known_adapters_[index] = known_adapters_[known_];
        known_luids_[index] = known_luids_[known_];
        return;
    }
}

/* D3DKMTQueryAdapterInfo has no LUID query for an open adapter, so the LUID is
 * the one the enumeration that opened the handle reported. */
Status GdiKmt::adapter_luid(uint32_t adapter, uint64_t &luid)
{
    AV_SCOPE("GdiKmt::adapter_luid");
    luid = 0u;
    for (uint32_t index = 0u; index < known_; index++) {
        if (known_adapters_[index] != adapter) continue;
        luid = known_luids_[index];
        return answer(0);
    }
    return answer(NtStatus(Status::invalid_parameter).value());
}

/* The desktop's DC, which gdi32 alone makes (the OpenCL ICD imports no
 * user32): the adapter it opens is the one the primary display is on. */
Status GdiKmt::primary_display_luid(uint64_t &luid)
{
    AV_SCOPE("GdiKmt::primary_display_luid");
    luid = 0u;
    const HDC screen = CreateDCW(L"DISPLAY", nullptr, nullptr, nullptr);
    if (screen == nullptr) return answer(NtStatus(Status::not_found).value());
    D3DKMT_OPENADAPTERFROMHDC open = {};
    open.hDc = screen;
    const Status opened = answer(D3DKMTOpenAdapterFromHdc(&open));
    static_cast<void>(DeleteDC(screen));
    if (opened != Status::ok) return opened;
    luid = static_cast<uint64_t>(static_cast<uint32_t>(open.AdapterLuid.HighPart)) << 32u | open.AdapterLuid.LowPart;
    D3DKMT_CLOSEADAPTER close = {};
    close.hAdapter = open.hAdapter;
    static_cast<void>(D3DKMTCloseAdapter(&close));
    return opened;
}

Status GdiKmt::query_video_memory(uint32_t adapter, MemorySegmentGroup group, uint64_t &budget, uint64_t &usage)
{
    AV_SCOPE("GdiKmt::query_video_memory");
    D3DKMT_QUERYVIDEOMEMORYINFO query = {};
    query.hAdapter = adapter;
    query.MemorySegmentGroup = static_cast<D3DKMT_MEMORY_SEGMENT_GROUP>(group);
    const Status status = answer(D3DKMTQueryVideoMemoryInfo(&query));
    budget = status == Status::ok ? query.Budget : 0u;
    usage = status == Status::ok ? query.CurrentUsage : 0u;
    return status;
}

Status GdiKmt::query_identity(uint32_t adapter, PciIdentity &identity)
{
    AV_SCOPE("GdiKmt::query_identity");
    D3DKMT_QUERY_DEVICE_IDS ids = {};
    D3DKMT_QUERYADAPTERINFO query = {};
    query.hAdapter = adapter;
    query.Type = KMTQAITYPE_PHYSICALADAPTERDEVICEIDS;
    query.pPrivateDriverData = &ids;
    query.PrivateDriverDataSize = sizeof ids;
    const Status status = answer(D3DKMTQueryAdapterInfo(&query));
    identity = PciIdentity(ids.DeviceIds.VendorID, ids.DeviceIds.DeviceID, ids.DeviceIds.SubVendorID,
                           ids.DeviceIds.SubSystemID, ids.DeviceIds.RevisionID);
    return status;
}

Status GdiKmt::query_adapter_location(uint32_t adapter, PciLocation &location)
{
    AV_SCOPE("GdiKmt::query_adapter_location");
    D3DKMT_ADAPTERADDRESS   where = {};
    D3DKMT_QUERYADAPTERINFO query = {};
    query.hAdapter = adapter;
    query.Type = KMTQAITYPE_ADAPTERADDRESS;
    query.pPrivateDriverData = &where;
    query.PrivateDriverDataSize = sizeof where;
    const Status status = answer(D3DKMTQueryAdapterInfo(&query));
    location = status == Status::ok ? PciLocation(where.BusNumber, where.DeviceNumber, where.FunctionNumber)
                                   : PciLocation();
    return status;
}

Status GdiKmt::query_adapter_name(uint32_t adapter, uint16_t *name, uint32_t capacity)
{
    AV_SCOPE("GdiKmt::query_adapter_name");
    if (name == nullptr || capacity == 0u) return Status::invalid_parameter;
    name[0] = 0u;
    D3DKMT_ADAPTERREGISTRYINFO info = {};
    D3DKMT_QUERYADAPTERINFO    query = {};
    query.hAdapter = adapter;
    query.Type = KMTQAITYPE_ADAPTERREGISTRYINFO;
    query.pPrivateDriverData = &info;
    query.PrivateDriverDataSize = sizeof info;
    const Status status = answer(D3DKMTQueryAdapterInfo(&query));
    if (status != Status::ok) return status;
    constexpr uint32_t held = sizeof info.AdapterString / sizeof info.AdapterString[0];
    uint32_t           index = 0u;
    for (; index + 1u < capacity && index < held && info.AdapterString[index] != 0; ++index)
        name[index] = static_cast<uint16_t>(info.AdapterString[index]);
    name[index] = 0u;
    return Status::ok;
}

Status GdiKmt::query_segment_sizes(uint32_t adapter, uint64_t &dedicated_video, uint64_t &dedicated_system,
                                   uint64_t &shared_system)
{
    AV_SCOPE("GdiKmt::query_segment_sizes");
    D3DKMT_SEGMENTSIZEINFO  sizes = {};
    D3DKMT_QUERYADAPTERINFO query = {};
    query.hAdapter = adapter;
    query.Type = KMTQAITYPE_GETSEGMENTSIZE;
    query.pPrivateDriverData = &sizes;
    query.PrivateDriverDataSize = sizeof sizes;
    const Status status = answer(D3DKMTQueryAdapterInfo(&query));
    const bool   ok = status == Status::ok;
    dedicated_video = ok ? sizes.DedicatedVideoMemorySize : 0u;
    dedicated_system = ok ? sizes.DedicatedSystemMemorySize : 0u;
    shared_system = ok ? sizes.SharedSystemMemorySize : 0u;
    return status;
}

Status GdiKmt::query_kmd_driver_version(uint32_t adapter, uint64_t &version)
{
    AV_SCOPE("GdiKmt::query_kmd_driver_version");
    D3DKMT_KMD_DRIVER_VERSION reported = {};
    D3DKMT_QUERYADAPTERINFO   query = {};
    query.hAdapter = adapter;
    query.Type = KMTQAITYPE_KMD_DRIVER_VERSION;
    query.pPrivateDriverData = &reported;
    query.PrivateDriverDataSize = sizeof reported;
    const Status status = answer(D3DKMTQueryAdapterInfo(&query));
    version = status == Status::ok ? static_cast<uint64_t>(reported.DriverVersion.QuadPart) : 0u;
    return status;
}

Status GdiKmt::query_driver_version(uint32_t adapter, uint32_t &version)
{
    AV_SCOPE("GdiKmt::query_driver_version");
    D3DKMT_DRIVERVERSION    reported = KMT_DRIVERVERSION_WDDM_1_0;
    D3DKMT_QUERYADAPTERINFO query = {};
    query.hAdapter = adapter;
    query.Type = KMTQAITYPE_DRIVERVERSION;
    query.pPrivateDriverData = &reported;
    query.PrivateDriverDataSize = sizeof reported;
    const Status status = answer(D3DKMTQueryAdapterInfo(&query));
    version = static_cast<uint32_t>(reported);
    return status;
}

Status GdiKmt::query_adapter_features(uint32_t adapter, UmdAdapterInfo &info)
{
    AV_SCOPE("GdiKmt::query_adapter_features");
    uint8_t                 bytes[UmdAdapterInfo::encoded_size] = {};
    D3DKMT_QUERYADAPTERINFO query = {};
    query.hAdapter = adapter;
    query.Type = KMTQAITYPE_UMDRIVERPRIVATE;
    query.pPrivateDriverData = bytes;
    query.PrivateDriverDataSize = sizeof bytes;
    const Status status = answer(D3DKMTQueryAdapterInfo(&query));
    if (status != Status::ok) return status;
    return info.decode(bytes, sizeof bytes);
}

Status GdiKmt::query_native_gpu_fences(uint32_t adapter, bool &supported)
{
    AV_SCOPE("GdiKmt::query_native_gpu_fences");
    D3DKMT_WDDM_3_1_CAPS    caps = {};
    D3DKMT_QUERYADAPTERINFO query = {};
    query.hAdapter = adapter;
    query.Type = KMTQAITYPE_WDDM_3_1_CAPS;
    query.pPrivateDriverData = &caps;
    query.PrivateDriverDataSize = sizeof caps;
    const Status status = answer(D3DKMTQueryAdapterInfo(&query));
    supported = status == Status::ok && caps.NativeGpuFenceSupported != 0u;
    return status;
}

/* A millisecond on a high-resolution timer of the caller's own: Sleep(1) runs
 * on the system tick, 15.6 ms unless something raised it, which is what a
 * wait that outlasted spinning paid each round. A Windows without them
 * (before 10 1803) sleeps as before. */
void GdiKmt::sleep_briefly()
{
    AV_SCOPE("GdiKmt::sleep_briefly");
    const HANDLE timer =
        CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
    LARGE_INTEGER due = {};
    due.QuadPart = -10000;
    if (timer != nullptr && SetWaitableTimerEx(timer, &due, 0, nullptr, nullptr, nullptr, 0u) != FALSE)
        static_cast<void>(WaitForSingleObject(timer, INFINITE));
    else
        Sleep(1u);
    if (timer != nullptr) static_cast<void>(CloseHandle(timer));
}

Status GdiKmt::close_adapter(uint32_t adapter)
{
    AV_SCOPE("GdiKmt::close_adapter");
    D3DKMT_CLOSEADAPTER request = {};
    request.hAdapter = adapter;
    const Status closed = answer(D3DKMTCloseAdapter(&request));
    if (closed == Status::ok) forget(adapter);
    return closed;
}

Status GdiKmt::create_device(uint32_t adapter, uint32_t &device)
{
    AV_SCOPE("GdiKmt::create_device");
    D3DKMT_CREATEDEVICE request = {};
    request.hAdapter = adapter;
    const Status status = answer(D3DKMTCreateDevice(&request));
    device = status == Status::ok ? request.hDevice : 0u;
    return status;
}

Status GdiKmt::destroy_device(uint32_t device)
{
    AV_SCOPE("GdiKmt::destroy_device");
    D3DKMT_DESTROYDEVICE request = {};
    request.hDevice = device;
    return answer(D3DKMTDestroyDevice(&request));
}

Status GdiKmt::create_allocation(uint32_t device, const uint8_t *descriptor, uint32_t size, uint32_t &allocation)
{
    AV_SCOPE("GdiKmt::create_allocation");
    allocation = 0u;
    uint8_t private_data[max_descriptor_bytes] = {};
    if (size > sizeof private_data) return answer(static_cast<int32_t>(NtStatus(Status::invalid_parameter).value()));
    for (uint32_t index = 0u; index < size; index++) private_data[index] = descriptor[index];
    D3DDDI_ALLOCATIONINFO2  info = {};
    info.pPrivateDriverData = private_data;
    info.PrivateDriverDataSize = size;
    D3DKMT_CREATEALLOCATION request = {};
    request.hDevice = device;
    request.NumAllocations = 1u;
    request.pAllocationInfo2 = &info;
    const Status status = answer(D3DKMTCreateAllocation2(&request));
    allocation = status == Status::ok ? info.hAllocation : 0u;
    return status;
}

Status GdiKmt::destroy_shared_allocation(uint32_t device, uint32_t resource)
{
    AV_SCOPE("GdiKmt::destroy_shared_allocation");
    if (resource == 0u) return answer(static_cast<int32_t>(NtStatus(Status::invalid_parameter).value()));
    /* Named by the resource and by no allocation: dxgkrnl builds the list of
     * everything in it, which is the only call that takes the resource too. */
    D3DKMT_DESTROYALLOCATION2 request = {};
    request.hDevice = device;
    request.hResource = resource;
    return answer(D3DKMTDestroyAllocation2(&request));
}

Status GdiKmt::destroy_allocation(uint32_t device, uint32_t allocation)
{
    AV_SCOPE("GdiKmt::destroy_allocation");
    const D3DKMT_HANDLE      handle = allocation;
    D3DKMT_DESTROYALLOCATION2 request = {};
    request.hDevice = device;
    request.phAllocationList = &handle;
    request.AllocationCount = 1u;
    return answer(D3DKMTDestroyAllocation2(&request));
}

Status GdiKmt::create_shared_allocation(uint32_t device, uint8_t *private_data, uint32_t size,
                                        uint32_t &resource)
{
    AV_SCOPE("GdiKmt::create_shared_allocation");
    resource = 0u;
    /* Answered as too_large rather than invalid_parameter so that last_code()
     * tells a refusal of this file's own apart from one dxgkrnl made: both
     * would otherwise be STATUS_INVALID_PARAMETER, and a guest report saying
     * which call refused is worth nothing if it cannot say whose refusal it
     * was (feature 053). */
    if (size > max_descriptor_bytes) return answer(static_cast<int32_t>(NtStatus(Status::too_large).value()));
    if (private_data == nullptr)
        return answer(static_cast<int32_t>(NtStatus(Status::invalid_parameter).value()));
    /* The runtime keeps the buffer it was handed, and the driver writes the
     * minted id into it during the call; so unlike create_allocation this one
     * hands D3DKMT the caller's own bytes rather than a copy, and what comes
     * back is what the opening process will be given. */
    D3DDDI_ALLOCATIONINFO2  info = {};
    info.pPrivateDriverData = private_data;
    info.PrivateDriverDataSize = size;
    D3DKMT_CREATEALLOCATION request = {};
    request.hDevice = device;
    request.NumAllocations = 1u;
    request.pAllocationInfo2 = &info;
    /* Created as a resource, shared, and shared by NT handle rather than by
     * the old global one. D3DKMTShareObjects mints a handle over a resource
     * and refuses anything else, silently, so a carrier that is only an
     * allocation can never be shared at all (research.md §16). */
    request.Flags.CreateResource = 1u;
    request.Flags.CreateShared = 1u;
    request.Flags.NtSecuritySharing = 1u;
    /* A refusal is left legible by last_code(): this file is translation only,
     * and the ICD writes the line naming which call refused and with what
     * (feature 053). */
    const Status status = answer(D3DKMTCreateAllocation2(&request));
    if (status != Status::ok) return status;
    resource = request.hResource;
    return Status::ok;
}

Status GdiKmt::share_object(uint32_t device, uint32_t resource, uint64_t &handle)
{
    AV_SCOPE("GdiKmt::share_object");
    handle = 0u;
    if (resource == 0u) return answer(static_cast<int32_t>(NtStatus(Status::invalid_parameter).value()));
    /* No name and no security descriptor: a handle nothing can open by name is
     * a handle only a process this one duplicated it into can open, which is
     * what a shared object is for (feature 053, spec Edge Cases). */
    ShareAttributes     attributes = {};
    attributes.length = sizeof attributes;
    const D3DKMT_HANDLE shared = resource;
    HANDLE              made = nullptr;
    const Status        status = answer(D3DKMTShareObjects(
        1u, &shared, reinterpret_cast<POBJECT_ATTRIBUTES>(&attributes), STANDARD_RIGHTS_ALL | GENERIC_ALL,
        &made));
    if (status != Status::ok) return status;
    handle = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(made));
    /* The device is not named by D3DKMTShareObjects: what is shared is the
     * resource, which the kernel already knows the device of. */
    static_cast<void>(device);
    return Status::ok;
}

Status GdiKmt::open_shared_object(uint32_t device, uint64_t handle, uint32_t &resource,
                                  uint8_t *private_data, uint32_t capacity, uint32_t &written)
{
    AV_SCOPE("GdiKmt::open_shared_object");
    resource = 0u;
    written = 0u;
    if (handle == 0u || private_data == nullptr)
        return answer(static_cast<int32_t>(NtStatus(Status::invalid_parameter).value()));
    HANDLE const nt = reinterpret_cast<HANDLE>(static_cast<uintptr_t>(handle));
    /* Two calls, because the opener cannot know how much private data the
     * creator wrote until it asks: the first sizes the buffers, the second
     * fills them. */
    D3DKMT_QUERYRESOURCEINFOFROMNTHANDLE sizes = {};
    sizes.hDevice = device;
    sizes.hNtHandle = nt;
    const Status asked = answer(D3DKMTQueryResourceInfoFromNtHandle(&sizes));
    if (asked != Status::ok) return asked;
    if (sizes.NumAllocations != 1u || sizes.TotalPrivateDriverDataSize > capacity)
        return answer(static_cast<int32_t>(NtStatus(Status::invalid_parameter).value()));
    D3DDDI_OPENALLOCATIONINFO2           info = {};
    uint8_t                              runtime_data[max_descriptor_bytes] = {};
    uint8_t                              resource_data[max_descriptor_bytes] = {};
    if (sizes.PrivateRuntimeDataSize > sizeof runtime_data ||
        sizes.ResourcePrivateDriverDataSize > sizeof resource_data)
        return answer(static_cast<int32_t>(NtStatus(Status::invalid_parameter).value()));
    D3DKMT_OPENRESOURCEFROMNTHANDLE open = {};
    open.hDevice = device;
    open.hNtHandle = nt;
    open.NumAllocations = 1u;
    open.pOpenAllocationInfo2 = &info;
    open.pPrivateRuntimeData = runtime_data;
    open.PrivateRuntimeDataSize = sizes.PrivateRuntimeDataSize;
    open.pResourcePrivateDriverData = resource_data;
    open.ResourcePrivateDriverDataSize = sizes.ResourcePrivateDriverDataSize;
    open.pTotalPrivateDriverDataBuffer = private_data;
    open.TotalPrivateDriverDataBufferSize = capacity;
    const Status opened = answer(D3DKMTOpenResourceFromNtHandle(&open));
    if (opened != Status::ok) return opened;
    /* The resource, not `info.hAllocation`: destroying names the resource and
     * takes the allocation in it, and the two handles are of types dxgkrnl
     * refuses in each other's place (research.md §17). */
    resource = open.hResource;
    written = open.TotalPrivateDriverDataBufferSize;
    return Status::ok;
}

Status GdiKmt::close_shared_object(uint64_t handle)
{
    AV_SCOPE("GdiKmt::close_shared_object");
    if (handle == 0u) return answer(static_cast<int32_t>(NtStatus(Status::invalid_parameter).value()));
    /* kernel32, not gdi32: what D3DKMTShareObjects made is an ordinary NT
     * handle, and nothing in D3DKMT closes one. */
    if (CloseHandle(reinterpret_cast<HANDLE>(static_cast<uintptr_t>(handle))) == 0)
        return answer(static_cast<int32_t>(NtStatus(Status::invalid_parameter).value()));
    return answer(0);
}

/* D3DDDICB_LOCK2FLAGS is one 32-bit Reserved field in WDDM 3.2: a user-mode
 * lock asks for nothing and gets the view VidMm placed, which for an
 * allocation in a CPU-visible segment is the hostmem BAR itself. */
Status GdiKmt::lock(uint32_t device, uint32_t allocation, void *&view)
{
    AV_SCOPE("GdiKmt::lock");
    D3DKMT_LOCK2 request = {};
    request.hDevice = device;
    request.hAllocation = allocation;
    const Status status = answer(D3DKMTLock2(&request));
    view = status == Status::ok ? request.pData : nullptr;
    return status;
}

Status GdiKmt::unlock(uint32_t device, uint32_t allocation)
{
    AV_SCOPE("GdiKmt::unlock");
    D3DKMT_UNLOCK2 request = {};
    request.hDevice = device;
    request.hAllocation = allocation;
    return answer(D3DKMTUnlock2(&request));
}

Status GdiKmt::create_paging_queue(uint32_t device, uint32_t &queue, uint32_t &fence_object)
{
    AV_SCOPE("GdiKmt::create_paging_queue");
    D3DKMT_CREATEPAGINGQUEUE request = {};
    request.hDevice = device;
    request.Priority = D3DDDI_PAGINGQUEUE_PRIORITY_NORMAL;
    const Status status = answer(D3DKMTCreatePagingQueue(&request));
    queue = status == Status::ok ? request.hPagingQueue : 0u;
    fence_object = status == Status::ok ? request.hSyncObject : 0u;
    return status;
}

Status GdiKmt::destroy_paging_queue(uint32_t queue)
{
    AV_SCOPE("GdiKmt::destroy_paging_queue");
    D3DDDI_DESTROYPAGINGQUEUE request = {};
    request.hPagingQueue = queue;
    return answer(D3DKMTDestroyPagingQueue(&request));
}

Status GdiKmt::make_resident(uint32_t queue, uint32_t allocation, uint64_t &paging_fence)
{
    AV_SCOPE("GdiKmt::make_resident");
    const D3DKMT_HANDLE handle = allocation;
    const UINT          priority = D3DDDI_ALLOCATIONPRIORITY_NORMAL;
    D3DDDI_MAKERESIDENT request = {};
    request.hPagingQueue = queue;
    request.NumAllocations = 1u;
    request.AllocationList = &handle;
    request.PriorityList = &priority;
    const Status status = answer(D3DKMTMakeResident(&request));
    paging_fence = status == Status::ok ? request.PagingFenceValue : 0u;
    return status;
}

Status GdiKmt::map_gpu_virtual_address(uint32_t queue, uint32_t allocation, uint64_t pages, uint64_t &address,
                                       uint64_t &paging_fence)
{
    AV_SCOPE("GdiKmt::map_gpu_virtual_address");
    D3DDDI_MAPGPUVIRTUALADDRESS request = {};
    request.hPagingQueue = queue;
    request.hAllocation = allocation;
    request.SizeInPages = pages;
    request.Protection.Write = 1u;
    const Status status = answer(D3DKMTMapGpuVirtualAddress(&request));
    address = status == Status::ok ? request.VirtualAddress : 0u;
    paging_fence = status == Status::ok ? request.PagingFenceValue : 0u;
    return status;
}

Status GdiKmt::free_gpu_virtual_address(uint32_t adapter, uint64_t address, uint64_t pages)
{
    AV_SCOPE("GdiKmt::free_gpu_virtual_address");
    D3DKMT_FREEGPUVIRTUALADDRESS request = {};
    request.hAdapter = adapter;
    request.BaseAddress = address;
    request.Size = pages * page_bytes;
    return answer(D3DKMTFreeGpuVirtualAddress(&request));
}

Status GdiKmt::wait_for_fence(uint32_t device, uint32_t fence_object, uint64_t fence)
{
    AV_SCOPE("GdiKmt::wait_for_fence");
    const D3DKMT_HANDLE                       handle = fence_object;
    const UINT64                              value = fence;
    D3DKMT_WAITFORSYNCHRONIZATIONOBJECTFROMCPU request = {};
    request.hDevice = device;
    request.ObjectCount = 1u;
    request.ObjectHandleArray = &handle;
    request.FenceValueArray = &value;
    request.hAsyncEvent = event_;
    const Status status = answer(D3DKMTWaitForSynchronizationObjectFromCpu(&request));
    return status == Status::ok ? await_event() : status;
}

Status GdiKmt::create_context(uint32_t device, uint32_t &context)
{
    AV_SCOPE("GdiKmt::create_context");
    D3DKMT_CREATECONTEXTVIRTUAL request = {};
    request.hDevice = device;
    request.NodeOrdinal = 0u;
    request.EngineAffinity = 1u;
    request.ClientHint = D3DKMT_CLIENTHINT_UNKNOWN;
    const Status status = answer(D3DKMTCreateContextVirtual(&request));
    context = status == Status::ok ? request.hContext : 0u;
    return status;
}

Status GdiKmt::create_hw_queue_context(uint32_t device, const uint8_t *private_data, uint32_t size,
                                       uint32_t &context)
{
    AV_SCOPE("GdiKmt::create_hw_queue_context");
    return make_context(device, private_data, size, true, context);
}

Status GdiKmt::create_context_virtual(uint32_t device, const uint8_t *private_data, uint32_t size,
                                      uint32_t &context)
{
    AV_SCOPE("GdiKmt::create_context_virtual");
    return make_context(device, private_data, size, false, context);
}

Status GdiKmt::make_context(uint32_t device, const uint8_t *private_data, uint32_t size, bool hardware_queue,
                            uint32_t &context)
{
    AV_SCOPE("GdiKmt::make_context");
    D3DKMT_CREATECONTEXTVIRTUAL request = {};
    request.Flags.HwQueueSupported = hardware_queue ? 1u : 0u;
    request.hDevice = device;
    /* The hardware-scheduled node is the second one: a queue, and so a
     * doorbell, only runs on the node that reports context scheduling
     * (feature 049, research.md §16). */
    request.NodeOrdinal = hardware_queue ? hardware_queue_node : 0u;
    /* The engines within the node, of which each has one. dxgkrnl passes it
     * through to DXGKARG_CREATECONTEXT unjudged (DXGCONTEXT::Initialize) and
     * the miniport refuses any other bit: `1 << NodeOrdinal` asked node 1 for
     * an engine it does not have, so no hardware-queue context was ever made. */
    request.EngineAffinity = 1u;
    request.ClientHint = D3DKMT_CLIENTHINT_UNKNOWN;
    request.pPrivateDriverData = const_cast<uint8_t *>(private_data);
    request.PrivateDriverDataSize = size;
    const Status status = answer(D3DKMTCreateContextVirtual(&request));
    context = status == Status::ok ? request.hContext : 0u;
    return status;
}

Status GdiKmt::destroy_context(uint32_t context)
{
    AV_SCOPE("GdiKmt::destroy_context");
    D3DKMT_DESTROYCONTEXT request = {};
    request.hContext = context;
    return answer(D3DKMTDestroyContext(&request));
}

Status GdiKmt::submit(uint32_t context, uint64_t commands, uint64_t length, const uint8_t *private_data,
                      uint32_t private_size)
{
    AV_SCOPE("GdiKmt::submit");
    D3DKMT_SUBMITCOMMAND request = {};
    request.Commands = commands;
    request.CommandLength = static_cast<UINT>(length);
    request.pPrivateDriverData = const_cast<uint8_t *>(private_data);
    request.PrivateDriverDataSize = private_size;
    request.BroadcastContextCount = 1u;
    request.BroadcastContext[0] = context;
    return answer(D3DKMTSubmitCommand(&request));
}

Status GdiKmt::create_hw_queue(uint32_t, uint32_t context, GuestHwQueue &queue)
{
    AV_SCOPE("GdiKmt::create_hw_queue");
    queue = GuestHwQueue{};
    D3DKMT_CREATEHWQUEUE request = {};
    request.hHwContext = context;
    const Status made = answer(D3DKMTCreateHwQueue(&request));
    if (made != Status::ok) return made;
    /* The monitored fence a wait waits on, and the read-only mapping of its
     * value: the process's own view of what the driver has finished. */
    queue = GuestHwQueue(request.hHwQueue, request.hHwQueueProgressFence,
                         static_cast<const uint64_t *>(request.HwQueueProgressFenceCPUVirtualAddress));
    return Status::ok;
}

Status GdiKmt::submit_to_hw_queue(uint32_t queue, uint64_t commands, uint64_t length, const uint8_t *private_data,
                                  uint32_t private_size, uint64_t progress_fence_id)
{
    AV_SCOPE("GdiKmt::submit_to_hw_queue");
    D3DKMT_SUBMITCOMMANDTOHWQUEUE request = {};
    request.hHwQueue = queue;
    request.HwQueueProgressFenceId = progress_fence_id;
    request.CommandBuffer = commands;
    request.CommandLength = static_cast<UINT>(length);
    request.pPrivateDriverData = const_cast<uint8_t *>(private_data);
    request.PrivateDriverDataSize = private_size;
    return answer(D3DKMTSubmitCommandToHwQueue(&request));
}

/* The queue's progress is a monitored fence, so the wait is dxgkrnl's own:
 * it wakes when the driver reports the fence signalled and the value has been
 * reached. */
Status GdiKmt::wait_for_hw_queue(uint32_t device, uint32_t progress_fence, uint64_t value)
{
    AV_SCOPE("GdiKmt::wait_for_hw_queue");
    const D3DKMT_HANDLE                        object = progress_fence;
    const UINT64                               awaited = value;
    D3DKMT_WAITFORSYNCHRONIZATIONOBJECTFROMCPU request = {};
    request.hDevice = device;
    request.ObjectCount = 1u;
    request.ObjectHandleArray = &object;
    request.FenceValueArray = &awaited;
    request.hAsyncEvent = event_;
    const Status waited = answer(D3DKMTWaitForSynchronizationObjectFromCpu(&request));
    return waited != Status::ok ? waited : await_event();
}

Status GdiKmt::destroy_hw_queue(uint32_t queue)
{
    AV_SCOPE("GdiKmt::destroy_hw_queue");
    D3DKMT_DESTROYHWQUEUE request = {};
    request.hHwQueue = queue;
    return answer(D3DKMTDestroyHwQueue(&request));
}

Status GdiKmt::create_doorbell(uint32_t queue, uint32_t ring, uint32_t &doorbell, void *&page,
                               const uint32_t *&status, uint64_t *&queued)
{
    AV_SCOPE("GdiKmt::create_doorbell");
    doorbell = 0u;
    page = nullptr;
    status = nullptr;
    queued = nullptr;
    D3DKMT_CREATE_DOORBELL request = {};
    request.hHwQueue = queue;
    request.hRingBuffer = ring;
    /* The ring's head and tail live in its own header, so the ring is its own
     * control area - and dxgkrnl refuses a doorbell without one, whatever the
     * header's "opt" says (DXGDOORBELL::Initialize looks the handle up
     * unconditionally). */
    request.hRingBufferControl = ring;
    const Status made = answer(D3DKMTCreateDoorbell(&request));
    if (made != Status::ok) return made;
    doorbell = request.hDoorbell;
    page = request.DoorbellCPUVirtualAddress;
    status = static_cast<const uint32_t *>(request.DoorbellStatusCPUVirtualAddress);
    queued = static_cast<uint64_t *>(request.HwQueueProgressFenceLastQueuedValueCPUVirtualAddress);
    return Status::ok;
}

Status GdiKmt::connect_doorbell(uint32_t doorbell)
{
    AV_SCOPE("GdiKmt::connect_doorbell");
    D3DKMT_CONNECT_DOORBELL request = {};
    request.hDoorbell = doorbell;
    return answer(D3DKMTConnectDoorbell(&request));
}

Status GdiKmt::destroy_doorbell(uint32_t doorbell)
{
    AV_SCOPE("GdiKmt::destroy_doorbell");
    D3DKMT_DESTROY_DOORBELL request = {};
    request.hDoorbell = doorbell;
    return answer(D3DKMTDestroyDoorbell(&request));
}

Status GdiKmt::notify_work_submission(uint32_t doorbell)
{
    AV_SCOPE("GdiKmt::notify_work_submission");
    D3DKMT_NOTIFY_WORK_SUBMISSION request = {};
    request.hDoorbell = doorbell;
    return answer(D3DKMTNotifyWorkSubmission(&request));
}

/* A CPU notification signalled behind everything the context has submitted. */
Status GdiKmt::wait_for_context(uint32_t device, uint32_t context)
{
    AV_SCOPE("GdiKmt::wait_for_context");
    D3DKMT_CREATESYNCHRONIZATIONOBJECT2 notification = {};
    notification.hDevice = device;
    notification.Info.Type = D3DDDI_CPU_NOTIFICATION;
    notification.Info.CPUNotification.Event = event_;
    Status status = answer(D3DKMTCreateSynchronizationObject2(&notification));
    if (status != Status::ok) return status;
    const D3DKMT_HANDLE                        object = notification.hSyncObject;
    const D3DKMT_HANDLE                        broadcast = context;
    D3DKMT_SIGNALSYNCHRONIZATIONOBJECTFROMGPU2 signal = {};
    signal.ObjectCount = 1u;
    signal.ObjectHandleArray = &object;
    signal.BroadcastContextCount = 1u;
    signal.BroadcastContextArray = &broadcast;
    status = answer(D3DKMTSignalSynchronizationObjectFromGpu2(&signal));
    if (status == Status::ok) status = await_event();
    D3DKMT_DESTROYSYNCHRONIZATIONOBJECT destroy = {};
    destroy.hSyncObject = object;
    const uint32_t code = last_code_;
    const Status   destroyed = answer(D3DKMTDestroySynchronizationObject(&destroy));
    if (status == Status::ok) return destroyed;
    last_code_ = code;
    return status;
}

/* dxgkrnl blocks until the driver reports the next CRTC vsync for the source,
 * or answers at once when it is not reporting them. */
Status GdiKmt::wait_for_vertical_blank(uint32_t adapter, uint32_t device, uint32_t source)
{
    AV_SCOPE("GdiKmt::wait_for_vertical_blank");
    D3DKMT_WAITFORVERTICALBLANKEVENT wait = {};
    wait.hAdapter = adapter;
    wait.hDevice = device;
    wait.VidPnSourceId = source;
    return answer(D3DKMTWaitForVerticalBlankEvent(&wait));
}

/* dxgkrnl answers from the driver's DxgkDdiGetScanLine, which reads the
 * device's display clock. */
Status GdiKmt::scan_line(uint32_t adapter, uint32_t source, BeamPosition &position)
{
    AV_SCOPE("GdiKmt::scan_line");
    D3DKMT_GETSCANLINE read = {};
    read.hAdapter = adapter;
    read.VidPnSourceId = source;
    const Status answered = answer(D3DKMTGetScanLine(&read));
    if (answered == Status::ok) position = BeamPosition(read.InVerticalBlank != FALSE, read.ScanLine);
    return answered;
}

/* dxgkrnl copies the data into the kernel, hands the copy to DxgkDdiEscape and
 * copies it back out, so an answer the driver wrote lands in `data`. */
Status GdiKmt::escape(uint32_t adapter, uint32_t device, uint32_t context, uint8_t *data, uint32_t size)
{
    AV_SCOPE("GdiKmt::escape");
    D3DKMT_ESCAPE request = {};
    request.hAdapter = adapter;
    request.hDevice = device;
    request.Type = D3DKMT_ESCAPE_DRIVERPRIVATE;
    request.pPrivateDriverData = data;
    request.PrivateDriverDataSize = size;
    request.hContext = context;
    return answer(D3DKMTEscape(&request));
}

uint64_t GdiKmt::performance_counter(uint64_t &frequency) const
{
    AV_SCOPE("GdiKmt::performance_counter");
    frequency = clock_.frequency();
    return clock_.ticks();
}

uint32_t GdiKmt::last_code() const
{
    AV_SCOPE("GdiKmt::last_code");
    return last_code_;
}

}  // namespace av
