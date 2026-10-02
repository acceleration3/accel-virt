#include "miniport/scheduling/context.hpp"

#include "miniport/ddi/dxgkarg_create_context_record.hpp"
#include "miniport/memory/aperture_segment.hpp"
#include "core/trace/function_scope.hpp"
#include "proto/submission/follow_record.hpp"
#include "proto/submission/submission_descriptor.hpp"

namespace av {

Context::Context(Platform &platform, Device &device, const GpuMmuGeometry &geometry, uint32_t node, uint32_t flags)
    : platform_(platform), device_(device), node_(node), flags_(flags), root_(platform, geometry),
      av_record_(), is_av_(false)
      {
          AV_SCOPE("Context::Context");
      }

Device &Context::device() const
{
    AV_SCOPE("Context::device");
    platform_.require_irql_at_most(Irql::dispatch, "Context::device");
    return device_;
}

uint32_t Context::node() const
{
    AV_SCOPE("Context::node");
    platform_.require_irql_at_most(Irql::dispatch, "Context::node");
    return node_;
}

uint32_t Context::flags() const
{
    AV_SCOPE("Context::flags");
    platform_.require_irql_at_most(Irql::dispatch, "Context::flags");
    return flags_;
}

void Context::become_av(const AvContextRecord &record)
{
    AV_SCOPE("Context::become_av");
    platform_.require_irql_at_most(Irql::apc, "Context::become_av");
    av_record_ = record;
    is_av_ = true;
}

bool Context::is_av() const
{
    AV_SCOPE("Context::is_av");
    platform_.require_irql_at_most(Irql::dispatch, "Context::is_av");
    return is_av_;
}

const AvContextRecord &Context::av_record() const
{
    AV_SCOPE("Context::av_record");
    platform_.require_irql_at_most(Irql::dispatch, "Context::av_record");
    return av_record_;
}

void Context::report_to(DxgkargCreateContextRecord &request)
{
    AV_SCOPE("Context::report_to");
    platform_.require_irql_at_most(Irql::dispatch, "Context::report_to");
    request.hContext = this;
    request.ContextInfoDmaBufferSize = dma_buffer_bytes;
    request.ContextInfoDmaBufferSegmentSet = ApertureSegment::segment_set;
    /* Any other context may carry a FollowRecord (Adapter::submit_command_virtual). */
    request.ContextInfoDmaBufferPrivateDataSize = static_cast<uint32_t>(
        is_av_ ? SubmissionDescriptor::encoded_size : FollowRecord::encoded_size);
    request.ContextInfoAllocationListSize = allocation_list_entries;
    request.ContextInfoPatchLocationListSize = patch_location_entries;
    request.ContextInfoReserved = 0u;
    request.ContextInfoCaps = 0u;
    request.ContextInfoPagingCompanionNodeId = 0u;
}

Status Context::assign_root(const DxgkargSetRootPageTableRecord &root)
{
    AV_SCOPE("Context::assign_root");
    platform_.require_irql_at_most(Irql::passive, "Context::assign_root");
    return root_.assign(root);
}

bool Context::admits(uint64_t dma_address) const
{
    AV_SCOPE("Context::admits");
    platform_.require_irql_at_most(Irql::dispatch, "Context::admits");
    return root_.admits(dma_address);
}

}  // namespace av
