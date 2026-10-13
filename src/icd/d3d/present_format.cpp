#include "icd/d3d/present_format.hpp"

#include "core/trace/function_scope.hpp"

namespace av::d3d {

namespace {

constexpr uint32_t dxgi_r10g10b10a2_typeless = 23u;
constexpr uint32_t dxgi_r10g10b10a2_unorm = 24u;
constexpr uint32_t dxgi_r16g16b16a16_float = 10u;
constexpr uint32_t dxgi_b8g8r8a8_unorm = 87u;
constexpr uint32_t dxgi_b8g8r8x8_unorm = 88u;
constexpr uint32_t dxgi_b8g8r8a8_typeless = 90u;
constexpr uint32_t dxgi_b8g8r8a8_unorm_srgb = 91u;
constexpr uint32_t dxgi_b8g8r8x8_typeless = 92u;
constexpr uint32_t dxgi_b8g8r8x8_unorm_srgb = 93u;
constexpr uint32_t ddi_a8r8g8b8 = 21u;
constexpr uint32_t ddi_x8r8g8b8 = 22u;
constexpr uint32_t ddi_a2b10g10r10 = 31u;
constexpr uint32_t ddi_a8b8g8r8 = 32u;
constexpr uint32_t ddi_a16b16g16r16f = 113u;
/* The video surfaces an overlay plane shows: DXGI's NV12 and YUY2, and the
 * D3DDDIFMT fourccs the miniport reads them by (OverlaySupport). */
constexpr uint32_t dxgi_nv12 = 103u;
constexpr uint32_t dxgi_yuy2 = 107u;
constexpr uint32_t ddi_nv12 = 0x3231564Eu;
constexpr uint32_t ddi_yuy2 = 0x32595559u;

}  // namespace

PresentFormat::PresentFormat(uint32_t dxgi) : dxgi_(dxgi)
{
    AV_SCOPE("PresentFormat::PresentFormat");
}

bool PresentFormat::take_ddi(uint32_t ddi)
{
    AV_SCOPE("PresentFormat::take_ddi");
    switch (ddi) {
    case ddi_a8r8g8b8: dxgi_ = dxgi_b8g8r8a8_unorm; return true;
    case ddi_x8r8g8b8: dxgi_ = dxgi_b8g8r8x8_unorm; return true;
    case ddi_a2b10g10r10: dxgi_ = dxgi_r10g10b10a2_unorm; return true;
    case ddi_a16b16g16r16f: dxgi_ = dxgi_r16g16b16a16_float; return true;
    case ddi_nv12: dxgi_ = dxgi_nv12; return true;
    case ddi_yuy2: dxgi_ = dxgi_yuy2; return true;
    default: return false;
    }
}

uint32_t PresentFormat::dxgi() const
{
    AV_SCOPE("PresentFormat::dxgi");
    return dxgi_;
}

uint32_t PresentFormat::ddi() const
{
    AV_SCOPE("PresentFormat::ddi");
    switch (dxgi_) {
    case dxgi_b8g8r8a8_unorm:
    case dxgi_b8g8r8a8_unorm_srgb:
    case dxgi_b8g8r8a8_typeless: return ddi_a8r8g8b8;
    case dxgi_b8g8r8x8_unorm:
    case dxgi_b8g8r8x8_unorm_srgb:
    case dxgi_b8g8r8x8_typeless: return ddi_x8r8g8b8;
    case dxgi_r10g10b10a2_unorm:
    case dxgi_r10g10b10a2_typeless: return ddi_a2b10g10r10;
    case dxgi_r16g16b16a16_float: return ddi_a16b16g16r16f;
    case dxgi_nv12: return ddi_nv12;
    case dxgi_yuy2: return ddi_yuy2;
    default: return ddi_a8b8g8r8;
    }
}

}  // namespace av::d3d
