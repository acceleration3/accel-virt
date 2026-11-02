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
    static constexpr uint32_t pixel_bytes = 4u;

    PresentSurface(const SurfaceLayout &layout, const GuestPageList &pages);

    const SurfaceLayout &layout() const;
    uint8_t             *map(Platform &platform, void *&mapping) const;
    bool                 reachable() const;
    bool                 swaps_with(const PresentSurface &other) const;
    uint32_t             runs() const;
    void                 write_runs(WireWriter &writer) const;
    PresentCopyRequest   blt_into(const PresentSurface &destination, uint32_t regions) const;
    PresentCopyRequest   fill_with(uint32_t colour, uint32_t regions) const;

private:
    SurfaceLayout layout_;
    GuestPageList pages_;
};

}

#endif
