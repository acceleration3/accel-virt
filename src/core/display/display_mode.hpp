/* One resolution the adapter scans out: a 32-bpp A8R8G8B8 primary with
 * progressive 60 Hz timing whose blanking keeps 1080p's proportions (160
 * columns, and one blank line for every 24 active ones). */
#ifndef AV_DISPLAY_MODE_HPP
#define AV_DISPLAY_MODE_HPP

#include <stdint.h>

namespace av {

struct D3dkmdtVideoSignalInfoRecord;
struct D3dkmdtVidPnSourceModeRecord;

class DisplayMode {
public:
    static constexpr uint32_t refresh_hz = 60u;

    DisplayMode(uint32_t width, uint32_t height);
    /* 640x480: what a list's slot holds until the list fills it. */
    DisplayMode();

    uint32_t width() const;
    uint32_t height() const;
    uint32_t total_width() const;
    uint32_t total_height() const;
    uint64_t pixel_rate() const;
    /* Whether Windows can be offered it: dxgkrnl refuses a source or signal
     * mode under 100 pixels on an edge (DMMVIDPNSOURCEMODE::_ValidateDdiGraphicsRenderingFormat,
     * DMMVIDEOSIGNALMODE::_ValidateDdiVideoSignalModeInfo, whose 1 MHz clock
     * floor 100x100 clears), and the monitor's EDID must be able to name it
     * preferred in a detailed timing - twelve bits for each edge and each
     * blank, sixteen for the pixel clock in 10 kHz units. */
    bool     offerable() const;

    /* Leaves the Id dxgkrnl assigned. */
    void describe(D3dkmdtVidPnSourceModeRecord &source) const;
    void describe(D3dkmdtVideoSignalInfoRecord &signal) const;
    bool matches(const D3dkmdtVidPnSourceModeRecord &source) const;
    bool matches(const D3dkmdtVideoSignalInfoRecord &signal) const;

    /* The line being scanned at `interrupt_time`; at or past height() it is in
     * vertical blank. */
    uint32_t scan_line(uint64_t interrupt_time) const;

private:
    static constexpr uint32_t horizontal_blank = 160u;
    static constexpr uint32_t smallest_width = 640u;
    static constexpr uint32_t smallest_height = 480u;
    static constexpr uint32_t min_source_edge = 100u;
    static constexpr uint32_t max_edid_edge = 4095u;
    static constexpr uint64_t edid_pixel_clock_unit_hz = 10000u;
    static constexpr uint64_t max_edid_pixel_clock_units = 65535u;
    static constexpr uint32_t active_lines_per_blank_line = 24u;
    static constexpr uint32_t bytes_per_pixel = 4u;
    static constexpr uint64_t interrupt_ticks_per_second = 10000000u;
    enum : int32_t {
        rendering_graphics = 1,   /* D3DKMDT_RMT_GRAPHICS */
        /* D3DDDIFMT_A8R8G8B8, not X8R8G8B8: cdd.dll's DrvGetModes keeps only
         * R8G8B8, A8R8G8B8, R5G6B5 and P8 source modes, and a list it empties
         * leaves win32k one made-up 800x600 and Settings no resolution to pick. */
        format_a8r8g8b8 = 21,
        color_basis_srgb = 2,     /* D3DKMDT_CB_SRGB */
        access_direct = 1,        /* D3DKMDT_PVAM_DIRECT */
        standard_other = 255,     /* D3DKMDT_VSS_OTHER */
    };
    static constexpr uint32_t ordering_progressive = 1u;   /* D3DDDI_VSSLO_PROGRESSIVE */

    uint32_t width_;
    uint32_t height_;
};

}  // namespace av

#endif  // AV_DISPLAY_MODE_HPP
