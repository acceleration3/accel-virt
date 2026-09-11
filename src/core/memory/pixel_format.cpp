#include "core/memory/pixel_format.hpp"

namespace av {

PixelFormat::PixelFormat(uint32_t format) : format_(format) {}

uint32_t PixelFormat::bytes_per_pixel() const
{
    const bool bgra_or_rgba = format_ == a8r8g8b8 || format_ == x8r8g8b8 || format_ == a8b8g8r8 || format_ == x8b8g8r8;
    return bgra_or_rgba ? 4u : 0u;
}

}  // namespace av
