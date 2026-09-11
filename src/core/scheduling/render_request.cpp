#include "core/scheduling/render_request.hpp"

#include "core/ddi/d3dddi_patch_location_list_record.hpp"
#include "core/ddi/dxgkarg_render_record.hpp"
#include "core/scheduling/patch_request.hpp"

namespace av {

RenderRequest::RenderRequest(Platform &platform, DxgkargRenderRecord &request)
    : platform_(platform), request_(request) {}

Status RenderRequest::validate() const
{
    const DxgkargRenderRecord &r = request_;
    const uint32_t patches = r.PatchLocationListInSize;
    if (r.pDmaBuffer == nullptr || r.CommandLength > r.DmaSize) return Status::invalid_parameter;
    if (r.CommandLength != 0u && r.pCommand == nullptr) return Status::invalid_parameter;
    if (patches > r.PatchLocationListOutSize) return Status::invalid_parameter;
    if (patches != 0u && (r.pPatchLocationListIn == nullptr || r.pPatchLocationListOut == nullptr ||
                          r.pAllocationList == nullptr))
        return Status::invalid_parameter;
    for (uint32_t index = 0u; index < patches; index++) {
        const D3dddiPatchLocationListRecord &location = r.pPatchLocationListIn[index];
        const uint64_t end = uint64_t{location.PatchOffset} + PatchRequest::address_bytes;
        if (location.AllocationIndex >= r.AllocationListSize || end > r.CommandLength) return Status::invalid_parameter;
    }
    return Status::ok;
}

Status RenderRequest::render()
{
    platform_.require_irql_at_most(Irql::passive, "RenderRequest::render");
    const Status status = validate();
    if (status != Status::ok) return status;
    const uint32_t patches = request_.PatchLocationListInSize;
    if (request_.CommandLength != 0u) platform_.copy(request_.pDmaBuffer, request_.pCommand, request_.CommandLength);
    if (patches != 0u)
        platform_.copy(request_.pPatchLocationListOut, request_.pPatchLocationListIn,
                       sizeof(D3dddiPatchLocationListRecord) * patches);
    request_.pDmaBuffer = static_cast<uint8_t *>(request_.pDmaBuffer) + request_.CommandLength;
    request_.pPatchLocationListOut += patches;
    return Status::ok;
}

}  // namespace av
