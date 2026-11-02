#ifndef AV_D3D_ROW_BAND_HPP
#define AV_D3D_ROW_BAND_HPP

#include <stdint.h>

namespace av::d3d {

class RowBand {
public:
    RowBand();
    RowBand(uint32_t first, uint32_t end);

    bool     empty() const;
    uint32_t first() const;
    uint32_t end() const;

    RowBand joined(const RowBand &other) const;

private:
    uint32_t first_;
    uint32_t end_;
};

}

#endif
