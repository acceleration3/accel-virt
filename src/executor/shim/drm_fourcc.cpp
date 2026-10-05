#include "executor/shim/drm_fourcc.hpp"

#include "core/trace/function_scope.hpp"
#include "executor/shim/d3d_headers.hpp"

namespace av {

namespace {

constexpr uint32_t fourcc(char a, char b, char c, char d)
{
    return static_cast<uint32_t>(a) | static_cast<uint32_t>(b) << 8u | static_cast<uint32_t>(c) << 16u |
           static_cast<uint32_t>(d) << 24u;
}

}  // namespace

DrmFourcc::DrmFourcc(uint32_t dxgi_format) : value_(0u)
{
    AV_SCOPE("DrmFourcc::DrmFourcc");
    switch (dxgi_format) {
    case DXGI_FORMAT_B8G8R8A8_UNORM:
    case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB: value_ = fourcc('A', 'R', '2', '4'); break;
    case DXGI_FORMAT_B8G8R8X8_UNORM: value_ = fourcc('X', 'R', '2', '4'); break;
    case DXGI_FORMAT_R8G8B8A8_UNORM:
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB: value_ = fourcc('A', 'B', '2', '4'); break;
    case DXGI_FORMAT_R10G10B10A2_UNORM: value_ = fourcc('A', 'B', '3', '0'); break;
    case DXGI_FORMAT_R16G16B16A16_FLOAT: value_ = fourcc('A', 'B', '4', 'H'); break;
    default: break;
    }
}

uint32_t DrmFourcc::value() const
{
    AV_SCOPE("DrmFourcc::value");
    return value_;
}

}  // namespace av
