#include "present_request.hpp"

#include "../proto/present_command.hpp"
#include "../proto/rectangle.hpp"
#include "../proto/wire_writer.hpp"
#include "ddi/dxgkarg_present_record.hpp"

namespace av {

PresentRequest::PresentRequest(Platform &platform, DxgkargPresentRecord &request)
    : platform_(platform), request_(request) {}

Status PresentRequest::operation(PresentOperation &operation) const
{
    switch (request_.Flags) {
    case flag_blt:
        operation = PresentOperation::blt;
        return Status::ok;
    case flag_color_fill:
        operation = PresentOperation::color_fill;
        return Status::ok;
    case flag_flip:
    case flag_flip | flag_flip_with_no_wait:
        operation = PresentOperation::flip;
        return Status::ok;
    default:
        return Status::invalid_parameter;
    }
}

/* Only the sub-rectangles not yet presented are read. */
Status PresentRequest::validate(PresentOperation operation) const
{
    const DxgkargPresentRecord &r = request_;
    if (r.pDmaBuffer == nullptr || r.MultipassOffset > r.SubRectCnt) return Status::invalid_parameter;
    if (r.SubRectCnt != 0u && r.pDstSubRects == nullptr) return Status::invalid_parameter;
    const uint32_t patches = operation == PresentOperation::blt ? 2u : 1u;
    if (r.pPatchLocationListOut == nullptr || r.PatchLocationListOutSize < patches)
        return Status::invalid_parameter;
    if (!Rectangle(r.SrcRect.left, r.SrcRect.top, r.SrcRect.right, r.SrcRect.bottom).is_valid() ||
        !Rectangle(r.DstRect.left, r.DstRect.top, r.DstRect.right, r.DstRect.bottom).is_valid())
        return Status::invalid_parameter;
    for (uint32_t index = r.MultipassOffset; index < r.SubRectCnt; index++) {
        const RectRecord &rect = r.pDstSubRects[index];
        if (!Rectangle(rect.left, rect.top, rect.right, rect.bottom).is_valid()) return Status::invalid_parameter;
    }
    return Status::ok;
}

NtStatus PresentRequest::present()
{
    platform_.require_irql_at_most(Irql::passive, "PresentRequest::present");
    PresentOperation kind = PresentOperation::blt;
    Status status = operation(kind);
    if (status == Status::ok) status = validate(kind);
    if (status != Status::ok) return NtStatus(status);
    const uint32_t remaining = request_.SubRectCnt - request_.MultipassOffset;
    const uint32_t needed = PresentCommand::header_size + (remaining != 0u ? PresentCommand::sub_rectangle_size : 0u);
    if (request_.DmaSize < needed) return NtStatus(insufficient_dma_buffer);
    const uint32_t room = (request_.DmaSize - PresentCommand::header_size) / PresentCommand::sub_rectangle_size;
    const uint32_t count = remaining < room ? remaining : room;
    const RectRecord &source = request_.SrcRect;
    const RectRecord &destination = request_.DstRect;
    const PresentCommand command(kind, kind == PresentOperation::color_fill ? request_.Color : 0u,
                                 Rectangle(source.left, source.top, source.right, source.bottom),
                                 Rectangle(destination.left, destination.top, destination.right, destination.bottom),
                                 kind == PresentOperation::flip ? static_cast<uint32_t>(request_.FlipInterval) : 0u,
                                 count);
    auto *const dma = static_cast<uint8_t *>(request_.pDmaBuffer);
    /* Exactly the bytes shown to fit, so nothing the writer is given overflows. */
    WireWriter writer(dma, PresentCommand::header_size + count * PresentCommand::sub_rectangle_size);
    command.write(writer);
    for (uint32_t index = 0u; index < count; index++) {
        const RectRecord &rect = request_.pDstSubRects[request_.MultipassOffset + index];
        Rectangle(rect.left, rect.top, rect.right, rect.bottom).write(writer);
    }
    if (kind != PresentOperation::color_fill) write_patch(source_index, PresentCommand::source_address_offset);
    if (kind != PresentOperation::flip) write_patch(destination_index, PresentCommand::destination_address_offset);
    request_.pDmaBuffer = dma + writer.offset();
    if (count == remaining) return NtStatus(Status::ok);
    request_.MultipassOffset += count;
    return NtStatus(insufficient_dma_buffer);
}

void PresentRequest::write_patch(uint32_t allocation_index, uint32_t offset)
{
    *request_.pPatchLocationListOut = D3dddiPatchLocationListRecord{allocation_index, 0u, 0u, 0u, offset, 0u};
    request_.pPatchLocationListOut++;
}

}  // namespace av
