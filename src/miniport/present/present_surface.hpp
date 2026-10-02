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

class Platform;
class PresentCopyRequest;
class WireWriter;

class PresentSurface {
public:
    /* Four bytes a pixel: every surface this driver presents (A8R8G8B8). */
    static constexpr uint32_t pixel_bytes = 4u;

    PresentSurface(const SurfaceLayout &layout, const GuestPageList &pages);

    const SurfaceLayout &layout() const;
    /* Every page of the surface mapped as one view (Platform::map_pages); null
     * when the platform cannot map them. */
    uint8_t             *map(Platform &platform, void *&mapping) const;
    /* A surface of four-byte pixels, with a pitch of whole pixels wide enough
     * for its own width, every page of it mapped, and room for every row. */
    bool                 reachable() const;
    /* Whether red and blue trade places between this surface's bytes and
     * `other`'s: an application's R8G8B8A8 back buffer into the desktop's
     * A8R8G8B8. */
    bool                 swaps_with(const PresentSurface &other) const;
    /* The pages as the device is told them (GuestPageList::write_runs). */
    uint32_t             runs() const;
    void                 write_runs(WireWriter &writer) const;
    /* What the device is told of a blt of `regions` regions from this surface
     * into `destination`, and of a fill of `colour` over `regions` of this one
     * (PresentCopyRequest). */
    PresentCopyRequest   blt_into(const PresentSurface &destination, uint32_t regions) const;
    PresentCopyRequest   fill_with(uint32_t colour, uint32_t regions) const;

private:
    SurfaceLayout layout_;
    GuestPageList pages_;
};

}  // namespace av

#endif  // AV_PRESENT_SURFACE_HPP
