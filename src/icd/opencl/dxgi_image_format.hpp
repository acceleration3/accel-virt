/* The OpenCL image format a Direct3D 11 texture's DXGI_FORMAT is, as the
 * extension's table maps one (cl_khr_d3d11_sharing, "Direct3D 11 and OpenCL
 * image formats"). A typeless format, a compressed one, a depth one, and
 * anything else the table does not name maps to nothing: an application asking
 * for an image over one is told CL_INVALID_IMAGE_FORMAT_DESCRIPTOR. */
#ifndef AV_CLICD_DXGI_IMAGE_FORMAT_HPP
#define AV_CLICD_DXGI_IMAGE_FORMAT_HPP

#include <CL/cl.h>

#include <stdint.h>

namespace av::clicd {

class DxgiImageFormat {
public:
    explicit DxgiImageFormat(uint32_t dxgi_format);

    bool            mapped() const;
    /* The format; channel order and type 0 when it maps to nothing. */
    cl_image_format format() const;

private:
    cl_image_format format_;
    bool            mapped_;
};

}  // namespace av::clicd

#endif  // AV_CLICD_DXGI_IMAGE_FORMAT_HPP
