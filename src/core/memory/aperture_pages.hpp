/* The guest pages one allocation is mapped into the aperture segment with.
 *
 * An aperture segment address tells the device nothing: the host reads guest
 * physical memory, so the frames dxgkrnl hands to
 * DXGK_OPERATION_MAP_APERTURE_SEGMENT are the only description of where an
 * allocation's pixels are. They are kept per allocation, indexed by segment
 * page, so a primary's address can be turned back into the pages
 * RESOURCE_ATTACH_BACKING must carry. */
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
    /* `capacity` is the allocation's size in pages, the most the aperture can
     * ever hold of it. */
    AperturePages(Platform &platform, uint32_t capacity);
    ~AperturePages();
    AperturePages(const AperturePages &) = delete;
    AperturePages &operator=(const AperturePages &) = delete;

    /* The frames the operation's MDL names, for the segment pages it names.
     * A mapping that lands outside the window already recorded replaces it:
     * VidMm maps an allocation where it finds room, and only the current
     * placement is worth remembering. */
    [[nodiscard]] Status map(const DxgkBuildPagingBufferMapApertureSegmentRecord &request);
    /* Forgets the frames of the segment pages the operation names. */
    void                 unmap(const DxgkBuildPagingBufferUnmapApertureSegmentRecord &request);
    /* The `count` pages from segment page `first_page`; an empty list unless
     * every one of them was mapped. Dispatch at most: the DPC asks before it
     * attaches a primary. */
    GuestPageList        pages(uint64_t first_page, uint32_t count) const;
    /* Every page of the allocation, from wherever the window starts; empty
     * unless each one is mapped. Dispatch at most. */
    GuestPageList        whole() const;

    bool invariants_hold() const;

private:
    /* Whether [first_page, first_page + count) lies in the recorded window. */
    bool     holds(uint64_t first_page, uint32_t count) const;
    void     forget_all();

    Platform &platform_;
    uint32_t  capacity_;
    uint64_t *frames_;
    /* The segment page frames_[0] describes; meaningless without frames_. */
    uint64_t  base_page_;

    friend struct AperturePagesProbe;
};

}  // namespace av

#endif  // AV_APERTURE_PAGES_HPP
