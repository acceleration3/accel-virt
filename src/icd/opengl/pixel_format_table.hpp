/* The pixel formats the ICD offers (feature 050, research.md §7): RGBA8 with
 * depth/stencil none, D24S8 or D32, single or double buffered, sRGB or not,
 * with 0 or 4 samples - 24 rows, fixed when the ICD is built, so an index names
 * the same format for the process's life (US2.1). Each is drawable to a window
 * and composited; none is a palette or bitmap format, so a memory DC finds
 * none. A row is written as the 40 bytes of a PIXELFORMATDESCRIPTOR, whose
 * layout the Windows shim proves. */
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
    /* Whether `index` (1-based, as WGL counts) names a row. */
    bool     contains(int32_t index) const;
    /* The row `index` names; only for an index contains() accepts. */
    const PixelFormat &format(int32_t index) const;
    /* count() for any index; the descriptor of row `index` written to
     * `descriptor` only for an index in range, a non-null descriptor and
     * `bytes` of at least descriptor_bytes, and nothing written otherwise. */
    int32_t describe(int32_t index, uint32_t bytes, uint8_t *descriptor) const;

    bool invariants_hold() const;

private:
    static constexpr uint32_t row_count = 24u;

    PixelFormat rows_[row_count];

    friend struct PixelFormatTableProbe;
};

}  // namespace av::glicd

#endif  // AV_GLICD_PIXEL_FORMAT_TABLE_HPP
