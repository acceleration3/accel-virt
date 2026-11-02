#ifndef AV_DISPLAY_MODE_HPP
#define AV_DISPLAY_MODE_HPP

#include <stdint.h>

namespace av {

struct D3dkmdtVideoSignalInfoRecord;
struct D3dkmdtVidPnSourceModeRecord;

class DisplayMode {
public:
    static constexpr uint32_t default_refresh_millihz = 60000u;
    static constexpr uint32_t source_format_count = 2u;
    static constexpr int32_t  source_formats[source_format_count] = {21, 32};

    DisplayMode(uint32_t width, uint32_t height, uint32_t refresh_millihz = default_refresh_millihz);
    DisplayMode();

    uint32_t width() const;
    uint32_t height() const;
    uint32_t refresh_millihz() const;
    uint64_t period() const;
    uint32_t total_width() const;
    uint32_t total_height() const;
    uint64_t pixel_rate() const;
    uint32_t frame_pages() const;
    bool        offerable() const;
    DisplayMode edid_timing() const;

    void describe(D3dkmdtVidPnSourceModeRecord &source, int32_t format) const;
    void describe(D3dkmdtVideoSignalInfoRecord &signal) const;
    bool matches(const D3dkmdtVidPnSourceModeRecord &source) const;
    bool matches(const D3dkmdtVideoSignalInfoRecord &signal) const;

    uint32_t scan_line(uint64_t interrupt_time) const;
    uint32_t scan_line_within(uint64_t since, uint64_t period) const;

private:
    static constexpr uint32_t horizontal_blank = 160u;
    static constexpr uint32_t smallest_width = 640u;
    static constexpr uint32_t smallest_height = 480u;
    static constexpr uint32_t min_source_edge = 100u;
    static constexpr uint32_t max_edid_edge = 4095u;
    static constexpr uint64_t edid_pixel_clock_unit_hz = 10000u;
    static constexpr uint64_t max_edid_pixel_clock_units = 65535u;
    static constexpr uint32_t millihertz_per_hertz = 1000u;
    static constexpr uint32_t active_lines_per_blank_line = 24u;
    static constexpr uint32_t bytes_per_pixel = 4u;
    static constexpr uint64_t interrupt_ticks_per_second = 10000000u;
    enum : int32_t {
        rendering_graphics = 1,
        color_basis_srgb = 2,
        access_direct = 1,
        standard_other = 255,
    };
    static constexpr uint32_t ordering_progressive = 1u;

    uint32_t width_;
    uint32_t height_;
    uint32_t refresh_millihz_;
};

}

#endif
