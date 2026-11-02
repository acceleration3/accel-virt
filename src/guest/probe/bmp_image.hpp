#ifndef AV_BMP_IMAGE_HPP
#define AV_BMP_IMAGE_HPP

#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

struct BmpImageProbe;

class BmpImage {
public:
    static constexpr size_t   header_bytes = 54u;
    static constexpr uint32_t max_extent = 4096u;

    enum class Rows : uint8_t { top_first, bottom_first };
    enum class Order : uint8_t { red_first, blue_first };

    BmpImage(const uint8_t *pixels, uint32_t width, uint32_t height, Rows rows, Order order = Order::red_first);

    size_t               encoded_size() const;
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

}

#endif
