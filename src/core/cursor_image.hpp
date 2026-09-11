/* One DxgkDdiSetPointerShape drawn as the 64x64 B8G8R8A8 image the virtio-gpu
 * cursor shows. Rows are top-down; what a cursor plane cannot do - inverting
 * the screen - is shown as the nearest opaque pixel. Everything beyond the
 * shape is transparent. */
#ifndef AV_CURSOR_IMAGE_HPP
#define AV_CURSOR_IMAGE_HPP

#include "platform.hpp"
#include "status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

struct DxgkargSetPointerShapeRecord;

class CursorImage {
public:
    static constexpr uint32_t side = 64u;
    static constexpr size_t   pixel_bytes = side * side * 4u;

    CursorImage(Platform &platform, const DxgkargSetPointerShapeRecord &shape);

    /* Writes pixel_bytes; invalid_parameter, with nothing written, for a shape
     * the adapter cannot show. */
    [[nodiscard]] Status draw(uint8_t *pixels) const;

private:
    enum : uint32_t { flag_monochrome = 1u, flag_color = 2u, flag_masked_color = 4u };   /* DXGK_POINTERFLAGS */

    [[nodiscard]] Status validate() const;
    void                 draw_monochrome(uint8_t *pixels) const;
    void                 draw_color(uint8_t *pixels) const;

    Platform                           &platform_;
    const DxgkargSetPointerShapeRecord &shape_;
};

}  // namespace av

#endif  // AV_CURSOR_IMAGE_HPP
