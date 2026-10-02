#include "miniport/scheduling/patch_request.hpp"

#include "miniport/ddi/d3dddi_patch_location_list_record.hpp"
#include "miniport/ddi/dxgk_allocation_list_record.hpp"
#include "miniport/ddi/dxgkarg_patch_record.hpp"
#include "core/trace/function_scope.hpp"
#include "proto/wire/wire_writer.hpp"

namespace av {

PatchRequest::PatchRequest(Platform &platform, const DxgkargPatchRecord &request)
    : platform_(platform), request_(request)
    {
        AV_SCOPE("PatchRequest::PatchRequest");
    }

Status PatchRequest::validate() const
{
    AV_SCOPE("PatchRequest::validate");
    const DxgkargPatchRecord &r = request_;
    const uint64_t last = uint64_t{r.PatchLocationListSubmissionStart} + r.PatchLocationListSubmissionLength;
    if (r.pDmaBuffer == nullptr || r.DmaBufferSubmissionEndOffset > r.DmaBufferSize ||
        r.DmaBufferSubmissionStartOffset > r.DmaBufferSubmissionEndOffset || last > r.PatchLocationListSize)
        return Status::invalid_parameter;
    if (r.PatchLocationListSubmissionLength != 0u && (r.pPatchLocationList == nullptr || r.pAllocationList == nullptr))
        return Status::invalid_parameter;
    for (uint64_t index = r.PatchLocationListSubmissionStart; index < last; index++) {
        const D3dddiPatchLocationListRecord &location = r.pPatchLocationList[index];
        const uint64_t end = uint64_t{location.PatchOffset} + address_bytes;
        if (location.AllocationIndex >= r.AllocationListSize || location.PatchOffset < r.DmaBufferSubmissionStartOffset ||
            end > r.DmaBufferSubmissionEndOffset)
            return Status::invalid_parameter;
    }
    return Status::ok;
}

Status PatchRequest::patch()
{
    AV_SCOPE("PatchRequest::patch");
    platform_.require_irql_at_most(Irql::passive, "PatchRequest::patch");
    const Status status = validate();
    if (status != Status::ok) return status;
    const uint64_t last = uint64_t{request_.PatchLocationListSubmissionStart} + request_.PatchLocationListSubmissionLength;
    for (uint64_t index = request_.PatchLocationListSubmissionStart; index < last; index++) {
        const D3dddiPatchLocationListRecord &location = request_.pPatchLocationList[index];
        const DxgkAllocationListRecord &allocation = request_.pAllocationList[location.AllocationIndex];
        WireWriter writer(static_cast<uint8_t *>(request_.pDmaBuffer) + location.PatchOffset, address_bytes);
        writer.write_u64(static_cast<uint64_t>(allocation.PhysicalAddress) + location.AllocationOffset);
    }
    return Status::ok;
}

}  // namespace av
