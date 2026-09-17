/* A render check's pixels as a Windows bitmap, so a person can look at what a
 * probe drew: 8-bit four-channel rows in, a 24-bit BMP (BITMAPINFOHEADER,
 * bottom-up rows padded to four bytes, BGR) out. Alpha is dropped. A render
 * read back from a Vulkan image is RGBA; one read back from a window is the
 * BGRA a DIB holds (feature 046), so the order is the caller's to say. */
#ifndef AV_BMP_IMAGE_HPP
#define AV_BMP_IMAGE_HPP

#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

struct BmpImageProbe;

class BmpImage {
public:
    /* The file header and the info header. */
    static constexpr size_t   header_bytes = 54u;
    static constexpr uint32_t max_extent = 4096u;

    /* Where the first row of `rgba` is: GL reads back from the bottom,
     * Vulkan copies from the top. */
    enum class Rows : uint8_t { top_first, bottom_first };
    /* Which of the first three bytes of a pixel is red. */
    enum class Order : uint8_t { red_first, blue_first };

    BmpImage(const uint8_t *pixels, uint32_t width, uint32_t height, Rows rows, Order order = Order::red_first);

    /* The file's size; 0 for no pixels, an empty side or one past max_extent. */
    size_t               encoded_size() const;
    /* invalid_parameter for an image encoded_size refuses or no destination;
     * no_space, writing nothing, when `capacity` is short of the file. */
    [[nodiscard]] Status encode(uint8_t *destination, size_t capacity) const;

    bool invariants_hold() const;

private:
    enum : uint32_t { rgb_bytes = 3u, rgba_bytes = 4u, row_multiple = 4u };

    uint32_t row_bytes() const;
    void     put(uint8_t *destination, size_t offset, uint32_t value, uint32_t bytes) const;

    const uint8_t *pixels_;
    uint32_t       width_;
    uint32_t       height_;
    Rows           rows_;
    Order          order_;

    friend struct BmpImageProbe;
};

}  // namespace av

#endif  // AV_BMP_IMAGE_HPP
