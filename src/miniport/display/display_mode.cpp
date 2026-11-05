#include "miniport/display/display_mode.hpp"

#include "miniport/ddi/d3dkmdt_video_signal_info_record.hpp"
#include "miniport/ddi/d3dkmdt_vidpn_source_mode_record.hpp"
#include "core/trace/function_scope.hpp"
#include "proto/resources/guest_page_list.hpp"

namespace av {

namespace {

uint64_t greatest_common_divisor(uint64_t first, uint64_t second)
{
    AV_SCOPE("greatest_common_divisor");
    while (second != 0u) {
        const uint64_t rest = first % second;
        first = second;
        second = rest;
    }
    return first;
}

void rational(uint64_t numerator, uint64_t denominator, uint32_t &numerator_out, uint32_t &denominator_out)
{
    AV_SCOPE("rational");
    const uint64_t divisor = greatest_common_divisor(numerator, denominator);
    numerator /= divisor;
    denominator /= divisor;
    while (numerator > 0xFFFFFFFFu && denominator > 1u) {
        numerator = (numerator + 1u) / 2u;
        denominator = (denominator + 1u) / 2u;
    }
    numerator_out = numerator > 0xFFFFFFFFu ? 0xFFFFFFFFu : static_cast<uint32_t>(numerator);
    denominator_out = static_cast<uint32_t>(denominator);
}

}

DisplayMode::DisplayMode(uint32_t width, uint32_t height, uint32_t refresh_millihz)
    : width_(width), height_(height), refresh_millihz_(refresh_millihz)
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
    return total_height() - height_ <= max_edid_edge;
}

DisplayMode DisplayMode::edid_timing() const
{
    AV_SCOPE("DisplayMode::edid_timing");
    const uint64_t frame = uint64_t{total_width()} * total_height();
    const uint64_t fastest = max_edid_pixel_clock_units * edid_pixel_clock_unit_hz * millihertz_per_hertz / frame;
    if (refresh_millihz_ <= fastest) return *this;
    return DisplayMode(width_, height_, static_cast<uint32_t>(fastest));
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
uint32_t DisplayMode::refresh_millihz() const
{
    AV_SCOPE("DisplayMode::refresh_millihz");
    return refresh_millihz_;
}

uint64_t DisplayMode::period() const
{
    AV_SCOPE("DisplayMode::period");
    return interrupt_ticks_per_second * millihertz_per_hertz / refresh_millihz_;
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
    const uint64_t frame = uint64_t{total_width()} * total_height();
    return (frame * refresh_millihz_ + millihertz_per_hertz / 2u) / millihertz_per_hertz;
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
    rational(refresh_millihz_, millihertz_per_hertz, signal.VSyncFreqNumerator, signal.VSyncFreqDenominator);
    rational(uint64_t{total_height()} * refresh_millihz_, millihertz_per_hertz, signal.HSyncFreqNumerator,
             signal.HSyncFreqDenominator);
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
    if (signal.ActiveSizeCx != width_ || signal.ActiveSizeCy != height_) return false;
    D3dkmdtVideoSignalInfoRecord own = {};
    describe(own);
    return signal.VSyncFreqNumerator == own.VSyncFreqNumerator && signal.VSyncFreqDenominator == own.VSyncFreqDenominator;
}

uint32_t DisplayMode::scan_line_within(uint64_t since, uint64_t period) const
{
    AV_SCOPE("DisplayMode::scan_line_within");
    if (period == 0u) return height_;
    const uint64_t into = since % period * total_height() / period;
    return static_cast<uint32_t>((height_ + into) % total_height());
}

uint32_t DisplayMode::scan_line(uint64_t interrupt_time) const
{
    AV_SCOPE("DisplayMode::scan_line");
    const uint64_t frame_position = (interrupt_time % interrupt_ticks_per_second) * refresh_millihz_ /
                                    millihertz_per_hertz % interrupt_ticks_per_second;
    return static_cast<uint32_t>(frame_position * total_height() / interrupt_ticks_per_second);
}

}
