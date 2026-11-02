#ifndef AV_CURSOR_IMAGE_HPP
#define AV_CURSOR_IMAGE_HPP

#include "core/platform/platform.hpp"
#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

struct DxgkargSetPointerShapeRecord;

class CursorImage {
public:
    static constexpr uint32_t side = 64u;
    static constexpr size_t   pixel_bytes = side * side * 4u;

    CursorImage(Platform &platform, const DxgkargSetPointerShapeRecord &shape);

    [[nodiscard]] Status draw(uint8_t *pixels) const;

private:
    enum : uint32_t { flag_monochrome = 1u, flag_color = 2u, flag_masked_color = 4u };

    [[nodiscard]] Status validate() const;
    void                 draw_monochrome(uint8_t *pixels) const;
    void                 draw_color(uint8_t *pixels) const;

    Platform                           &platform_;
    const DxgkargSetPointerShapeRecord &shape_;
};

}

#endif
