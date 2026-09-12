#include "core/scheduling/context.hpp"

#include "core/ddi/dxgkarg_create_context_record.hpp"
#include "core/memory/aperture_segment.hpp"

namespace av {

Context::Context(Platform &platform, Device &device, const GpuMmuGeometry &geometry, uint32_t node, uint32_t flags)
    : platform_(platform), device_(device), node_(node), flags_(flags), root_(platform, geometry) {}

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
    request.ContextInfoDmaBufferSegmentSet = ApertureSegment::segment_set;
    request.ContextInfoDmaBufferPrivateDataSize = 0u;
    request.ContextInfoAllocationListSize = allocation_list_entries;
    request.ContextInfoPatchLocationListSize = patch_location_entries;
    request.ContextInfoReserved = 0u;
    request.ContextInfoCaps = 0u;
    request.ContextInfoPagingCompanionNodeId = 0u;
}

Status Context::assign_root(const DxgkargSetRootPageTableRecord &root)
{
    platform_.require_irql_at_most(Irql::passive, "Context::assign_root");
    return root_.assign(root);
}

bool Context::admits(uint64_t dma_address) const
{
    platform_.require_irql_at_most(Irql::dispatch, "Context::admits");
    return root_.admits(dma_address);
}

}  // namespace av
