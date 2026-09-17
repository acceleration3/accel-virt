/* The pixels a present moves (feature 046, FR-004). The DMA command a present
 * writes is executed on the host only for a context of the rvk protocol;
 * dxgkrnl's own presents - the desktop's - are loopback-completed, so nothing
 * would ever move them. This moves them here, in the miniport: the source and
 * the destination are allocations whose guest pages the aperture mapping
 * recorded, so their bytes are reachable one page at a time.
 *
 * One responsibility: copying (or filling) rectangles between two surfaces of
 * the same pixel size. Scaling is refused - the desktop presents one to one -
 * and every rectangle is clipped to both surfaces before a byte moves. */
#ifndef AV_PRESENT_BLIT_HPP
#define AV_PRESENT_BLIT_HPP

#include "core/platform/platform.hpp"
#include "core/present/present_surface.hpp"
#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class Rectangle;
struct RectRecord;
struct PresentBlitProbe;

class PresentBlit {
public:
    static constexpr uint32_t pixel_bytes = PresentSurface::pixel_bytes;

    /* `destination` is written, `source` only read; a fill needs no source. */
    PresentBlit(Platform &platform, const PresentSurface *source, const PresentSurface &destination);
    PresentBlit(const PresentBlit &) = delete;
    PresentBlit &operator=(const PresentBlit &) = delete;

    /* `source` copied to `destination`, one sub-rectangle at a time, each
     * clipped to both surfaces and to the destination rectangle. A rectangle
     * that clips away is not a failure: nothing of it was on screen.
     * not_supported when the two rectangles are of different sizes or either
     * surface is not one this can reach. */
    [[nodiscard]] Status blit(const Rectangle &source, const Rectangle &destination, const RectRecord *sub_rectangles,
                              uint32_t count);
    /* `colour` written over `destination`, the same way. */
    [[nodiscard]] Status fill(uint32_t colour, const Rectangle &destination, const RectRecord *sub_rectangles,
                              uint32_t count);

    /* The rows this moved: what a test judges, and what says whether a present
     * reached the screen at all. */
    uint32_t rows() const;

    /* It never writes outside the destination surface: every row it counted is
     * one it clipped first. */
    bool invariants_hold() const;

private:
    /* One row of `bytes` from the source row `from_y` at `from_x` into the
     * destination row `to_y` at `to_x`; a fill writes `colour` instead. */
    [[nodiscard]] Status move_row(uint32_t from_x, uint32_t from_y, uint32_t to_x, uint32_t to_y, uint32_t bytes,
                                  bool filling, uint32_t colour);
    /* The page `offset` of `surface` is in, mapped; `within` is where in it the
     * bytes are and `available` how many of them there are to its end. Null
     * when the page is not mapped; what is unmapped is what this answered. */
    uint8_t             *map_at(const PresentSurface &surface, uint64_t offset, uint32_t &within,
                                uint32_t &available);
    [[nodiscard]] Status move_rectangle(const Rectangle &source, const Rectangle &destination, bool filling,
                                        uint32_t colour);
    /* Whether both surfaces are four-byte ones this can reach. */
    bool                 reachable(bool filling) const;

    Platform             &platform_;
    const PresentSurface *source_;
    const PresentSurface &destination_;
    uint32_t          rows_;
    uint32_t          clipped_rows_;

    friend struct PresentBlitProbe;
};

}  // namespace av

#endif  // AV_PRESENT_BLIT_HPP
