#ifndef AV_D3D12_SHADING_RATE_HPP
#define AV_D3D12_SHADING_RATE_HPP

#include <stdint.h>

namespace av {

class D3d12ShadingRate {
public:
    D3d12ShadingRate(uint32_t base, uint32_t first_combiner, uint32_t second_combiner);

    bool holds(uint32_t tier, bool additional_rates, bool sum_combiner) const;

private:
    uint32_t base_;
    uint32_t combiners_[2];
};

}

#endif
