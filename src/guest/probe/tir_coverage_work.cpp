#include "guest/probe/tir_coverage_work.hpp"

#include "core/support/status.hpp"
#include "core/trace/function_scope.hpp"
#include "guest/probe/coverage_triangle.hpp"
#include "guest/probe/d3d11_tir_entry.hpp"
#include "guest/suite/scenario_steps.hpp"

namespace av {

namespace {

constexpr int32_t pixel = static_cast<int32_t>(CoverageTriangle::subpixels);
constexpr float   half_extent = static_cast<float>(pixel * static_cast<int32_t>(TirCoverageWork::extent / 2u));

}

TirCoverageWork::TirCoverageWork(D3d11TirEntry &d3d11)
    : d3d11_(d3d11), made_(false), read_(false), added_(0u), x_{}, y_{}, positions_{}, texels_{}, expected_{}
{
    AV_SCOPE("TirCoverageWork::TirCoverageWork");
    for (int32_t i = 0; i < static_cast<int32_t>(straight); i++) {
        const int32_t width = (1 + i) * pixel / 32;
        const int32_t row = (2 + 3 * i) * pixel + i * pixel / 16;
        quad(pixel + i * pixel / 8, row, 62 * pixel + pixel / 2, row + width);
        const int32_t column = row + pixel / 32;
        quad(column, 26 * pixel, column + width, 62 * pixel - i * pixel / 4);
    }
    for (int32_t i = 0; i < static_cast<int32_t>(slanted); i++)
        stroke((28 + 8 * i) * pixel, 27 * pixel + i * pixel / 16, (36 + 6 * i) * pixel, 62 * pixel, pixel / 16);
    expect();
}

TirCoverageWork::~TirCoverageWork()
{
    AV_SCOPE("TirCoverageWork::~TirCoverageWork");
    if (made_) d3d11_.release();
}

void TirCoverageWork::run(ScenarioSteps &steps)
{
    AV_SCOPE("TirCoverageWork::run");
    if (steps.check_result("tir_device", d3d11_.create_device()) != Status::ok) return;
    made_ = true;
    if (steps.check_result("tir_pipeline", d3d11_.create_pipeline(extent, samples)) != Status::ok) return;
    if (steps.check_result("tir_draw", d3d11_.draw(positions_, vertices)) != Status::ok) return;
    if (steps.check_result("tir_readback", d3d11_.read_back(texels_, texels)) != Status::ok) return;
    read_ = true;
    if (d3d11_.layered()) {
        steps.note("tir_coverage", "the device is a layer's: DXVK as released rasterizes a forced sample count "
                                   "at the render target's");
    } else {
        const uint32_t matching = right();
        steps.check_measured("tir_coverage", matching == texels ? Status::ok : Status::invalid_state, matching);
    }
    static_cast<void>(steps.check_result("tir_device_kept", d3d11_.removed_reason()));
}

void TirCoverageWork::quad(int32_t x0, int32_t y0, int32_t x1, int32_t y1)
{
    AV_SCOPE("TirCoverageWork::quad");
    add(x0, y0);
    add(x1, y0);
    add(x1, y1);
    add(x0, y0);
    add(x1, y1);
    add(x0, y1);
}

void TirCoverageWork::stroke(int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t width)
{
    AV_SCOPE("TirCoverageWork::stroke");
    add(x0, y0);
    add(x1, y1);
    add(x1 + width, y1);
    add(x0, y0);
    add(x1 + width, y1);
    add(x0 + width, y0);
}

void TirCoverageWork::add(int32_t x, int32_t y)
{
    AV_SCOPE("TirCoverageWork::add");
    x_[added_] = x;
    y_[added_] = y;
    positions_[2u * added_] = static_cast<float>(x) / half_extent - 1.0F;
    positions_[2u * added_ + 1u] = 1.0F - static_cast<float>(y) / half_extent;
    added_++;
}

void TirCoverageWork::expect()
{
    AV_SCOPE("TirCoverageWork::expect");
    for (uint32_t first = 0u; first + 3u <= added_; first += 3u) {
        const int32_t          x[3] = {x_[first], x_[first + 1u], x_[first + 2u]};
        const int32_t          y[3] = {y_[first], y_[first + 1u], y_[first + 2u]};
        const CoverageTriangle triangle(x, y);
        for (uint32_t texel = 0u; texel < texels; texel++)
            expected_[texel] |= triangle.coverage(samples, texel % extent, texel / extent);
    }
}

uint32_t TirCoverageWork::right() const
{
    AV_SCOPE("TirCoverageWork::right");
    uint32_t matching = 0u;
    for (uint32_t texel = 0u; texel < texels; texel++) matching += texels_[texel] == expected_[texel] ? 1u : 0u;
    return matching;
}

bool TirCoverageWork::invariants_hold() const
{
    AV_SCOPE("TirCoverageWork::invariants_hold");
    if (!made_ && read_) return false;
    return added_ == vertices;
}

}
