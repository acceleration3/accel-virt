/* A D3DDDIFORMAT the driver can lay out. Only 32-bpp formats until a mode
 * needs another. */
#ifndef AV_PIXEL_FORMAT_HPP
#define AV_PIXEL_FORMAT_HPP

#include <stdint.h>

namespace av {

class PixelFormat {
public:
    explicit PixelFormat(uint32_t format);

    /* Zero for a format the driver does not lay out. */
    uint32_t bytes_per_pixel() const;

private:
    enum : uint32_t { a8r8g8b8 = 21u, x8r8g8b8 = 22u, a8b8g8r8 = 32u, x8b8g8r8 = 33u };

    uint32_t format_;
};

}  // namespace av

#endif  // AV_PIXEL_FORMAT_HPP
