#include "um/d3d/paging_queue.hpp"

#include "core/trace/function_scope.hpp"
#include "core/platform/poll_budget.hpp"

namespace av::d3d11 {

namespace {

constexpr uint64_t paging_patience_ns = 2000000000u;

}

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
    const HRESULT made = kernel_.pfnMakeResidentCb(device_, &resident);
    if (FAILED(made) && made != E_PENDING) return made;
    return wait_for(resident.PagingFenceValue);
}

HRESULT PagingQueue::queue_resident(D3DKMT_HANDLE allocation)
{
    AV_SCOPE("PagingQueue::queue_resident");
    if (queue_ == 0u) {
        const HRESULT opened = open();
        if (FAILED(opened)) return opened;
    }
    const UINT          priority = D3DDDI_ALLOCATIONPRIORITY_NORMAL;
    D3DDDI_MAKERESIDENT resident = {};
    resident.hPagingQueue = queue_;
    resident.NumAllocations = 1u;
    resident.AllocationList = &allocation;
    resident.PriorityList = &priority;
    const HRESULT made = kernel_.pfnMakeResidentCb(device_, &resident);
    return made == E_PENDING ? S_OK : made;
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

}
