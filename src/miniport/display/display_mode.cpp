#include "miniport/display/display_mode.hpp"

#include "miniport/ddi/d3dkmdt_video_signal_info_record.hpp"
#include "miniport/ddi/d3dkmdt_vidpn_source_mode_record.hpp"
#include "core/trace/function_scope.hpp"
#include "proto/resources/guest_page_list.hpp"

namespace av {

DisplayMode::DisplayMode(uint32_t width, uint32_t height) : width_(width), height_(height)
{
    AV_SCOPE("DisplayMode::DisplayMode");
}

DisplayMode::DisplayMode() : DisplayMode(smallest_width, smallest_height)
{
    AV_SCOPE("DisplayMode::DisplayMode");
}

uint32_t DisplayMode::frame_pages() const
{
    AV_SCOPE("DisplayMode::frame_pages");
    const uint64_t bytes = uint64_t{width_} * height_ * bytes_per_pixel;
    return static_cast<uint32_t>((bytes + GuestPageList::page_bytes - 1u) / GuestPageList::page_bytes);
}

bool DisplayMode::offerable() const
{
    AV_SCOPE("DisplayMode::offerable");
    if (width_ < min_source_edge || height_ < min_source_edge) return false;
    if (width_ > max_edid_edge || height_ > max_edid_edge) return false;
    if (total_height() - height_ > max_edid_edge) return false;
    return pixel_rate() / edid_pixel_clock_unit_hz <= max_edid_pixel_clock_units;
}

uint32_t DisplayMode::width() const
{
    AV_SCOPE("DisplayMode::width");
    return width_;
}
uint32_t DisplayMode::height() const
{
    AV_SCOPE("DisplayMode::height");
    return height_;
}
uint32_t DisplayMode::total_width() const
{
    AV_SCOPE("DisplayMode::total_width");
    return width_ + horizontal_blank;
}
uint32_t DisplayMode::total_height() const
{
    AV_SCOPE("DisplayMode::total_height");
    return height_ + height_ / active_lines_per_blank_line;
}

uint64_t DisplayMode::pixel_rate() const
{
    AV_SCOPE("DisplayMode::pixel_rate");
    return static_cast<uint64_t>(total_width()) * total_height() * refresh_hz;
}

void DisplayMode::describe(D3dkmdtVidPnSourceModeRecord &source, int32_t format) const
{
    AV_SCOPE("DisplayMode::describe");
    source.Type = rendering_graphics;
    source.PrimSurfSizeCx = width_;
    source.PrimSurfSizeCy = height_;
    source.VisibleRegionSizeCx = width_;
    source.VisibleRegionSizeCy = height_;
    source.Stride = width_ * bytes_per_pixel;
    source.PixelFormat = format;
    source.ColorBasis = color_basis_srgb;
    source.PixelValueAccessMode = access_direct;
}

void DisplayMode::describe(D3dkmdtVideoSignalInfoRecord &signal) const
{
    AV_SCOPE("DisplayMode::describe");
    signal.VideoStandard = standard_other;
    signal.TotalSizeCx = total_width();
    signal.TotalSizeCy = total_height();
    signal.ActiveSizeCx = width_;
    signal.ActiveSizeCy = height_;
    signal.VSyncFreqNumerator = refresh_hz;
    signal.VSyncFreqDenominator = 1u;
    signal.HSyncFreqNumerator = total_height() * refresh_hz;
    signal.HSyncFreqDenominator = 1u;
    signal.PixelRate = pixel_rate();
    signal.ScanLineOrdering = ordering_progressive;
}

bool DisplayMode::matches(const D3dkmdtVidPnSourceModeRecord &source) const
{
    AV_SCOPE("DisplayMode::matches");
    if (source.PrimSurfSizeCx != width_ || source.PrimSurfSizeCy != height_) return false;
    for (const int32_t format : source_formats)
        if (source.PixelFormat == format) return true;
    return false;
}

bool DisplayMode::matches(const D3dkmdtVideoSignalInfoRecord &signal) const
{
    AV_SCOPE("DisplayMode::matches");
    return signal.ActiveSizeCx == width_ && signal.ActiveSizeCy == height_;
}

uint32_t DisplayMode::scan_line_within(uint64_t since, uint64_t period) const
{
    AV_SCOPE("DisplayMode::scan_line_within");
    if (period == 0u) return height_;
    const uint64_t into = since % period * total_height() / period;
    return static_cast<uint32_t>((height_ + into) % total_height());
}

/* Reduced modulo one second first, so no interrupt time overflows. */
uint32_t DisplayMode::scan_line(uint64_t interrupt_time) const
{
    AV_SCOPE("DisplayMode::scan_line");
    const uint64_t frame_position = (interrupt_time % interrupt_ticks_per_second) * refresh_hz %
                                    interrupt_ticks_per_second;
    return static_cast<uint32_t>(frame_position * total_height() / interrupt_ticks_per_second);
}

}  // namespace av
