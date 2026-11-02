#include "miniport/present/present_request.hpp"

#include "miniport/ddi/dxgkarg_present_record.hpp"
#include "miniport/present/patched_present_addresses.hpp"
#include "miniport/present/virtual_present_addresses.hpp"
#include "core/trace/function_scope.hpp"
#include "proto/dma/present_command.hpp"
#include "proto/dma/rectangle.hpp"
#include "proto/wire/wire_writer.hpp"

namespace av {

PresentRequest::PresentRequest(Platform &platform, DxgkargPresentRecord &request)
    : platform_(platform), request_(request)
    {
        AV_SCOPE("PresentRequest::PresentRequest");
    }

Status PresentRequest::operation(PresentOperation &operation) const
{
    AV_SCOPE("PresentRequest::operation");
    switch (request_.Flags) {
    case flag_blt:
        operation = PresentOperation::blt;
        return Status::ok;
    case flag_color_fill:
        operation = PresentOperation::color_fill;
        return Status::ok;
    case flag_flip:
    case flag_flip | flag_flip_with_no_wait:
    case flag_flip | flag_flip_with_multiplane_overlay:
    case flag_flip | flag_flip_with_no_wait | flag_flip_with_multiplane_overlay:
        operation = PresentOperation::flip;
        return Status::ok;
    default:
        return Status::invalid_parameter;
    }
}

Status PresentRequest::validate(PresentOperation operation, const PresentAddressing &addressing) const
{
    AV_SCOPE("PresentRequest::validate");
    const DxgkargPresentRecord &r = request_;
    if (r.pDmaBuffer == nullptr || r.MultipassOffset > r.SubRectCnt) return Status::invalid_parameter;
    if (r.SubRectCnt != 0u && r.pDstSubRects == nullptr) return Status::invalid_parameter;
    const uint32_t references = operation == PresentOperation::blt ? 2u : operation == PresentOperation::flip ? 0u : 1u;
    if (addressing.validate(references) != Status::ok) return Status::invalid_parameter;
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
    AV_SCOPE("PresentRequest::present");
    platform_.require_irql_at_most(Irql::passive, "PresentRequest::present");
    PatchedPresentAddresses patched(platform_, request_);
    VirtualPresentAddresses virtual_addresses(platform_, request_);
    PresentAddressing &addressing = request_.pPatchLocationListOut != nullptr
                                        ? static_cast<PresentAddressing &>(patched)
                                        : static_cast<PresentAddressing &>(virtual_addresses);
    PresentOperation kind = PresentOperation::blt;
    Status status = operation(kind);
    if (status == Status::ok && kind == PresentOperation::flip && request_.pDmaBuffer == nullptr)
        return NtStatus(Status::ok);
    if (status == Status::ok) status = validate(kind, addressing);
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
                                 count, addressing.space());
    auto *const dma = static_cast<uint8_t *>(request_.pDmaBuffer);
    WireWriter writer(dma, PresentCommand::header_size + count * PresentCommand::sub_rectangle_size);
    command.write(writer);
    for (uint32_t index = 0u; index < count; index++) {
        const RectRecord &rect = request_.pDstSubRects[request_.MultipassOffset + index];
        Rectangle(rect.left, rect.top, rect.right, rect.bottom).write(writer);
    }
    const bool named = kind != PresentOperation::flip || addressing.validate(1u) == Status::ok;
    if (kind != PresentOperation::color_fill && named)
        addressing.refer(dma, source_index, PresentCommand::source_address_offset);
    if (kind != PresentOperation::flip)
        addressing.refer(dma, destination_index, PresentCommand::destination_address_offset);
    request_.pDmaBuffer = dma + writer.offset();
    if (count == remaining) return NtStatus(Status::ok);
    request_.MultipassOffset += count;
    return NtStatus(insufficient_dma_buffer);
}

}
