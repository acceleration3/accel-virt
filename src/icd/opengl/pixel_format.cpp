#include "icd/opengl/pixel_format.hpp"

#include "core/trace/function_scope.hpp"

namespace av::glicd {

PixelFormat::PixelFormat(uint8_t depth_bits, uint8_t stencil_bits, bool double_buffer, bool srgb,
                                 uint8_t samples)
    : depth_bits_(depth_bits), stencil_bits_(stencil_bits), double_buffer_(double_buffer), srgb_(srgb),
      samples_(samples)
      {
          AV_SCOPE("PixelFormat::PixelFormat");
      }

uint8_t PixelFormat::depth_bits() const
{
    AV_SCOPE("PixelFormat::depth_bits");
    return depth_bits_;
}
uint8_t PixelFormat::stencil_bits() const
{
    AV_SCOPE("PixelFormat::stencil_bits");
    return stencil_bits_;
}
bool    PixelFormat::double_buffer() const
{
    AV_SCOPE("PixelFormat::double_buffer");
    return double_buffer_;
}
bool    PixelFormat::srgb() const
{
    AV_SCOPE("PixelFormat::srgb");
    return srgb_;
}
uint8_t PixelFormat::samples() const
{
    AV_SCOPE("PixelFormat::samples");
    return samples_;
}

uint32_t PixelFormat::flags() const
{
    AV_SCOPE("PixelFormat::flags");
    const uint32_t window = flag_draw_to_window | flag_support_opengl | flag_support_composition;
    return double_buffer_ ? window | flag_double_buffer : window;
}

}
