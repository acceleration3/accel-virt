/* How an exported surface's memory holds its picture, as a consumer imports
 * it (docs/external-export.md): the picture's size, the DRM format of its
 * pixels, where its rows are, and the DRM format modifier - implicit, for a
 * layout the driver keeps with the memory itself. */
#ifndef AV_EXPORTED_LAYOUT_HPP
#define AV_EXPORTED_LAYOUT_HPP

#include <stdint.h>

namespace av {

class ExportedLayout {
public:
    ExportedLayout();
    ExportedLayout(uint32_t width, uint32_t height, uint32_t fourcc, uint32_t stride, uint32_t offset,
                   uint64_t modifier);

    uint32_t width() const;
    uint32_t height() const;
    uint32_t fourcc() const;
    /* Bytes from one row to the next, at `offset` bytes into the memory. */
    uint32_t stride() const;
    uint32_t offset() const;
    uint64_t modifier() const;
    /* A picture with a size, a format and rows: what an export answers. */
    bool     describes_a_picture() const;
    /* Nothing at all: what every other answer carries. */
    bool     empty() const;

private:
    uint32_t width_;
    uint32_t height_;
    uint32_t fourcc_;
    uint32_t stride_;
    uint32_t offset_;
    uint64_t modifier_;
};

}  // namespace av

#endif  // AV_EXPORTED_LAYOUT_HPP
