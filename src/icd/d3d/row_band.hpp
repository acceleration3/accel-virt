/* A run of a surface's rows, first up to but not including end. A guest
 * surface's refresh finds the band of rows that changed and may have to keep
 * it for a later refresh, so the band is a value the runtime's resource
 * record holds and copies by assignment. */
#ifndef AV_D3D_ROW_BAND_HPP
#define AV_D3D_ROW_BAND_HPP

#include <stdint.h>

namespace av::d3d {

class RowBand {
public:
    RowBand();
    /* An end before first is an empty band at first. */
    RowBand(uint32_t first, uint32_t end);

    bool     empty() const;
    uint32_t first() const;
    uint32_t end() const;

    /* The smallest band holding both; an empty one adds nothing. */
    RowBand joined(const RowBand &other) const;

private:
    uint32_t first_;
    uint32_t end_;
};

}  // namespace av::d3d

#endif  // AV_D3D_ROW_BAND_HPP
