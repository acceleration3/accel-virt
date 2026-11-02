#ifndef AV_PRESENT_BLIT_HPP
#define AV_PRESENT_BLIT_HPP

#include "core/platform/platform.hpp"
#include "miniport/present/present_area.hpp"
#include "miniport/present/present_surface.hpp"
#include "core/support/status.hpp"
#include "proto/dma/present_region.hpp"

#include <stdint.h>

namespace av {

class PixelMover;
class Rectangle;
struct RectRecord;
struct PresentBlitProbe;

class PresentBlit {
public:
    static constexpr uint32_t batch = 64u;

    PresentBlit(Platform &platform, PixelMover &mover, const PresentSurface *source,
                const PresentSurface &destination);
    PresentBlit(const PresentBlit &) = delete;
    PresentBlit &operator=(const PresentBlit &) = delete;

    [[nodiscard]] Status blit(const Rectangle &source, const Rectangle &destination, const RectRecord *sub_rectangles,
                              uint32_t count);
    [[nodiscard]] Status fill(uint32_t colour, const Rectangle &destination, const RectRecord *sub_rectangles,
                              uint32_t count);

    uint32_t rows() const;

    bool invariants_hold() const;

private:
    [[nodiscard]] Status gather(const PresentArea &source, const PresentArea &destination,
                                const RectRecord *sub_rectangles, uint32_t count);
    [[nodiscard]] Status add(const PresentArea &source, const PresentArea &destination);
    [[nodiscard]] Status flush();
    bool                 reachable() const;

    Platform             &platform_;
    PixelMover           &mover_;
    const PresentSurface *source_;
    const PresentSurface &destination_;
    bool                  filling_;
    uint32_t              colour_;
    PresentRegion         regions_[batch];
    uint32_t              held_;
    uint32_t              held_rows_;
    uint32_t              rows_;

    friend struct PresentBlitProbe;
};

}

#endif
