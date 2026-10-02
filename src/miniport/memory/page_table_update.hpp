/* One DXGK_OPERATION_UPDATE_PAGE_TABLE: VidMm's entries written straight into
 * the page table through the CPU mapping it passes
 * (DXGK_PAGETABLEUPDATE_CPU_VIRTUAL), so the paging buffer needs no bytes. */
#ifndef AV_PAGE_TABLE_UPDATE_HPP
#define AV_PAGE_TABLE_UPDATE_HPP

#include "core/platform/platform.hpp"
#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

struct DxgkBuildPagingBufferUpdatePageTableRecord;

class PageTableUpdate {
public:
    PageTableUpdate(Platform &platform, const DxgkBuildPagingBufferUpdatePageTableRecord &update);
    PageTableUpdate(const PageTableUpdate &) = delete;
    PageTableUpdate &operator=(const PageTableUpdate &) = delete;

    /* invalid_parameter, writing nothing, for an update outside one table. */
    [[nodiscard]] Status write() const;

private:
    static constexpr uint32_t flag_repeat = 0x1u;   /* DXGK_UPDATEPAGETABLEFLAGS */
    static constexpr int32_t  update_through_cpu = 0;

    Platform                                         &platform_;
    const DxgkBuildPagingBufferUpdatePageTableRecord &update_;
};

}  // namespace av

#endif  // AV_PAGE_TABLE_UPDATE_HPP
