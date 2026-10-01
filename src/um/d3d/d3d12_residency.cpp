#include "um/d3d/d3d12_residency.hpp"

#include "core/trace/function_scope.hpp"
#include "um/d3d/d3d12_records.hpp"

namespace av::d3d12 {

D3d12Residency::D3d12Residency(const D3D12DDI_CORELAYER_DEVICECALLBACKS_0050 &core, D3D12DDI_HRTDEVICE runtime)
    : core_(core), runtime_(runtime)
{
    AV_SCOPE("D3d12Residency::D3d12Residency");
}

HRESULT D3d12Residency::make_resident(D3D12DDIARG_MAKERESIDENT_0001 &args) const
{
    AV_SCOPE("D3d12Residency::make_resident");
    args.WaitMask = 0u;
    HRESULT       answer = S_OK;
    D3DKMT_HANDLE allocations[batch] = {};
    for (uint32_t first = 0u; first < args.NumObjects;) {
        uint32_t       consumed = 0u;
        const uint32_t count = gather(args.pObjects, args.NumObjects, first, allocations, consumed);
        first += consumed;
        if (count == 0u) continue;
        const HRESULT paged = page_in(args, allocations, count);
        if (FAILED(paged) && paged != E_PENDING) return paged;
        if (paged == E_PENDING) answer = E_PENDING;
    }
    return answer;
}

HRESULT D3d12Residency::evict(const D3D12DDIARG_EVICT &args) const
{
    AV_SCOPE("D3d12Residency::evict");
    D3DKMT_HANDLE allocations[batch] = {};
    for (uint32_t first = 0u; first < args.NumObjects;) {
        uint32_t       consumed = 0u;
        const uint32_t count = gather(args.pObjects, args.NumObjects, first, allocations, consumed);
        first += consumed;
        if (count == 0u) continue;
        D3DDDICB_EVICT evicted = {};
        evicted.Flags = args.Flags;
        evicted.NumAllocations = count;
        evicted.AllocationList = allocations;
        const HRESULT done = core_.pfnEvictCb(runtime_, &evicted);
        if (FAILED(done)) return done;
    }
    return S_OK;
}

/* A committed resource's heap is the pageable object D3D12Core names; the
 * resource itself is named only by an application asking for it. */
D3DKMT_HANDLE D3d12Residency::allocation_of(const D3D12DDI_HANDLE_AND_TYPE &object) const
{
    AV_SCOPE("D3d12Residency::allocation_of");
    if (object.Handle == nullptr) return 0u;
    if (object.Type == D3D12DDI_HT_HEAP) {
        const auto &heap = *static_cast<const HeapRecord *>(object.Handle);
        return heap.header.magic == HeapRecord::tag ? heap.allocation : 0u;
    }
    if (object.Type == D3D12DDI_HT_0012_RESOURCE) {
        const auto &resource = *static_cast<const ResourceRecord *>(object.Handle);
        return resource.header.magic == ResourceRecord::tag ? resource.allocation : 0u;
    }
    return 0u;
}

uint32_t D3d12Residency::gather(const D3D12DDI_HANDLE_AND_TYPE *objects, uint32_t count, uint32_t first,
                                D3DKMT_HANDLE *allocations, uint32_t &consumed) const
{
    AV_SCOPE("D3d12Residency::gather");
    uint32_t gathered = 0u;
    consumed = 0u;
    for (uint32_t index = first; objects != nullptr && index < count && gathered < batch; index++) {
        consumed++;
        const D3DKMT_HANDLE allocation = allocation_of(objects[index]);
        if (allocation != 0u) allocations[gathered++] = allocation;
    }
    if (objects == nullptr) consumed = count - first;
    return gathered;
}

/* Every adapter's queue pages the same allocations in: this device runs on
 * one GPU, and D3D12Core names one queue for it. */
HRESULT D3d12Residency::page_in(D3D12DDIARG_MAKERESIDENT_0001 &args, const D3DKMT_HANDLE *allocations,
                                uint32_t count) const
{
    AV_SCOPE("D3d12Residency::page_in");
    UINT priorities[batch] = {};
    for (uint32_t index = 0u; index < count; index++) priorities[index] = D3DDDI_ALLOCATIONPRIORITY_NORMAL;
    HRESULT answer = S_OK;
    for (UINT adapter = 0u; adapter < args.NumAdapters && args.pRTPagingQueue != nullptr; adapter++) {
        D3DDDI_MAKERESIDENT resident = {};
        resident.NumAllocations = count;
        resident.AllocationList = allocations;
        resident.PriorityList = priorities;
        resident.Flags = args.Flags;
        const HRESULT paged = core_.pfnMakeResidentCb(runtime_, args.pRTPagingQueue[adapter], &resident);
        if (FAILED(paged) && paged != E_PENDING) return paged;
        if (paged != E_PENDING || args.pPagingFenceValue == nullptr) continue;
        if ((args.WaitMask & (1u << adapter)) == 0u || args.pPagingFenceValue[adapter] < resident.PagingFenceValue)
            args.pPagingFenceValue[adapter] = resident.PagingFenceValue;
        args.WaitMask |= 1u << adapter;
        answer = E_PENDING;
    }
    return answer;
}

}  // namespace av::d3d12
