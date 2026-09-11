/* One resolution the adapter scans out: a 32-bpp X8R8G8B8 primary with
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

    uint32_t width() const;
    uint32_t height() const;
    uint32_t total_width() const;
    uint32_t total_height() const;
    uint64_t pixel_rate() const;

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
    static constexpr uint32_t active_lines_per_blank_line = 24u;
    static constexpr uint32_t bytes_per_pixel = 4u;
    static constexpr uint64_t interrupt_ticks_per_second = 10000000u;
    enum : int32_t {
        rendering_graphics = 1,   /* D3DKMDT_RMT_GRAPHICS */
        format_x8r8g8b8 = 22,     /* D3DDDIFMT_X8R8G8B8 */
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
