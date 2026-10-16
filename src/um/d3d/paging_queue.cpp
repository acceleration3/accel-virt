#include "um/d3d/paging_queue.hpp"

#include "core/trace/function_scope.hpp"
#include "guest/transport/counter_spin.hpp"

namespace av::d3d11 {

namespace {

/* A present waits on this, not the GPU: paging an aperture allocation in is a
 * page-table update the miniport answers at once, so a fence still short after
 * two seconds is a kernel that will not page it. */
constexpr uint64_t paging_patience_us = 2u * CounterSpin::microseconds_per_second;

}  // namespace

PagingQueue::PagingQueue(const D3DDDI_DEVICECALLBACKS &kernel, HANDLE device, const Kmt &kmt)
    : kernel_(kernel), device_(device), kmt_(kmt), queue_(0u), fence_(nullptr)
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
    CounterSpin patience(kmt_, paging_patience_us);
    while (*fence_ < fence) {
        if (!patience.lasts()) return E_FAIL;
        Sleep(0u);
    }
    return S_OK;
}

}  // namespace av::d3d11
