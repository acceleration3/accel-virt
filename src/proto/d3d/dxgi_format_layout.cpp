#include "proto/d3d/dxgi_format_layout.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/d3d/dxgi_format.hpp"

namespace av {

DxgiFormatLayout::DxgiFormatLayout(uint32_t format) : format_(format)
{
    AV_SCOPE("DxgiFormatLayout::DxgiFormatLayout");
    const DxgiFormat row(format_);
    if (row.planar_sample_bytes() != 0u)
        lay_out_planar(row.planar_sample_bytes());
    else
        lay_out(row.element_bytes(), row.element_width(), row.element_height());
}

bool DxgiFormatLayout::known() const
{
    AV_SCOPE("DxgiFormatLayout::known");
    return DxgiFormat(format_).known();
}

}
