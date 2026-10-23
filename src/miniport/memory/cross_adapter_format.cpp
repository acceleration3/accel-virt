#include "miniport/memory/cross_adapter_format.hpp"

#include "core/trace/function_scope.hpp"
#include "miniport/memory/pixel_format.hpp"

namespace av {

CrossAdapterFormat::CrossAdapterFormat(uint32_t format) : format_(format)
{
    AV_SCOPE("CrossAdapterFormat::CrossAdapterFormat");
}

uint32_t CrossAdapterFormat::bytes_per_pixel() const
{
    AV_SCOPE("CrossAdapterFormat::bytes_per_pixel");
    if (format_ == a2b10g10r10 || format_ == a2r10g10b10) return 4u;
    if (format_ == a16b16g16r16f) return 8u;
    if (format_ == a32b32g32r32f) return 16u;
    return PixelFormat(format_).bytes_per_pixel();
}

}  // namespace av
