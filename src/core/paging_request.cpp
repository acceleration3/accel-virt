#include "paging_request.hpp"

#include "../proto/paging_command_writer.hpp"
#include "ddi/dxgk_build_paging_buffer_copy_page_table_entries_record.hpp"
#include "ddi/dxgk_build_paging_buffer_copy_range_record.hpp"
#include "ddi/dxgk_build_paging_buffer_fill_virtual_record.hpp"
#include "ddi/dxgk_build_paging_buffer_flush_tlb_record.hpp"
#include "ddi/dxgk_build_paging_buffer_transfer_virtual_record.hpp"
#include "ddi/dxgk_build_paging_buffer_update_page_table_record.hpp"
#include "ddi/dxgkarg_build_paging_buffer_record.hpp"
#include "gpu_mmu_geometry.hpp"
#include "page_table_update.hpp"

namespace av {

PagingRequest::PagingRequest(Platform &platform, DxgkargBuildPagingBufferRecord &request)
    : platform_(platform), request_(request) {}

template <typename Operation> void PagingRequest::read_operation(Operation &operation) const
{
    platform_.copy(&operation, request_.Reserved, sizeof operation);
}

NtStatus PagingRequest::build()
{
    platform_.require_irql_at_most(Irql::passive, "PagingRequest::build");
    PagingCommandWriter writer(static_cast<uint8_t *>(request_.pDmaBuffer), request_.DmaSize);
    switch (request_.Operation) {
    case discard_content:
    case map_aperture_segment:
    case unmap_aperture_segment:
    case notify_residency: return NtStatus(Status::ok);
    case update_page_table: return NtStatus(update_table());
    case flush_tlb: return finish(write_flush(writer), writer.length());
    case virtual_fill: return finish(write_fill(writer), writer.length());
    case virtual_transfer: return finish(write_transfer(writer), writer.length());
    case copy_page_table_entries: return copy_entries(writer);
    default: return NtStatus(Status::invalid_parameter);
    }
}

Status PagingRequest::update_table() const
{
    DxgkBuildPagingBufferUpdatePageTableRecord update;
    read_operation(update);
    return PageTableUpdate(platform_, update).write();
}

Status PagingRequest::write_flush(PagingCommandWriter &writer) const
{
    DxgkBuildPagingBufferFlushTlbRecord flush;
    read_operation(flush);
    return writer.flush_tlb(flush.RootPageTableAddressSegmentId, flush.RootPageTableAddressSegmentOffset,
                            flush.StartVirtualAddress, flush.EndVirtualAddress);
}

Status PagingRequest::write_fill(PagingCommandWriter &writer) const
{
    DxgkBuildPagingBufferFillVirtualRecord fill;
    read_operation(fill);
    return writer.fill(fill.DestinationVirtualAddress, fill.FillSizeInBytes, fill.FillPattern);
}

Status PagingRequest::write_transfer(PagingCommandWriter &writer) const
{
    DxgkBuildPagingBufferTransferVirtualRecord transfer;
    read_operation(transfer);
    return writer.transfer(transfer.SourceVirtualAddress, transfer.DestinationVirtualAddress,
                           transfer.TransferSizeInBytes, transfer.SourcePageTable, transfer.DestinationPageTable,
                           static_cast<uint32_t>(transfer.TransferDirection), transfer.Flags);
}

NtStatus PagingRequest::copy_entries(PagingCommandWriter &writer)
{
    uint32_t next = request_.MultipassOffset;
    const Status status = write_copy(writer, next);
    if (status == Status::invalid_parameter) return NtStatus(status);
    /* A header with no range after it is not worth a buffer. */
    const bool progressed = next != request_.MultipassOffset;
    request_.MultipassOffset = next;
    return finish(status, progressed ? writer.length() : 0u);
}

Status PagingRequest::write_copy(PagingCommandWriter &writer, uint32_t &next) const
{
    DxgkBuildPagingBufferCopyPageTableEntriesRecord copy;
    read_operation(copy);
    if (copy.pRanges == nullptr || next >= copy.NumRanges) return Status::invalid_parameter;
    Status status = writer.copy_page_table_entries();
    while (status == Status::ok && next < copy.NumRanges) {
        status = write_range(writer, copy.pRanges[next]);
        if (status == Status::ok) next++;
    }
    return status;
}

Status PagingRequest::write_range(PagingCommandWriter &writer, const DxgkBuildPagingBufferCopyRangeRecord &range) const
{
    const uint64_t source_end = uint64_t{range.SrcStartPteIndex} + range.NumPageTableEntries;
    const uint64_t destination_end = uint64_t{range.DstStartPteIndex} + range.NumPageTableEntries;
    if (source_end > GpuMmuGeometry::entries_per_table || destination_end > GpuMmuGeometry::entries_per_table)
        return Status::invalid_parameter;
    return writer.copy_range(range.SrcPageTableAddress, range.DstPageTableAddress, range.SrcStartPteIndex,
                             range.DstStartPteIndex, range.NumPageTableEntries);
}

NtStatus PagingRequest::finish(Status status, size_t length)
{
    if (request_.pDmaBuffer == nullptr) return NtStatus(Status::invalid_parameter);
    request_.pDmaBuffer = static_cast<uint8_t *>(request_.pDmaBuffer) + length;
    return status == Status::ok ? NtStatus(Status::ok) : NtStatus(insufficient_dma_buffer);
}

}  // namespace av
