#ifndef AV_GLICD_PIXEL_FORMAT_TABLE_HPP
#define AV_GLICD_PIXEL_FORMAT_TABLE_HPP

#include "icd/opengl/pixel_format.hpp"

#include <stdint.h>

namespace av::glicd {

struct PixelFormatTableProbe;

class PixelFormatTable {
public:
    static constexpr uint32_t descriptor_bytes = 40u;


    PixelFormatTable();
    PixelFormatTable(const PixelFormatTable &) = delete;
    PixelFormatTable &operator=(const PixelFormatTable &) = delete;

    uint32_t count() const;
    bool     contains(int32_t index) const;
    const PixelFormat &format(int32_t index) const;
    int32_t describe(int32_t index, uint32_t bytes, uint8_t *descriptor) const;

    bool invariants_hold() const;

private:
    static constexpr uint32_t row_count = 24u;

    PixelFormat rows_[row_count];

    friend struct PixelFormatTableProbe;
};

}

#endif
