/* One surface a present moves pixels to or from (feature 046): its layout and
 * the guest pages its bytes are in. The two always travel together - a layout
 * without pages cannot be read, and pages without a layout say nothing about
 * where a pixel is - so they are one object, and it is what PresentBlit takes.
 * The pages belong to the allocation and must outlive this. */
#ifndef AV_PRESENT_SURFACE_HPP
#define AV_PRESENT_SURFACE_HPP

#include "proto/resources/guest_page_list.hpp"
#include "proto/resources/surface_layout.hpp"

#include <stdint.h>

namespace av {

class PresentSurface {
public:
    /* Four bytes a pixel: every surface this driver presents (A8R8G8B8). */
    static constexpr uint32_t pixel_bytes = 4u;

    PresentSurface(const SurfaceLayout &layout, const GuestPageList &pages);

    const SurfaceLayout &layout() const;
    /* The bytes at `offset`, up to the end of their page, and how many there
     * are; the frame is 0 when the offset is past the pages. */
    uint64_t             frame_at(uint64_t offset, uint32_t &within) const;
    /* A surface of four-byte pixels, with a pitch wide enough for its own
     * width, every page of it mapped, and room for every row. */
    bool                 reachable() const;

private:
    SurfaceLayout layout_;
    GuestPageList pages_;
};

}  // namespace av

#endif  // AV_PRESENT_SURFACE_HPP
