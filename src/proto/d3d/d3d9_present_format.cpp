#include "proto/d3d/d3d9_present_format.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/d3d/dxgi_format.hpp"

namespace av {

namespace {

constexpr uint32_t dxgi_b8g8r8a8_unorm = 87u;
constexpr uint32_t dxgi_b8g8r8x8_unorm = 88u;
constexpr uint32_t ddi_a8b8g8r8 = 32u;

}

D3d9PresentFormat::D3d9PresentFormat(uint32_t dxgi) : dxgi_(dxgi)
{
    AV_SCOPE("D3d9PresentFormat::D3d9PresentFormat");
}

bool D3d9PresentFormat::take_ddi(uint32_t ddi)
{
    AV_SCOPE("D3d9PresentFormat::take_ddi");
    if (ddi == 0u) return false;
    for (uint32_t value = 0u; value < DxgiFormat::count; value++) {
        const DxgiFormat format(value);
        if (format.ddi_format() != ddi || format.typeless() || format.srgb()) continue;
        dxgi_ = value;
        return true;
    }
    return false;
}

uint32_t D3d9PresentFormat::dxgi() const
{
    AV_SCOPE("D3d9PresentFormat::dxgi");
    return dxgi_;
}

uint32_t D3d9PresentFormat::ddi() const
{
    AV_SCOPE("D3d9PresentFormat::ddi");
    const uint32_t named = DxgiFormat(dxgi_).ddi_format();
    return named != 0u ? named : ddi_a8b8g8r8;
}

uint32_t D3d9PresentFormat::viewed() const
{
    AV_SCOPE("D3d9PresentFormat::viewed");
    return dxgi_ == dxgi_b8g8r8x8_unorm ? dxgi_b8g8r8a8_unorm : dxgi_;
}

}
