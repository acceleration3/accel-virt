#include "miniport/scheduling/render_km_tally.hpp"

#include "core/platform/invariant_scope.hpp"
#include "core/trace/function_scope.hpp"
#include "core/trace/state_writer.hpp"
#include "miniport/ddi/dxgk_allocation_list_record.hpp"
#include "miniport/ddi/dxgk_renderkm_command_record.hpp"
#include "miniport/ddi/dxgkarg_render_record.hpp"
#include "miniport/memory/allocation.hpp"
#include "miniport/memory/device_allocation.hpp"
#include "miniport/scheduling/device_table.hpp"
#include "proto/resources/allocation_descriptor.hpp"

#include <stddef.h>

namespace av {

namespace {

/* OpCode and CommandSize: all of a command that every operation has. */
constexpr uint32_t command_header_bytes = offsetof(DxgkRenderkmCommandRecord, BitBlt);
/* A BitBlt's arguments through DstAllocationIndex, the part that is read. */
constexpr uint32_t bitblt_read_bytes =
    command_header_bytes + static_cast<uint32_t>(offsetof(DxgkGdiargBitbltRecord, NumSubRects));

}  // namespace

RenderKmTally::RenderKmTally(Platform &platform, DeviceTable &devices)
    : platform_(platform), devices_(devices), calls_(0u), bitblts_(0u), color_fills_(0u), alpha_blends_(0u),
      stretch_blts_(0u), transparent_blts_(0u), cleartype_blends_(0u), escapes_(0u), unknown_(0u),
      last_bitblt_source_kind_(0u), last_bitblt_destination_kind_(0u)
      {
          AV_SCOPE("RenderKmTally::RenderKmTally");
      }

NtStatus RenderKmTally::render(DxgkargRenderRecord &request)
{
    AV_SCOPE("RenderKmTally::render");
    platform_.require_irql_at_most(Irql::passive, "RenderKmTally::render");
    InvariantScope<RenderKmTally> guard(platform_, *this, "render");
    if (request.pDmaBuffer == nullptr || (request.CommandLength != 0u && request.pCommand == nullptr))
        return NtStatus(Status::invalid_parameter);
    /* Before the count, so a buffer dxgkrnl retries with more room is not counted twice. */
    if (request.DmaSize < no_op_bytes) return NtStatus(insufficient_dma_buffer);
    calls_++;
    const Status walked = walk(request);
    if (walked != Status::ok) return NtStatus(walked);
    platform_.zero(request.pDmaBuffer, no_op_bytes);
    request.pDmaBuffer = static_cast<uint8_t *>(request.pDmaBuffer) + no_op_bytes;
    return NtStatus(Status::ok);
}

Status RenderKmTally::walk(const DxgkargRenderRecord &request)
{
    AV_SCOPE("RenderKmTally::walk");
    const uint8_t *const commands = static_cast<const uint8_t *>(request.pCommand);
    uint32_t             offset = 0u;
    while (offset < request.CommandLength) {
        const uint32_t remaining = request.CommandLength - offset;
        if (remaining < command_header_bytes) return Status::invalid_parameter;
        /* The buffer is bytes of another component's making: read it as such. */
        DxgkRenderkmCommandRecord command = {};
        platform_.copy(&command, commands + offset, remaining < bitblt_read_bytes ? remaining : bitblt_read_bytes);
        /* A size that does not move on, or runs past the buffer, ends the walk. */
        if (command.CommandSize < command_header_bytes || command.CommandSize > remaining)
            return Status::invalid_parameter;
        count(command, command.CommandSize, request);
        offset += command.CommandSize;
    }
    return Status::ok;
}

void RenderKmTally::count(const DxgkRenderkmCommandRecord &command, uint32_t available,
                          const DxgkargRenderRecord &request)
{
    AV_SCOPE("RenderKmTally::count");
    switch (static_cast<DxgkRenderkmOperation>(command.OpCode)) {
    case DxgkRenderkmOperation::bitblt:
        bitblts_++;
        /* A command too short to carry the indices is still a BitBlt. */
        last_bitblt_source_kind_ =
            available >= bitblt_read_bytes ? kind_at(request, command.BitBlt.SrcAllocationIndex) : 0u;
        last_bitblt_destination_kind_ =
            available >= bitblt_read_bytes ? kind_at(request, command.BitBlt.DstAllocationIndex) : 0u;
        break;
    case DxgkRenderkmOperation::colorfill: color_fills_++; break;
    case DxgkRenderkmOperation::alphablend: alpha_blends_++; break;
    case DxgkRenderkmOperation::stretchblt: stretch_blts_++; break;
    case DxgkRenderkmOperation::escape: escapes_++; break;
    case DxgkRenderkmOperation::transparentblt: transparent_blts_++; break;
    case DxgkRenderkmOperation::cleartypeblend: cleartype_blends_++; break;
    default: unknown_++; break;
    }
}

uint32_t RenderKmTally::kind_at(const DxgkargRenderRecord &request, uint32_t index)
{
    AV_SCOPE("RenderKmTally::kind_at");
    if (request.pAllocationList == nullptr || index >= request.AllocationListSize) return 0u;
    const DeviceAllocation *const opened = devices_.opened(request.pAllocationList[index].hDeviceSpecificAllocation);
    return opened != nullptr ? static_cast<uint32_t>(opened->allocation().descriptor().kind()) : 0u;
}

void RenderKmTally::report(StateWriter &writer) const
{
    AV_SCOPE("RenderKmTally::report");
    platform_.require_irql_at_most(Irql::passive, "RenderKmTally::report");
    writer.number("renderkm_calls", calls_);
    writer.number("renderkm_bitblt", bitblts_);
    writer.number("renderkm_colorfill", color_fills_);
    writer.number("renderkm_alphablend", alpha_blends_);
    writer.number("renderkm_stretchblt", stretch_blts_);
    writer.number("renderkm_transparentblt", transparent_blts_);
    writer.number("renderkm_cleartypeblend", cleartype_blends_);
    writer.number("renderkm_escape", escapes_);
    writer.number("renderkm_unknown", unknown_);
    writer.number("renderkm_last_bitblt_source_kind", last_bitblt_source_kind_);
    writer.number("renderkm_last_bitblt_destination_kind", last_bitblt_destination_kind_);
}

bool RenderKmTally::invariants_hold() const
{
    AV_SCOPE("RenderKmTally::invariants_hold");
    /* A kind is only ever read off a BitBlt. */
    return bitblts_ != 0u || (last_bitblt_source_kind_ == 0u && last_bitblt_destination_kind_ == 0u);
}

}  // namespace av
