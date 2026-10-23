#include "proto/resources/cross_adapter_layout.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

namespace {

constexpr uint64_t max_u32 = 0xFFFFFFFFull;
constexpr uint64_t page_bytes = 0x1000u;

uint64_t aligned(uint64_t value, uint64_t alignment)
{
    AV_SCOPE("aligned");
    return (value + alignment - 1u) / alignment * alignment;
}

}  // namespace

CrossAdapterLayout::CrossAdapterLayout(uint32_t width, uint32_t height, uint32_t format, uint32_t bytes_per_pixel)
    : width_(width), height_(height), format_(format), pitch_(0u), bytes_(0u), valid_(false)
{
    AV_SCOPE("CrossAdapterLayout::CrossAdapterLayout");
    const bool     heap = format == 0u;
    const uint64_t element = heap ? 1u : bytes_per_pixel;
    if (width == 0u || height == 0u || element == 0u || (heap && height != 1u)) return;
    const uint64_t pitch = aligned(uint64_t{width} * element, pitch_alignment);
    const uint64_t rows = heap ? 1u : aligned(height, height_alignment);
    const uint64_t bytes = aligned(pitch * rows, page_bytes);
    if (pitch > max_u32 || bytes > max_u32) return;
    pitch_ = static_cast<uint32_t>(pitch);
    bytes_ = bytes;
    valid_ = true;
}

bool CrossAdapterLayout::valid() const
{
    AV_SCOPE("CrossAdapterLayout::valid");
    return valid_;
}

uint32_t CrossAdapterLayout::pitch() const
{
    AV_SCOPE("CrossAdapterLayout::pitch");
    return pitch_;
}

uint64_t CrossAdapterLayout::bytes() const
{
    AV_SCOPE("CrossAdapterLayout::bytes");
    return bytes_;
}

SurfaceLayout CrossAdapterLayout::surface() const
{
    AV_SCOPE("CrossAdapterLayout::surface");
    return SurfaceLayout(width_, height_, format_, pitch_);
}

}  // namespace av
