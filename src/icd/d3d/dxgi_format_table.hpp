#ifndef AV_ICD_D3D_DXGI_FORMAT_TABLE_HPP
#define AV_ICD_D3D_DXGI_FORMAT_TABLE_HPP

#include "proto/d3d/dxgi_format.hpp"

#include <stdint.h>

namespace av::d3dproto {
struct FormatCaps;
}

namespace av::d3d {

class DxgiFormatTable {
public:
    static constexpr uint32_t sample_steps = 5u;

    DxgiFormatTable();
    DxgiFormatTable(const DxgiFormatTable &) = delete;
    DxgiFormatTable &operator=(const DxgiFormatTable &) = delete;

    void learn(const d3dproto::FormatCaps *rows, uint32_t count);

    uint32_t format_support(uint32_t format) const;
    uint32_t tiled_support(uint32_t format) const;
    uint32_t quality_levels(uint32_t format, uint32_t samples) const;

private:
    uint32_t carried(uint32_t format) const;
    bool     multisamples(uint32_t format) const;
    bool     family_multisamples(uint32_t format) const;
    uint32_t step_of(uint32_t samples) const;
    void     settle();
    void     settle_family(uint32_t head);

    uint32_t support_[DxgiFormat::count];
    uint32_t support2_[DxgiFormat::count];
    uint32_t levels_[DxgiFormat::count][sample_steps];
    uint32_t family_levels_[DxgiFormat::count][sample_steps];
    bool     keeps_[DxgiFormat::count];
};

}

#endif
