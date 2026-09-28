/* A present's pixels moved by the miniport: each surface's pages mapped once
 * as one view (SurfaceView) and every row of every region copied or filled
 * through it. Every CPU mapping of this adapter's allocations is uncached, so
 * this is what a present costs when the host cannot move it (PresentCopier):
 * thirty milliseconds for a 1280x800 blt. */
#ifndef AV_MAPPED_PIXELS_HPP
#define AV_MAPPED_PIXELS_HPP

#include "core/present/pixel_mover.hpp"
#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class Platform;
class PresentRegion;
class PresentSurface;

class MappedPixels final : public PixelMover {
public:
    static constexpr uint32_t pixel_bytes = 4u;

    explicit MappedPixels(Platform &platform);
    MappedPixels(const MappedPixels &) = delete;
    MappedPixels &operator=(const MappedPixels &) = delete;

    /* not_found when either surface's pages cannot be mapped. */
    [[nodiscard]] Status copy(const PresentSurface &source, const PresentSurface &destination,
                              const PresentRegion *regions, uint32_t count) override;
    [[nodiscard]] Status fill(const PresentSurface &destination, uint32_t colour, const PresentRegion *regions,
                              uint32_t count) override;

private:
    /* The order a pixel's bytes move in between two surfaces: as they are, or
     * with the first and third traded. */
    class ByteOrder {
    public:
        ByteOrder(const PresentSurface &source, const PresentSurface &destination);

        void copy_pixels(const uint8_t *from, uint8_t *to, uint32_t count) const;

    private:
        /* One pixel, as one four-byte load and one store. */
        void copy_pixel(const uint8_t *from, uint8_t *to) const;

        /* A shuffle over sixteen bytes; its first four serve a lone pixel. */
        const uint8_t *order_;
    };

    Platform &platform_;
};

}  // namespace av

#endif  // AV_MAPPED_PIXELS_HPP
