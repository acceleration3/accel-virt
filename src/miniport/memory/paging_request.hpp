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

    NtStatus build();

private:
    enum : int32_t {
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
    static constexpr uint32_t insufficient_dma_buffer = 0xC01E0001u;

    [[nodiscard]] Status map_aperture();
    [[nodiscard]] Status unmap_aperture();
    void                 log_aperture(const char *operation, uint32_t segment, uint64_t offset, uint64_t pages,
                                      Status status);
    NtStatus             refuse();
    template <typename Operation> void read_operation(Operation &operation) const;
    [[nodiscard]] Status update_table() const;
    [[nodiscard]] Status write_flush(PagingCommandWriter &writer) const;
    [[nodiscard]] Status write_fill(PagingCommandWriter &writer) const;
    [[nodiscard]] Status write_transfer(PagingCommandWriter &writer) const;
    NtStatus             copy_entries(PagingCommandWriter &writer);
    [[nodiscard]] Status write_copy(PagingCommandWriter &writer, uint32_t &next) const;
    [[nodiscard]] Status write_range(PagingCommandWriter &writer, const DxgkBuildPagingBufferCopyRangeRecord &range) const;
    NtStatus             finish(Status status, size_t length);

    Platform                       &platform_;
    AllocationTable                &allocations_;
    LogBudget                      &budget_;
    DxgkargBuildPagingBufferRecord &request_;
};

}

#endif
