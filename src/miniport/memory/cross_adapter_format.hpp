/* A D3DDDIFORMAT a cross-adapter texture can be made in, and its bytes per
 * pixel: the 32-bit formats PixelFormat lays out, and the wider ones a
 * Direct3D texture shared between two adapters is made in. Kept apart from
 * PixelFormat, which is what primaries, shadows and overlay planes are
 * judged by: those stay 32-bit. The space is D3DDDIFORMAT's, which
 * D3DKMDT_GDISURFACEDATA.Format is typed as; a dxgkrnl that names DXGI
 * formats there instead (diagnosed by the refusal's log line,
 * Adapter::get_standard_allocation_driver_data) is taught here, and only
 * here. */
#ifndef AV_CROSS_ADAPTER_FORMAT_HPP
#define AV_CROSS_ADAPTER_FORMAT_HPP

#include <stdint.h>

namespace av {

class CrossAdapterFormat {
public:
    explicit CrossAdapterFormat(uint32_t format);

    /* Zero for a format a cross-adapter texture is not laid out in. */
    uint32_t bytes_per_pixel() const;

private:
    enum : uint32_t {
        a2b10g10r10 = 31u,
        a2r10g10b10 = 35u,
        a16b16g16r16f = 113u,
        a32b32g32r32f = 116u
    };

    uint32_t format_;
};

}  // namespace av

#endif  // AV_CROSS_ADAPTER_FORMAT_HPP
