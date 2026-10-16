#include "device/display/scanout_format.hpp"

#include "core/support/four_cc.hpp"
#include "core/trace/function_scope.hpp"

namespace av {

namespace {

constexpr uint32_t drm_argb8888 = FourCc('A', 'R', '2', '4').value();
constexpr uint32_t drm_xrgb8888 = FourCc('X', 'R', '2', '4').value();
constexpr uint32_t drm_bgra8888 = FourCc('B', 'A', '2', '4').value();
constexpr uint32_t drm_bgrx8888 = FourCc('B', 'X', '2', '4').value();
constexpr uint32_t drm_abgr8888 = FourCc('A', 'B', '2', '4').value();
constexpr uint32_t drm_rgbx8888 = FourCc('R', 'X', '2', '4').value();
constexpr uint32_t drm_rgba8888 = FourCc('R', 'A', '2', '4').value();
constexpr uint32_t drm_xbgr8888 = FourCc('X', 'B', '2', '4').value();

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
