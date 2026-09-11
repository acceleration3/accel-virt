#include "core/display/display_mode.hpp"

#include "core/ddi/d3dkmdt_video_signal_info_record.hpp"
#include "core/ddi/d3dkmdt_vidpn_source_mode_record.hpp"

namespace av {

DisplayMode::DisplayMode(uint32_t width, uint32_t height) : width_(width), height_(height) {}

uint32_t DisplayMode::width() const { return width_; }
uint32_t DisplayMode::height() const { return height_; }
uint32_t DisplayMode::total_width() const { return width_ + horizontal_blank; }
uint32_t DisplayMode::total_height() const { return height_ + height_ / active_lines_per_blank_line; }

uint64_t DisplayMode::pixel_rate() const
{
    return static_cast<uint64_t>(total_width()) * total_height() * refresh_hz;
}

void DisplayMode::describe(D3dkmdtVidPnSourceModeRecord &source) const
{
    source.Type = rendering_graphics;
    source.PrimSurfSizeCx = width_;
    source.PrimSurfSizeCy = height_;
    source.VisibleRegionSizeCx = width_;
    source.VisibleRegionSizeCy = height_;
    source.Stride = width_ * bytes_per_pixel;
    source.PixelFormat = format_x8r8g8b8;
    source.ColorBasis = color_basis_srgb;
    source.PixelValueAccessMode = access_direct;
}

void DisplayMode::describe(D3dkmdtVideoSignalInfoRecord &signal) const
{
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
    return source.PrimSurfSizeCx == width_ && source.PrimSurfSizeCy == height_ &&
           source.PixelFormat == format_x8r8g8b8;
}

bool DisplayMode::matches(const D3dkmdtVideoSignalInfoRecord &signal) const
{
    return signal.ActiveSizeCx == width_ && signal.ActiveSizeCy == height_;
}

/* Reduced modulo one second first, so no interrupt time overflows. */
uint32_t DisplayMode::scan_line(uint64_t interrupt_time) const
{
    const uint64_t frame_position = (interrupt_time % interrupt_ticks_per_second) * refresh_hz %
                                    interrupt_ticks_per_second;
    return static_cast<uint32_t>(frame_position * total_height() / interrupt_ticks_per_second);
}

}  // namespace av
