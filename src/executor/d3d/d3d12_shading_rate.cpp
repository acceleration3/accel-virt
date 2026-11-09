#include "executor/d3d/d3d12_shading_rate.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

namespace {
constexpr uint32_t rate_1x1 = 0x0u;
constexpr uint32_t rate_1x2 = 0x1u;
constexpr uint32_t rate_2x1 = 0x4u;
constexpr uint32_t rate_2x2 = 0x5u;
constexpr uint32_t rate_2x4 = 0x6u;
constexpr uint32_t rate_4x2 = 0x9u;
constexpr uint32_t rate_4x4 = 0xAu;
constexpr uint32_t combiner_sum = 4u;

}

D3d12ShadingRate::D3d12ShadingRate(uint32_t base, uint32_t first_combiner, uint32_t second_combiner)
    : base_(base), combiners_{first_combiner, second_combiner}
{
    AV_SCOPE("D3d12ShadingRate::D3d12ShadingRate");
}

bool D3d12ShadingRate::holds(uint32_t tier, bool additional_rates, bool sum_combiner) const
{
    AV_SCOPE("D3d12ShadingRate::holds");
    if (tier == 0u) return false;
    const bool plain = base_ == rate_1x1 || base_ == rate_1x2 || base_ == rate_2x1 || base_ == rate_2x2;
    const bool extra = base_ == rate_2x4 || base_ == rate_4x2 || base_ == rate_4x4;
    if (!plain && !(extra && additional_rates)) return false;
    for (const uint32_t combiner : combiners_) {
        if (combiner > combiner_sum || (combiner == combiner_sum && !sum_combiner)) return false;
        if (tier < 2u && combiner != 0u) return false;
    }
    return true;
}

}
