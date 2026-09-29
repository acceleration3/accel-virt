#include "core/memory/paging_request.hpp"

#include "core/ddi/dxgk_build_paging_buffer_copy_page_table_entries_record.hpp"
#include "core/ddi/dxgk_build_paging_buffer_copy_range_record.hpp"
#include "core/ddi/dxgk_build_paging_buffer_fill_virtual_record.hpp"
#include "core/ddi/dxgk_build_paging_buffer_map_aperture_segment_record.hpp"
#include "core/ddi/dxgk_build_paging_buffer_unmap_aperture_segment_record.hpp"
#include "core/ddi/dxgk_build_paging_buffer_flush_tlb_record.hpp"
#include "core/ddi/dxgk_build_paging_buffer_transfer_virtual_record.hpp"
#include "core/ddi/dxgk_build_paging_buffer_update_page_table_record.hpp"
#include "core/ddi/dxgkarg_build_paging_buffer_record.hpp"
#include "core/memory/allocation.hpp"
#include "core/memory/allocation_table.hpp"
#include "core/memory/aperture_segment.hpp"
#include "core/memory/gpu_mmu_geometry.hpp"
#include "core/memory/page_table_update.hpp"
#include "core/platform/log_budget.hpp"
#include "core/support/text_buffer.hpp"
#include "core/trace/function_scope.hpp"
#include "proto/commands/paging_command_writer.hpp"

