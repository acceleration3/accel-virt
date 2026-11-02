#ifndef AV_MAPPED_PIXELS_HPP
#define AV_MAPPED_PIXELS_HPP

#include "miniport/present/pixel_mover.hpp"
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

    [[nodiscard]] Status copy(const PresentSurface &source, const PresentSurface &destination,
                              const PresentRegion *regions, uint32_t count) override;
    [[nodiscard]] Status fill(const PresentSurface &destination, uint32_t colour, const PresentRegion *regions,
                              uint32_t count) override;

private:
    class ByteOrder {
    public:
        ByteOrder(const PresentSurface &source, const PresentSurface &destination);

        void copy_pixels(const uint8_t *from, uint8_t *to, uint32_t count) const;

    private:
        void copy_pixel(const uint8_t *from, uint8_t *to) const;

        const uint8_t *order_;
    };

    Platform &platform_;
};

}

#endif
