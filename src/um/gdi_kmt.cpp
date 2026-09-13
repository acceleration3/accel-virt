#include "um/gdi_kmt.hpp"

#include "core/support/nt_status.hpp"
#include "guest/kmt/pci_identity.hpp"

#include <windows.h>
typedef _Return_type_success_(return >= 0) LONG NTSTATUS;
#include <d3dkmthk.h>

/* The offsets dxgkrnl writes its answers at, so a header subset that widened a
 * union cannot make a zero handle look like the runtime's answer:
 * DxgkCreateDeviceImpl writes the device handle at +0x0c of D3DKMT_CREATEDEVICE. */
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

namespace av {

namespace {

constexpr uint32_t max_adapters = 16u;
constexpr uint64_t page_bytes = 4096u;
constexpr size_t   max_descriptor_bytes = 256u;
/* STATUS_IO_TIMEOUT: a failure, where STATUS_TIMEOUT would read as success. */
constexpr uint32_t timed_out = 0xC00000B5u;

}  // namespace

GdiKmt::GdiKmt() : event_(nullptr), last_code_(0u) {}

GdiKmt::~GdiKmt()
{
    if (event_ != nullptr) CloseHandle(event_);
}

Status GdiKmt::initialize()
{
    if (event_ != nullptr) return Status::invalid_state;
    event_ = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    return event_ != nullptr ? Status::ok : Status::no_memory;
}

Status GdiKmt::answer(int32_t code)
{
    const NtStatus status(code);
    last_code_ = static_cast<uint32_t>(status.value());
    return status.status();
}

Status GdiKmt::await_event()
{
    if (WaitForSingleObject(event_, wait_milliseconds) == WAIT_OBJECT_0) return Status::ok;
    last_code_ = timed_out;
    return NtStatus(timed_out).status();
}

Status GdiKmt::enumerate_adapters(uint32_t *adapters, uint32_t capacity, uint32_t &count)
{
    D3DKMT_ADAPTERINFO   infos[max_adapters] = {};
    D3DKMT_ENUMADAPTERS2 enumeration = {};
    enumeration.NumAdapters = max_adapters;
    enumeration.pAdapters = infos;
    const Status status = answer(D3DKMTEnumAdapters2(&enumeration));
    count = status == Status::ok ? enumeration.NumAdapters : 0u;
    for (uint32_t index = 0u; index < count && index < capacity && index < max_adapters; index++)
        adapters[index] = infos[index].hAdapter;
    return status;
}

Status GdiKmt::query_identity(uint32_t adapter, PciIdentity &identity)
{
    D3DKMT_QUERY_DEVICE_IDS ids = {};
    D3DKMT_QUERYADAPTERINFO query = {};
    query.hAdapter = adapter;
    query.Type = KMTQAITYPE_PHYSICALADAPTERDEVICEIDS;
    query.pPrivateDriverData = &ids;
    query.PrivateDriverDataSize = sizeof ids;
    const Status status = answer(D3DKMTQueryAdapterInfo(&query));
    identity = PciIdentity(ids.DeviceIds.VendorID, ids.DeviceIds.DeviceID, ids.DeviceIds.SubVendorID,
                           ids.DeviceIds.SubSystemID);
    return status;
}

Status GdiKmt::query_driver_version(uint32_t adapter, uint32_t &version)
{
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

Status GdiKmt::close_adapter(uint32_t adapter)
{
    D3DKMT_CLOSEADAPTER request = {};
    request.hAdapter = adapter;
    return answer(D3DKMTCloseAdapter(&request));
}

Status GdiKmt::create_device(uint32_t adapter, uint32_t &device)
{
    D3DKMT_CREATEDEVICE request = {};
    request.hAdapter = adapter;
    const Status status = answer(D3DKMTCreateDevice(&request));
    device = status == Status::ok ? request.hDevice : 0u;
    return status;
}

Status GdiKmt::destroy_device(uint32_t device)
{
    D3DKMT_DESTROYDEVICE request = {};
    request.hDevice = device;
    return answer(D3DKMTDestroyDevice(&request));
}

Status GdiKmt::create_allocation(uint32_t device, const uint8_t *descriptor, uint32_t size, uint32_t &allocation)
{
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

Status GdiKmt::destroy_allocation(uint32_t device, uint32_t allocation)
{
    const D3DKMT_HANDLE      handle = allocation;
    D3DKMT_DESTROYALLOCATION2 request = {};
    request.hDevice = device;
    request.phAllocationList = &handle;
    request.AllocationCount = 1u;
    return answer(D3DKMTDestroyAllocation2(&request));
}

/* D3DDDICB_LOCK2FLAGS is one 32-bit Reserved field in WDDM 3.2: a user-mode
 * lock asks for nothing and gets the view VidMm placed, which for an
 * allocation in a CPU-visible segment is the hostmem BAR itself. */
Status GdiKmt::lock(uint32_t device, uint32_t allocation, void *&view)
{
    D3DKMT_LOCK2 request = {};
    request.hDevice = device;
    request.hAllocation = allocation;
    const Status status = answer(D3DKMTLock2(&request));
    view = status == Status::ok ? request.pData : nullptr;
    return status;
}

Status GdiKmt::unlock(uint32_t device, uint32_t allocation)
{
    D3DKMT_UNLOCK2 request = {};
    request.hDevice = device;
    request.hAllocation = allocation;
    return answer(D3DKMTUnlock2(&request));
}

Status GdiKmt::create_paging_queue(uint32_t device, uint32_t &queue, uint32_t &fence_object)
{
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
    D3DDDI_DESTROYPAGINGQUEUE request = {};
    request.hPagingQueue = queue;
    return answer(D3DKMTDestroyPagingQueue(&request));
}

Status GdiKmt::make_resident(uint32_t queue, uint32_t allocation, uint64_t &paging_fence)
{
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
    D3DKMT_FREEGPUVIRTUALADDRESS request = {};
    request.hAdapter = adapter;
    request.BaseAddress = address;
    request.Size = pages * page_bytes;
    return answer(D3DKMTFreeGpuVirtualAddress(&request));
}

Status GdiKmt::wait_for_fence(uint32_t device, uint32_t fence_object, uint64_t fence)
{
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
    D3DKMT_CREATECONTEXTVIRTUAL request = {};
    request.hDevice = device;
    request.NodeOrdinal = 0u;
    request.EngineAffinity = 1u;
    request.ClientHint = D3DKMT_CLIENTHINT_UNKNOWN;
    const Status status = answer(D3DKMTCreateContextVirtual(&request));
    context = status == Status::ok ? request.hContext : 0u;
    return status;
}

Status GdiKmt::destroy_context(uint32_t context)
{
    D3DKMT_DESTROYCONTEXT request = {};
    request.hContext = context;
    return answer(D3DKMTDestroyContext(&request));
}

Status GdiKmt::submit(uint32_t context, uint64_t commands, uint64_t length)
{
    D3DKMT_SUBMITCOMMAND request = {};
    request.Commands = commands;
    request.CommandLength = static_cast<UINT>(length);
    request.BroadcastContextCount = 1u;
    request.BroadcastContext[0] = context;
    return answer(D3DKMTSubmitCommand(&request));
}

/* A CPU notification signalled behind everything the context has submitted. */
Status GdiKmt::wait_for_context(uint32_t device, uint32_t context)
{
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
    D3DKMT_WAITFORVERTICALBLANKEVENT wait = {};
    wait.hAdapter = adapter;
    wait.hDevice = device;
    wait.VidPnSourceId = source;
    return answer(D3DKMTWaitForVerticalBlankEvent(&wait));
}

uint64_t GdiKmt::performance_counter(uint64_t &frequency) const
{
    LARGE_INTEGER ticks = {};
    LARGE_INTEGER rate = {};
    QueryPerformanceFrequency(&rate);
    QueryPerformanceCounter(&ticks);
    frequency = static_cast<uint64_t>(rate.QuadPart);
    return static_cast<uint64_t>(ticks.QuadPart);
}

uint32_t GdiKmt::last_code() const { return last_code_; }

}  // namespace av
