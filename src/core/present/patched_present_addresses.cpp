#include "core/present/patched_present_addresses.hpp"

#include "core/ddi/dxgkarg_present_record.hpp"
#include "core/trace/function_scope.hpp"

namespace av {

PatchedPresentAddresses::PatchedPresentAddresses(Platform &platform, DxgkargPresentRecord &request)
    : platform_(platform), request_(request)
    {
        AV_SCOPE("PatchedPresentAddresses::PatchedPresentAddresses");
    }

PresentAddressSpace PatchedPresentAddresses::space() const
{
    AV_SCOPE("PatchedPresentAddresses::space");
    platform_.require_irql_at_most(Irql::passive, "PatchedPresentAddresses::space");
    return PresentAddressSpace::physical;
}

Status PatchedPresentAddresses::validate(uint32_t references) const
{
    AV_SCOPE("PatchedPresentAddresses::validate");
    platform_.require_irql_at_most(Irql::passive, "PatchedPresentAddresses::validate");
    const bool room = request_.pPatchLocationListOut != nullptr && request_.PatchLocationListOutSize >= references;
    return room ? Status::ok : Status::invalid_parameter;
}

void PatchedPresentAddresses::refer(uint8_t *, uint32_t allocation_index, uint32_t offset)
{
    AV_SCOPE("PatchedPresentAddresses::refer");
    platform_.require_irql_at_most(Irql::passive, "PatchedPresentAddresses::refer");
    *request_.pPatchLocationListOut = D3dddiPatchLocationListRecord{allocation_index, 0u, 0u, 0u, offset, 0u};
    request_.pPatchLocationListOut++;
}

}  // namespace av
