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

    bool     set(Field field, int32_t value);
    int32_t  get(Field field) const;
    size_t   span(uint32_t format, uint32_t type, int32_t width, int32_t height, int32_t depth) const;
    size_t   bitmap_span(int32_t width, int32_t height) const;
    bool     layout(uint32_t format, uint32_t type, int32_t width, int32_t height, uint64_t &start, uint64_t &row_bytes,
                    uint64_t &stride) const;

    bool invariants_hold() const;

private:
    static constexpr uint32_t fields = 6u;

    size_t components(uint32_t format) const;
    size_t packed_bytes(uint32_t type) const;
    size_t value_bytes(uint32_t type) const;
    bool   packed_fits(uint32_t format, uint32_t type) const;

    int32_t values_[fields];

    friend struct PixelStoreProbe;
};

}

#endif
