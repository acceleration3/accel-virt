#include "core/present/presented_allocations.hpp"

#include "core/ddi/dxgk_allocation_list_record.hpp"
#include "core/ddi/dxgk_present_allocation_info_record.hpp"
#include "core/ddi/dxgkarg_present_record.hpp"
#include "core/trace/function_scope.hpp"

namespace av {

PresentedAllocations::PresentedAllocations(const DxgkargPresentRecord &request) : request_(request)
{
    AV_SCOPE("PresentedAllocations::PresentedAllocations");
}

void *PresentedAllocations::source() const
{
    AV_SCOPE("PresentedAllocations::source");
    if (request_.pAllocationList == nullptr) return nullptr;
    if (fixed()) return at(source_index);
    return request_.NumSrcAllocations == 1u && request_.NumDstAllocations == 1u ? at(0u) : nullptr;
}

void *PresentedAllocations::destination() const
{
    AV_SCOPE("PresentedAllocations::destination");
    if (request_.pAllocationList == nullptr) return nullptr;
    if (fixed()) return at(destination_index);
    return request_.NumDstAllocations == 1u && request_.NumSrcAllocations <= 1u ? at(request_.NumSrcAllocations)
                                                                                : nullptr;
}

bool PresentedAllocations::fixed() const
{
    AV_SCOPE("PresentedAllocations::fixed");
    return request_.pPatchLocationListOut == nullptr && request_.NumSrcAllocations == 0u &&
           request_.NumDstAllocations == 0u;
}

void *PresentedAllocations::at(uint32_t index) const
{
    AV_SCOPE("PresentedAllocations::at");
    if (request_.pPatchLocationListOut != nullptr)
        return static_cast<const DxgkAllocationListRecord *>(request_.pAllocationList)[index].hDeviceSpecificAllocation;
    return static_cast<const DxgkPresentAllocationInfoRecord *>(request_.pAllocationList)[index]
        .hDeviceSpecificAllocation;
}

}  // namespace av
