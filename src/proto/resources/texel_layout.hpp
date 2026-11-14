#ifndef AV_TEXEL_LAYOUT_HPP
#define AV_TEXEL_LAYOUT_HPP

#include <stdint.h>

namespace av {

class TexelLayout {
public:
    TexelLayout(uint32_t bytes, uint32_t width, uint32_t height);
    TexelLayout(const TexelLayout &) = default;
    TexelLayout &operator=(const TexelLayout &) = default;
    ~TexelLayout() = default;

    uint32_t element_bytes() const;
    uint32_t element_width() const;
    uint32_t element_height() const;
    uint32_t row_bytes(uint32_t width) const;
    uint32_t rows(uint32_t height) const;
    uint64_t offset_of(uint32_t x, uint32_t y, uint32_t pitch) const;
    uint32_t copied_row_bytes(uint32_t width) const;
    bool     word_texels() const;
    bool     planar() const;

protected:
    TexelLayout();

    void lay_out(uint32_t bytes, uint32_t width, uint32_t height);
    void lay_out_planar(uint32_t sample_bytes);

private:
    uint32_t bytes_;
    uint32_t width_;
    uint32_t height_;
    uint32_t planar_;
};

}

#endif
