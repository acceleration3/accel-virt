#include "guest/suite/bmp_image.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

namespace {

/* "BM", the info header's size, one plane, 24 bits a pixel, and 2835 pixels
 * a metre (72 dpi), which viewers ignore but some tools want non-zero. */
constexpr uint32_t signature = 0x4D42u;
constexpr uint32_t info_bytes = 40u;
constexpr uint32_t planes = 1u;
constexpr uint32_t bits_per_pixel = 24u;
constexpr uint32_t pixels_per_metre = 2835u;

}  // namespace

BmpImage::BmpImage(const uint8_t *rgba, uint32_t width, uint32_t height, Rows rows)
    : rgba_(rgba), width_(width), height_(height), rows_(rows)
{
    AV_SCOPE("BmpImage::BmpImage");
}

size_t BmpImage::encoded_size() const
{
    AV_SCOPE("BmpImage::encoded_size");
    if (rgba_ == nullptr || width_ == 0u || height_ == 0u || width_ > max_extent || height_ > max_extent) return 0u;
    return header_bytes + static_cast<size_t>(row_bytes()) * height_;
}

Status BmpImage::encode(uint8_t *destination, size_t capacity) const
{
    AV_SCOPE("BmpImage::encode");
    const size_t size = encoded_size();
    if (size == 0u || destination == nullptr) return Status::invalid_parameter;
    if (capacity < size) return Status::no_space;
    for (size_t byte = 0u; byte < header_bytes; byte++) destination[byte] = 0u;
    put(destination, 0u, signature, 2u);
    put(destination, 2u, static_cast<uint32_t>(size), 4u);
    put(destination, 10u, static_cast<uint32_t>(header_bytes), 4u);
    put(destination, 14u, info_bytes, 4u);
    put(destination, 18u, width_, 4u);
    put(destination, 22u, height_, 4u);
    put(destination, 26u, planes, 2u);
    put(destination, 28u, bits_per_pixel, 2u);
    put(destination, 34u, static_cast<uint32_t>(size - header_bytes), 4u);
    put(destination, 38u, pixels_per_metre, 4u);
    put(destination, 42u, pixels_per_metre, 4u);
    /* A positive height is a bottom-up file: its first stored row is the
     * image's bottom one. */
    for (uint32_t stored = 0u; stored < height_; stored++) {
        const uint32_t source_row = rows_ == Rows::bottom_first ? stored : height_ - 1u - stored;
        const size_t   out = header_bytes + static_cast<size_t>(stored) * row_bytes();
        const size_t   in = static_cast<size_t>(source_row) * width_ * rgba_bytes;
        for (uint32_t column = 0u; column < width_; column++) {
            const size_t pixel = in + static_cast<size_t>(column) * rgba_bytes;
            const size_t at = out + static_cast<size_t>(column) * rgb_bytes;
            destination[at] = rgba_[pixel + 2u];
            destination[at + 1u] = rgba_[pixel + 1u];
            destination[at + 2u] = rgba_[pixel];
        }
        for (size_t pad = static_cast<size_t>(width_) * rgb_bytes; pad < row_bytes(); pad++) destination[out + pad] = 0u;
    }
    return Status::ok;
}

bool BmpImage::invariants_hold() const
{
    AV_SCOPE("BmpImage::invariants_hold");
    return rows_ == Rows::top_first || rows_ == Rows::bottom_first;
}

uint32_t BmpImage::row_bytes() const
{
    AV_SCOPE("BmpImage::row_bytes");
    const uint32_t used = width_ * rgb_bytes;
    return (used + row_multiple - 1u) / row_multiple * row_multiple;
}

void BmpImage::put(uint8_t *destination, size_t offset, uint32_t value, uint32_t bytes) const
{
    AV_SCOPE("BmpImage::put");
    for (uint32_t byte = 0u; byte < bytes; byte++) destination[offset + byte] = static_cast<uint8_t>(value >> (8u * byte));
}

}  // namespace av
