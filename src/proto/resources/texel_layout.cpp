#include "proto/resources/texel_layout.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

namespace {

constexpr uint32_t word_bytes = 4u;

}

TexelLayout::TexelLayout() : bytes_(0u), width_(1u), height_(1u), planar_(0u)
{
    AV_SCOPE("TexelLayout::TexelLayout");
}

TexelLayout::TexelLayout(uint32_t bytes, uint32_t width, uint32_t height)
    : bytes_(bytes), width_(width == 0u ? 1u : width), height_(height == 0u ? 1u : height), planar_(0u)
{
    AV_SCOPE("TexelLayout::TexelLayout");
}

void TexelLayout::lay_out(uint32_t bytes, uint32_t width, uint32_t height)
{
    AV_SCOPE("TexelLayout::lay_out");
    bytes_ = bytes;
    width_ = width;
    height_ = height;
    planar_ = 0u;
}

void TexelLayout::lay_out_planar(uint32_t sample_bytes)
{
    AV_SCOPE("TexelLayout::lay_out_planar");
    bytes_ = 0u;
    width_ = 1u;
    height_ = 1u;
    planar_ = sample_bytes;
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
    if (planar_ != 0u) return width * planar_;
    return (width + width_ - 1u) / width_ * bytes_;
}

uint32_t TexelLayout::rows(uint32_t height) const
{
    AV_SCOPE("TexelLayout::rows");
    if (planar_ != 0u) return height + (height + 1u) / 2u;
    return (height + height_ - 1u) / height_;
}

uint64_t TexelLayout::offset_of(uint32_t x, uint32_t y, uint32_t pitch) const
{
    AV_SCOPE("TexelLayout::offset_of");
    return uint64_t{y / height_} * pitch + uint64_t{x / width_} * bytes_;
}

uint32_t TexelLayout::copied_row_bytes(uint32_t width) const
{
    AV_SCOPE("TexelLayout::copied_row_bytes");
    return bytes_ == 0u && planar_ == 0u ? width : row_bytes(width);
}

bool TexelLayout::planar() const
{
    AV_SCOPE("TexelLayout::planar");
    return planar_ != 0u;
}

bool TexelLayout::word_texels() const
{
    AV_SCOPE("TexelLayout::word_texels");
    return bytes_ == word_bytes && width_ == 1u;
}

}
