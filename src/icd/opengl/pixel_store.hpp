/* One direction's pixel store state of an OpenGL context - pack or unpack -
 * and the bytes a transfer under it spans from the application's pointer
 * (OpenGL 4.6 §8.4.3 and §18.2.2): row length, alignment, skipped pixels, rows
 * and images. What the host is told changes nothing here that the host would
 * refuse: a value it would refuse is not kept. */
#ifndef AV_GLICD_PIXEL_STORE_HPP
#define AV_GLICD_PIXEL_STORE_HPP

#include <stddef.h>
#include <stdint.h>

namespace av::glicd {

struct PixelStoreProbe;

class PixelStore {
public:
    enum class Field : uint8_t { alignment, row_length, image_height, skip_pixels, skip_rows, skip_images };

    PixelStore();

    /* false, keeping the old value, for an alignment other than 1, 2, 4 or 8
     * and a negative length or skip. */
    bool     set(Field field, int32_t value);
    int32_t  get(Field field) const;
    /* The bytes from the pointer to the last byte of a `width` x `height` x
     * `depth` transfer of `format` and `type`, skips included; image height
     * and image skips count only when `depth` is above 1. 0 for a non-positive
     * size, a format or type no transfer takes, a packed type with the wrong
     * format, or a span past size_t. */
    size_t   span(uint32_t format, uint32_t type, int32_t width, int32_t height, int32_t depth) const;

    bool invariants_hold() const;

private:
    static constexpr uint32_t fields = 6u;

    /* The values one pixel has in `format`; 0 for no format a transfer takes. */
    size_t components(uint32_t format) const;
    /* A packed type's bytes for one whole pixel; 0 for an unpacked type. */
    size_t packed_bytes(uint32_t type) const;
    /* An unpacked type's bytes per value; 0 for no such type. */
    size_t value_bytes(uint32_t type) const;
    bool   packed_fits(uint32_t format, uint32_t type) const;

    int32_t values_[fields];

    friend struct PixelStoreProbe;
};

}  // namespace av::glicd

#endif  // AV_GLICD_PIXEL_STORE_HPP
