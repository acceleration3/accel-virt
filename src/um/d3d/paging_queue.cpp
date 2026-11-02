#include "um/d3d/paging_queue.hpp"

#include "core/trace/function_scope.hpp"
#include "core/platform/poll_budget.hpp"

namespace av::d3d11 {

namespace {

/* A present waits on this, not the GPU: paging an aperture allocation in is a
 * page-table update the miniport answers at once, so a fence still short after
 * two seconds is a kernel that will not page it. */
constexpr uint64_t paging_patience_ns = 2000000000u;
constexpr uint64_t page_bytes = 4096u;

}  // namespace

PagingQueue::PagingQueue(const D3DDDI_DEVICECALLBACKS &kernel, HANDLE device, PollClock &clock)
    : kernel_(kernel), device_(device), clock_(clock), queue_(0u), fence_(nullptr)
{
    AV_SCOPE("PagingQueue::PagingQueue");
}

PagingQueue::~PagingQueue()
{
    AV_SCOPE("PagingQueue::~PagingQueue");
    if (queue_ == 0u) return;
    D3DDDI_DESTROYPAGINGQUEUE destroy = {};
    destroy.hPagingQueue = queue_;
    static_cast<void>(kernel_.pfnDestroyPagingQueueCb(device_, &destroy));
}

HRESULT PagingQueue::make_resident(D3DKMT_HANDLE allocation)
{
    AV_SCOPE("PagingQueue::make_resident");
    if (queue_ == 0u) {
        const HRESULT opened = open();
        if (FAILED(opened)) return opened;
    }
    const UINT         priority = D3DDDI_ALLOCATIONPRIORITY_NORMAL;
    D3DDDI_MAKERESIDENT resident = {};
    resident.hPagingQueue = queue_;
    resident.NumAllocations = 1u;
    resident.AllocationList = &allocation;
    resident.PriorityList = &priority;
    /* The kernel answers E_PENDING when it queued the paging: the allocation
     * is resident once the queue's fence reaches the value it hands back. */
    const HRESULT made = kernel_.pfnMakeResidentCb(device_, &resident);
    if (FAILED(made) && made != E_PENDING) return made;
    return wait_for(resident.PagingFenceValue);
}

HRESULT PagingQueue::map_address(D3DKMT_HANDLE allocation, uint64_t pages, uint64_t &address)
{
    AV_SCOPE("PagingQueue::map_address");
    address = 0u;
    if (kernel_.pfnMapGpuVirtualAddressCb == nullptr) return E_NOTIMPL;
    if (queue_ == 0u) {
        const HRESULT opened = open();
        if (FAILED(opened)) return opened;
    }
    D3DDDI_MAPGPUVIRTUALADDRESS map = {};
    map.hPagingQueue = queue_;
    map.hAllocation = allocation;
    map.SizeInPages = pages;
    map.Protection.Write = 1u;
    /* The kernel answers E_PENDING when it queued the mapping, with the
     * address already chosen. */
    const HRESULT made = kernel_.pfnMapGpuVirtualAddressCb(device_, &map);
    if (FAILED(made) && made != E_PENDING) return made;
    if (map.VirtualAddress == 0u) return E_FAIL;
    address = map.VirtualAddress;
    return S_OK;
}

HRESULT PagingQueue::unmap_address(uint64_t address, uint64_t pages)
{
    AV_SCOPE("PagingQueue::unmap_address");
    if (address == 0u) return S_OK;
    if (kernel_.pfnFreeGpuVirtualAddressCb == nullptr) return E_NOTIMPL;
    D3DDDICB_FREEGPUVIRTUALADDRESS release = {};
    release.BaseAddress = address;
    release.Size = pages * page_bytes;
    return kernel_.pfnFreeGpuVirtualAddressCb(device_, &release);
}

HRESULT PagingQueue::open()
{
    AV_SCOPE("PagingQueue::open");
    D3DDDICB_CREATEPAGINGQUEUE create = {};
    create.Priority = D3DDDI_PAGINGQUEUE_PRIORITY_NORMAL;
    const HRESULT made = kernel_.pfnCreatePagingQueueCb(device_, &create);
    if (FAILED(made)) return made;
    queue_ = create.hPagingQueue;
    fence_ = static_cast<const volatile uint64_t *>(create.FenceValueCPUVirtualAddress);
    return S_OK;
}

HRESULT PagingQueue::wait_for(uint64_t fence) const
{
    AV_SCOPE("PagingQueue::wait_for");
    if (fence_ == nullptr) return E_FAIL;
    PollBudget patience(clock_, 0u, 0u, paging_patience_ns);
    while (*fence_ < fence) {
        if (!patience.next()) return E_FAIL;
    }
    return S_OK;
}

}  // namespace av::d3d11
