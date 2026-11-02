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

    [[nodiscard]] Status write() const;

private:
    static constexpr uint32_t flag_repeat = 0x1u;
    static constexpr int32_t  update_through_cpu = 0;

    Platform                                         &platform_;
    const DxgkBuildPagingBufferUpdatePageTableRecord &update_;
};

}

#endif