namespace av {

PagingRequest::PagingRequest(Platform &platform, AllocationTable &allocations, LogBudget &budget,
                             DxgkargBuildPagingBufferRecord &request)
    : platform_(platform), allocations_(allocations), budget_(budget), request_(request)
    {
        AV_SCOPE("PagingRequest::PagingRequest");
    }

template <typename Operation> void PagingRequest::read_operation(Operation &operation) const
{
    AV_SCOPE("PagingRequest::read_operation");
    platform_.copy(&operation, request_.Reserved, sizeof operation);
}

NtStatus PagingRequest::build()
{
    AV_SCOPE("PagingRequest::build");
    platform_.require_irql_at_most(Irql::passive, "PagingRequest::build");
    PagingCommandWriter writer(static_cast<uint8_t *>(request_.pDmaBuffer), request_.DmaSize);
    switch (request_.Operation) {
    case discard_content:
    case notify_residency: return NtStatus(Status::ok);
    case map_aperture_segment: return NtStatus(map_aperture());
    case unmap_aperture_segment: return NtStatus(unmap_aperture());
    case update_page_table: return NtStatus(update_table());
    case flush_tlb: return finish(write_flush(writer), writer.length());
    case virtual_fill: return finish(write_fill(writer), writer.length());
    case virtual_transfer: return finish(write_transfer(writer), writer.length());
    case copy_page_table_entries: return copy_entries(writer);
    default: return refuse();
    }
}

Status PagingRequest::map_aperture()
{
    AV_SCOPE("PagingRequest::map_aperture");
    DxgkBuildPagingBufferMapApertureSegmentRecord map;
    read_operation(map);
    Allocation *const allocation = map.SegmentId == ApertureSegment::segment_id
                                       ? allocations_.find(map.hAllocation)
                                       : nullptr;
    const Status recorded = allocation != nullptr ? allocation->map_aperture(map) : Status::not_found;
    log_aperture("map", map.SegmentId, map.OffsetInPages, map.NumberOfPages, recorded);
    return Status::ok;
}

Status PagingRequest::unmap_aperture()
{
    AV_SCOPE("PagingRequest::unmap_aperture");
    DxgkBuildPagingBufferUnmapApertureSegmentRecord unmap;
    read_operation(unmap);
    Allocation *const allocation = unmapped();
    if (allocation != nullptr) allocation->unmap_aperture(unmap);
    log_aperture("unmap", unmap.SegmentId, unmap.OffsetInPages, unmap.NumberOfPages,
                 allocation != nullptr ? Status::ok : Status::not_found);
    return Status::ok;
}

Allocation *PagingRequest::unmapped() const
{
    AV_SCOPE("PagingRequest::unmapped");
    if (request_.Operation != unmap_aperture_segment) return nullptr;
    DxgkBuildPagingBufferUnmapApertureSegmentRecord unmap;
    read_operation(unmap);
    return unmap.SegmentId == ApertureSegment::segment_id ? allocations_.find(unmap.hAllocation) : nullptr;
}

NtStatus PagingRequest::refuse()
{
    AV_SCOPE("PagingRequest::refuse");
    if (budget_.allows()) {
        char       bytes[64];
        TextBuffer line(bytes, sizeof bytes);
        line.append("rvkgpu: paging operation ");
        line.append_decimal(static_cast<uint64_t>(request_.Operation));
        line.append(" refused");
        platform_.log(line.text());
    }
    return NtStatus(Status::invalid_parameter);
}

void PagingRequest::log_aperture(const char *operation, uint32_t segment, uint64_t offset, uint64_t pages,
                                 Status status)
{
    AV_SCOPE("PagingRequest::log_aperture");
    if (!budget_.allows()) return;
    char       bytes[128];
    TextBuffer line(bytes, sizeof bytes);
    line.append("rvkgpu: aperture ");
    line.append(operation);
    line.append(" segment ");
    line.append_decimal(segment);
    line.append(" offset ");
    line.append_decimal(offset);
    line.append(" pages ");
    line.append_decimal(pages);
    line.append(" status ");
    line.append_decimal(static_cast<uint64_t>(status));
    platform_.log(line.text());
}

Status PagingRequest::update_table() const
{
    AV_SCOPE("PagingRequest::update_table");
    DxgkBuildPagingBufferUpdatePageTableRecord update;
    read_operation(update);
    return PageTableUpdate(platform_, update).write();
}

Status PagingRequest::write_flush(PagingCommandWriter &writer) const
{
    AV_SCOPE("PagingRequest::write_flush");
    DxgkBuildPagingBufferFlushTlbRecord flush;
    read_operation(flush);
    return writer.flush_tlb(flush.RootPageTableAddressSegmentId, flush.RootPageTableAddressSegmentOffset,
                            flush.StartVirtualAddress, flush.EndVirtualAddress);
}

Status PagingRequest::write_fill(PagingCommandWriter &writer) const
{
    AV_SCOPE("PagingRequest::write_fill");
    DxgkBuildPagingBufferFillVirtualRecord fill;
    read_operation(fill);
    return writer.fill(fill.DestinationVirtualAddress, fill.FillSizeInBytes, fill.FillPattern);
}

Status PagingRequest::write_transfer(PagingCommandWriter &writer) const
{
    AV_SCOPE("PagingRequest::write_transfer");
    DxgkBuildPagingBufferTransferVirtualRecord transfer;
    read_operation(transfer);
    return writer.transfer(transfer.SourceVirtualAddress, transfer.DestinationVirtualAddress,
                           transfer.TransferSizeInBytes, transfer.SourcePageTable, transfer.DestinationPageTable,
                           static_cast<uint32_t>(transfer.TransferDirection), transfer.Flags);
}

NtStatus PagingRequest::copy_entries(PagingCommandWriter &writer)
{
    AV_SCOPE("PagingRequest::copy_entries");
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
    AV_SCOPE("PagingRequest::write_copy");
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
    AV_SCOPE("PagingRequest::write_range");
    const uint64_t source_end = uint64_t{range.SrcStartPteIndex} + range.NumPageTableEntries;
    const uint64_t destination_end = uint64_t{range.DstStartPteIndex} + range.NumPageTableEntries;
    if (source_end > GpuMmuGeometry::entries_per_table || destination_end > GpuMmuGeometry::entries_per_table)
        return Status::invalid_parameter;
    return writer.copy_range(range.SrcPageTableAddress, range.DstPageTableAddress, range.SrcStartPteIndex,
                             range.DstStartPteIndex, range.NumPageTableEntries);
}

NtStatus PagingRequest::finish(Status status, size_t length)
{
    AV_SCOPE("PagingRequest::finish");
    if (request_.pDmaBuffer == nullptr) return NtStatus(Status::invalid_parameter);
    request_.pDmaBuffer = static_cast<uint8_t *>(request_.pDmaBuffer) + length;
    return status == Status::ok ? NtStatus(Status::ok) : NtStatus(insufficient_dma_buffer);
}

}  // namespace av
