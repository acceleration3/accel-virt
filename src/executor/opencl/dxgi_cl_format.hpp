/* A DXGI format as OpenCL names the same bytes: the channel order and type an
 * image over a shared Direct3D 11 texture's memory is made with
 * (OpenClBackend::import_object). Only formats whose texels OpenCL reads in
 * the order Direct3D wrote them - BGRA as CL_BGRA, not RGBA swapped - and an
 * sRGB format not at all, since OpenCL would read it as linear. */
#ifndef AV_DXGI_CL_FORMAT_HPP
#define AV_DXGI_CL_FORMAT_HPP

#include <stdint.h>

namespace av {

class DxgiClFormat {
public:
    explicit DxgiClFormat(uint32_t dxgi);

    /* Whether OpenCL has the format at all. */
    bool     known() const;
    /* cl_channel_order and cl_channel_type; 0 for an unknown format. */
    uint32_t order() const;
    uint32_t type() const;

private:
    uint32_t order_;
    uint32_t type_;
};

}  // namespace av

#endif  // AV_DXGI_CL_FORMAT_HPP
