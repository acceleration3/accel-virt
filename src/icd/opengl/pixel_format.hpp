/* One pixel format the ICD offers (research.md §7): RGBA8 with the depth and
 * stencil bits, buffering, sRGB and samples it names, and the PFD_ flags
 * opengl32 is shown for it. */
#ifndef AV_GLICD_PIXEL_FORMAT_HPP
#define AV_GLICD_PIXEL_FORMAT_HPP

#include <stdint.h>

namespace av::glicd {

class PixelFormat {
public:
    static constexpr uint32_t flag_double_buffer = 0x00000001u;
    static constexpr uint32_t flag_draw_to_window = 0x00000004u;
    static constexpr uint32_t flag_support_opengl = 0x00000020u;
    static constexpr uint32_t flag_support_composition = 0x00008000u;

    PixelFormat(uint8_t depth_bits, uint8_t stencil_bits, bool double_buffer, bool srgb, uint8_t samples);

    uint8_t  depth_bits() const;
    uint8_t  stencil_bits() const;
    bool     double_buffer() const;
    bool     srgb() const;
    uint8_t  samples() const;
    uint32_t flags() const;

private:
    uint8_t depth_bits_;
    uint8_t stencil_bits_;
    bool    double_buffer_;
    bool    srgb_;
    uint8_t samples_;
};

}  // namespace av::glicd

#endif  // AV_GLICD_PIXEL_FORMAT_HPP
