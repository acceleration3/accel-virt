#include "device/display/scanout_format.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

namespace {

constexpr uint32_t fourcc_of(char a, char b, char c, char d)
{
    return static_cast<uint32_t>(a) | (static_cast<uint32_t>(b) << 8) | (static_cast<uint32_t>(c) << 16) |
           (static_cast<uint32_t>(d) << 24);
}

constexpr uint32_t drm_argb8888 = fourcc_of('A', 'R', '2', '4');
constexpr uint32_t drm_xrgb8888 = fourcc_of('X', 'R', '2', '4');
constexpr uint32_t drm_bgra8888 = fourcc_of('B', 'A', '2', '4');
constexpr uint32_t drm_bgrx8888 = fourcc_of('B', 'X', '2', '4');
constexpr uint32_t drm_abgr8888 = fourcc_of('A', 'B', '2', '4');
constexpr uint32_t drm_rgbx8888 = fourcc_of('R', 'X', '2', '4');
constexpr uint32_t drm_rgba8888 = fourcc_of('R', 'A', '2', '4');
constexpr uint32_t drm_xbgr8888 = fourcc_of('X', 'B', '2', '4');

constexpr uint32_t pixel_width = 4u;

}  // namespace

ScanoutFormat::ScanoutFormat(uint32_t format) : format_(format)
{
    AV_SCOPE("ScanoutFormat::ScanoutFormat");
}

uint32_t ScanoutFormat::fourcc() const
{
    AV_SCOPE("ScanoutFormat::fourcc");
    switch (format_) {
    case b8g8r8a8: return drm_argb8888;
    case b8g8r8x8: return drm_xrgb8888;
    case a8r8g8b8: return drm_bgra8888;
    case x8r8g8b8: return drm_bgrx8888;
    case r8g8b8a8: return drm_abgr8888;
    case x8b8g8r8: return drm_rgbx8888;
    case a8b8g8r8: return drm_rgba8888;
    case r8g8b8x8: return drm_xbgr8888;
    default: return 0u;
    }
}

bool ScanoutFormat::scannable() const
{
    AV_SCOPE("ScanoutFormat::scannable");
    return fourcc() != 0u;
}

uint32_t ScanoutFormat::pixel_bytes() const
{
    AV_SCOPE("ScanoutFormat::pixel_bytes");
    return scannable() ? pixel_width : 0u;
}

uint32_t ScanoutFormat::format() const
{
    AV_SCOPE("ScanoutFormat::format");
    return format_;
}

}  // namespace av
