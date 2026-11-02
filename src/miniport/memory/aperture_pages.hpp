#ifndef AV_APERTURE_PAGES_HPP
#define AV_APERTURE_PAGES_HPP

#include "core/platform/platform.hpp"
#include "core/support/status.hpp"
#include "proto/resources/guest_page_list.hpp"

#include <stdint.h>

namespace av {

struct AperturePagesProbe;
struct DxgkBuildPagingBufferMapApertureSegmentRecord;
struct DxgkBuildPagingBufferUnmapApertureSegmentRecord;

class AperturePages {
public:
    AperturePages(Platform &platform, uint32_t capacity);
    ~AperturePages();
    AperturePages(const AperturePages &) = delete;
    AperturePages &operator=(const AperturePages &) = delete;

    [[nodiscard]] Status map(const DxgkBuildPagingBufferMapApertureSegmentRecord &request);
    void                 unmap(const DxgkBuildPagingBufferUnmapApertureSegmentRecord &request);
    GuestPageList        pages(uint64_t first_page, uint32_t count) const;
    GuestPageList        whole() const;

    bool invariants_hold() const;

private:
    bool     holds(uint64_t first_page, uint32_t count) const;
    void     forget_all();

    Platform &platform_;
    uint32_t  capacity_;
    uint64_t *frames_;
    uint64_t  base_page_;

    friend struct AperturePagesProbe;
};

}

#endif
