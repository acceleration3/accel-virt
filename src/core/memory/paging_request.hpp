/* One DxgkDdiBuildPagingBuffer call. An aperture segment over guest memory
 * needs no paging commands, but mapping and unmapping one is where the pages
 * an allocation really lives in are recorded: an aperture address means
 * nothing to a device that reads guest physical memory. Discarding an
 * allocation's content changes nothing the device reads, and neither does
 * residency. Page
 * table updates go straight into the table through the CPU mapping VidMm
 * passes. TLB flushes, virtual fills and transfers and page table copies are
 * commands for the device (docs/protocol.md). VidMm issues the physical FILL
 * and TRANSFER only to WDDMv1 drivers, so every other operation is refused. */
#ifndef AV_PAGING_REQUEST_HPP
#define AV_PAGING_REQUEST_HPP

#include "core/platform/platform.hpp"
#include "core/support/nt_status.hpp"
#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class AllocationTable;
class LogBudget;
class PagingCommandWriter;
struct DxgkargBuildPagingBufferRecord;
struct DxgkBuildPagingBufferCopyRangeRecord;

class PagingRequest {
public:
    PagingRequest(Platform &platform, AllocationTable &allocations, LogBudget &budget,
                  DxgkargBuildPagingBufferRecord &request);
    PagingRequest(const PagingRequest &) = delete;
    PagingRequest &operator=(const PagingRequest &) = delete;

    /* Advances pDmaBuffer past the command it wrote. A command that does not
     * fit answers STATUS_GRAPHICS_INSUFFICIENT_DMA_BUFFER; for a page table
     * copy MultipassOffset then names the first range left. */
    NtStatus build();

private:
    enum : int32_t {   /* DXGK_BUILDPAGINGBUFFER_OPERATION */
        discard_content = 2,
        map_aperture_segment = 5,
        unmap_aperture_segment = 6,
        virtual_transfer = 8,
        virtual_fill = 9,
        update_page_table = 11,
        flush_tlb = 12,
        copy_page_table_entries = 14,
        notify_residency = 15,
    };
    static constexpr uint32_t insufficient_dma_buffer = 0xC01E0001u;   /* STATUS_GRAPHICS_INSUFFICIENT_DMA_BUFFER */

    /* Aperture placement is recorded, never refused: dxgkrnl has no answer for
     * a driver that will not map a segment it reported, so a mapping we cannot
     * record is logged and the allocation is left with no pages, which shows as
     * a source that never scans out rather than as a failed paging operation. */
    [[nodiscard]] Status map_aperture();
    [[nodiscard]] Status unmap_aperture();
    void                 log_aperture(const char *operation, uint32_t segment, uint64_t offset, uint64_t pages,
                                      Status status);
    /* An operation this phase does not implement, named so a guest that needs
     * one says which (constitution IX). */
    NtStatus             refuse();
    /* The operation's record, copied out of the union dxgkrnl fills. */
    template <typename Operation> void read_operation(Operation &operation) const;
    [[nodiscard]] Status update_table() const;
    [[nodiscard]] Status write_flush(PagingCommandWriter &writer) const;
    [[nodiscard]] Status write_fill(PagingCommandWriter &writer) const;
    [[nodiscard]] Status write_transfer(PagingCommandWriter &writer) const;
    NtStatus             copy_entries(PagingCommandWriter &writer);
    /* too_large when ranges remain; `next` is the first of them. */
    [[nodiscard]] Status write_copy(PagingCommandWriter &writer, uint32_t &next) const;
    [[nodiscard]] Status write_range(PagingCommandWriter &writer, const DxgkBuildPagingBufferCopyRangeRecord &range) const;
    /* Commits `length` bytes of a command written with `status`. */
    NtStatus             finish(Status status, size_t length);

    Platform                       &platform_;
    AllocationTable                &allocations_;
    LogBudget                      &budget_;
    DxgkargBuildPagingBufferRecord &request_;
};

}  // namespace av

#endif  // AV_PAGING_REQUEST_HPP
