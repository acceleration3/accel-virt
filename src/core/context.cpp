#include "context.hpp"

#include "ddi/dxgkarg_create_context_record.hpp"

namespace av {

Context::Context(Platform &platform, Device &device, uint32_t node, uint32_t flags)
    : platform_(platform), device_(device), node_(node), flags_(flags) {}

Device &Context::device() const
{
    platform_.require_irql_at_most(Irql::dispatch, "Context::device");
    return device_;
}

uint32_t Context::node() const
{
    platform_.require_irql_at_most(Irql::dispatch, "Context::node");
    return node_;
}

uint32_t Context::flags() const
{
    platform_.require_irql_at_most(Irql::dispatch, "Context::flags");
    return flags_;
}

void Context::report_to(DxgkargCreateContextRecord &request)
{
    platform_.require_irql_at_most(Irql::dispatch, "Context::report_to");
    request.hContext = this;
    request.ContextInfoDmaBufferSize = dma_buffer_bytes;
    request.ContextInfoDmaBufferSegmentSet = 0u;
    request.ContextInfoDmaBufferPrivateDataSize = 0u;
    request.ContextInfoAllocationListSize = allocation_list_entries;
    request.ContextInfoPatchLocationListSize = patch_location_entries;
    request.ContextInfoReserved = 0u;
    request.ContextInfoCaps = 0u;
    request.ContextInfoPagingCompanionNodeId = 0u;
}

}  // namespace av
