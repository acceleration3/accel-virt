/* The pixels a present moves (feature 046). The DMA command a present writes
 * is executed on the host only for a context of the av protocol; dxgkrnl's
 * own presents - the desktop's - and an application's blt-model present into
 * its window's redirection surface are loopback-completed, so nothing would
 * ever move them. This clips each rectangle to both surfaces and to the
 * destination rectangle, and hands what is left, as regions, to whatever
 * moves the pixels (PixelMover).
 *
 * One responsibility: turning a present's rectangles into regions inside both
 * surfaces of the same pixel size. Scaling is refused - the desktop presents
 * one to one - and a rectangle that clips away is not a failure: nothing of it
 * was on screen. */
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
    /* Regions handed over at once: a present with more sub-rectangles is
     * moved in as many batches. */
    static constexpr uint32_t batch = 64u;

    /* `destination` is written, `source` only read; a fill needs no source. */
    PresentBlit(Platform &platform, PixelMover &mover, const PresentSurface *source,
                const PresentSurface &destination);
    PresentBlit(const PresentBlit &) = delete;
    PresentBlit &operator=(const PresentBlit &) = delete;

    /* `source` copied to `destination`, one sub-rectangle at a time.
     * not_supported when the two rectangles are of different sizes or either
     * surface is not one this can reach; otherwise what the mover answers. */
    [[nodiscard]] Status blit(const Rectangle &source, const Rectangle &destination, const RectRecord *sub_rectangles,
                              uint32_t count);
    /* `colour` written over `destination`, the same way. */
    [[nodiscard]] Status fill(uint32_t colour, const Rectangle &destination, const RectRecord *sub_rectangles,
                              uint32_t count);

    /* The rows the mover moved: what a test judges, and what says whether a
     * present reached the screen at all. */
    uint32_t rows() const;

    /* Every region it holds is inside the destination surface. */
    bool invariants_hold() const;

private:
    /* Every sub-rectangle of `destination` (or all of it, with none), and the
     * same piece of `source`, clipped and handed over. */
    [[nodiscard]] Status gather(const PresentArea &source, const PresentArea &destination,
                                const RectRecord *sub_rectangles, uint32_t count);
    /* One rectangle of `destination`, from the same size of `source`, clipped
     * to both surfaces; nothing when it clips away. */
    [[nodiscard]] Status add(const PresentArea &source, const PresentArea &destination);
    /* The regions held, to the mover. */
    [[nodiscard]] Status flush();
    /* Whether both surfaces are four-byte ones this can reach. */
    bool                 reachable() const;

    Platform             &platform_;
    PixelMover           &mover_;
    const PresentSurface *source_;
    const PresentSurface &destination_;
    /* A fill has no source to read: its regions are read from the destination. */
    bool                  filling_;
    uint32_t              colour_;
    PresentRegion         regions_[batch];
    uint32_t              held_;
    /* The rows of the regions held, counted once the mover moved them. */
    uint32_t              held_rows_;
    uint32_t              rows_;

    friend struct PresentBlitProbe;
};

}  // namespace av

#endif  // AV_PRESENT_BLIT_HPP
