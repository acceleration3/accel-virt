#include "proto/d3d/texel_layout.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

namespace {

constexpr uint32_t word_bytes = 4u;

}  // namespace

TexelLayout::TexelLayout() : bytes_(0u), width_(1u), height_(1u)
{
    AV_SCOPE("TexelLayout::TexelLayout");
}

void TexelLayout::lay_out(uint32_t bytes, uint32_t width, uint32_t height)
{
    AV_SCOPE("TexelLayout::lay_out");
    bytes_ = bytes;
    width_ = width;
    height_ = height;
}

uint32_t TexelLayout::element_bytes() const
{
    AV_SCOPE("TexelLayout::element_bytes");
    return bytes_;
}

uint32_t TexelLayout::element_width() const
{
    AV_SCOPE("TexelLayout::element_width");
    return width_;
}

uint32_t TexelLayout::element_height() const
{
    AV_SCOPE("TexelLayout::element_height");
    return height_;
}

uint32_t TexelLayout::row_bytes(uint32_t width) const
{
    AV_SCOPE("TexelLayout::row_bytes");
    return (width + width_ - 1u) / width_ * bytes_;
}

uint32_t TexelLayout::rows(uint32_t height) const
{
    AV_SCOPE("TexelLayout::rows");
    return (height + height_ - 1u) / height_;
}

uint32_t TexelLayout::copied_row_bytes(uint32_t width) const
{
    AV_SCOPE("TexelLayout::copied_row_bytes");
    return bytes_ == 0u ? width : row_bytes(width);
}

bool TexelLayout::word_texels() const
{
    AV_SCOPE("TexelLayout::word_texels");
    return bytes_ == word_bytes && width_ == 1u;
}

}  // namespace av
