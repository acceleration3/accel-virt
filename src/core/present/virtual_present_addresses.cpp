#include "core/present/virtual_present_addresses.hpp"

#include "core/ddi/dxgk_present_allocation_info_record.hpp"
#include "core/ddi/dxgkarg_present_record.hpp"
#include "proto/wire/wire_writer.hpp"

namespace av {

VirtualPresentAddresses::VirtualPresentAddresses(Platform &platform, const DxgkargPresentRecord &request)
    : platform_(platform), request_(request) {}

PresentAddressSpace VirtualPresentAddresses::space() const
{
    platform_.require_irql_at_most(Irql::passive, "VirtualPresentAddresses::space");
    return PresentAddressSpace::gpu_virtual;
}

Status VirtualPresentAddresses::validate(uint32_t) const
{
    platform_.require_irql_at_most(Irql::passive, "VirtualPresentAddresses::validate");
    return request_.pAllocationList != nullptr ? Status::ok : Status::invalid_parameter;
}

void VirtualPresentAddresses::refer(uint8_t *command, uint32_t allocation_index, uint32_t offset)
{
    platform_.require_irql_at_most(Irql::passive, "VirtualPresentAddresses::refer");
    const auto *allocations = static_cast<const DxgkPresentAllocationInfoRecord *>(request_.pAllocationList);
    WireWriter(command + offset, address_bytes).write_u64(allocations[allocation_index].AllocationVirtualAddress);
}

}  // namespace av
