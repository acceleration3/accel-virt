#include "executor/shim/drm_fourcc.hpp"

#include "core/support/four_cc.hpp"
#include "core/trace/function_scope.hpp"
#include "executor/shim/d3d_headers.hpp"

namespace av {

DrmFourcc::DrmFourcc(uint32_t dxgi_format) : value_(0u)
{
    AV_SCOPE("DrmFourcc::DrmFourcc");
    switch (dxgi_format) {
    case DXGI_FORMAT_B8G8R8A8_UNORM:
    case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB: value_ = FourCc('A', 'R', '2', '4').value(); break;
    case DXGI_FORMAT_B8G8R8X8_UNORM: value_ = FourCc('X', 'R', '2', '4').value(); break;
    case DXGI_FORMAT_R8G8B8A8_UNORM:
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB: value_ = FourCc('A', 'B', '2', '4').value(); break;
    case DXGI_FORMAT_R10G10B10A2_UNORM: value_ = FourCc('A', 'B', '3', '0').value(); break;
    case DXGI_FORMAT_R16G16B16A16_FLOAT: value_ = FourCc('A', 'B', '4', 'H').value(); break;
    default: break;
    }
}

uint32_t DrmFourcc::value() const
{
    AV_SCOPE("DrmFourcc::value");
    return value_;
}

}  // namespace av
