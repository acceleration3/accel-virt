#include "harness/guest/scripted_d3d11_tir.hpp"

#include "guest/probe/coverage_triangle.hpp"
#include "harness/framework/test_registry.hpp"

namespace av {

ScriptedD3d11Tir::ScriptedD3d11Tir()
    : calls_{}, fail_at_{}, fail_with_{}, texels_{}, extent_(0u), samples_(0u), vertices_(0u), one_sample_(false),
      layered_(false), lost_(false), live_(false)
{
}

void ScriptedD3d11Tir::fail(Call call, std::uint32_t nth, std::int32_t result)
{
    fail_at_[static_cast<unsigned>(call)] = nth;
    fail_with_[static_cast<unsigned>(call)] = result;
}

void ScriptedD3d11Tir::rasterize_at_one_sample()
{
    one_sample_ = true;
}

void ScriptedD3d11Tir::lose()
{
    lost_ = true;
}

void ScriptedD3d11Tir::set_layered(bool layered)
{
    layered_ = layered;
}

std::uint32_t ScriptedD3d11Tir::calls(Call call) const
{
    return calls_[static_cast<unsigned>(call)];
}

bool ScriptedD3d11Tir::live() const
{
    return live_;
}

std::uint32_t ScriptedD3d11Tir::extent() const
{
    return extent_;
}

std::uint32_t ScriptedD3d11Tir::samples() const
{
    return samples_;
}

std::uint32_t ScriptedD3d11Tir::vertices() const
{
    return vertices_;
}

std::int32_t ScriptedD3d11Tir::refusal(Call call)
{
    const unsigned index = static_cast<unsigned>(call);
    calls_[index]++;
    if (fail_at_[index] == 0u || calls_[index] < fail_at_[index]) return 0;
    return fail_with_[index];
}

std::int32_t ScriptedD3d11Tir::create_device()
{
    const std::int32_t refused = refusal(Call::create_device);
    if (refused != 0) return refused;
    live_ = true;
    return 0;
}

std::int32_t ScriptedD3d11Tir::create_pipeline(std::uint32_t extent, std::uint32_t samples)
{
    const std::int32_t refused = refusal(Call::create_pipeline);
    if (refused != 0) return refused;
    CHECK(live_);
    CHECK(extent != 0u && extent <= most_extent);
    extent_ = extent;
    samples_ = samples;
    return 0;
}

/* Clip space back to subpixels: exact for what the scenario sends. */
std::int32_t ScriptedD3d11Tir::draw(const float *positions, std::uint32_t count)
{
    const std::int32_t refused = refusal(Call::draw);
    if (refused != 0) return refused;
    CHECK(extent_ != 0u && count % 3u == 0u);
    vertices_ = count;
    for (std::uint32_t &texel : texels_) texel = 0u;
    const double half = static_cast<double>(CoverageTriangle::subpixels) * extent_ / 2.0;
    for (std::uint32_t first = 0u; first + 3u <= count; first += 3u) {
        std::int32_t x[3] = {}, y[3] = {};
        for (std::uint32_t vertex = 0u; vertex < 3u; vertex++) {
            x[vertex] = static_cast<std::int32_t>((positions[2u * (first + vertex)] + 1.0) * half);
            y[vertex] = static_cast<std::int32_t>((1.0 - positions[2u * (first + vertex) + 1u]) * half);
        }
        const CoverageTriangle triangle(x, y);
        for (std::uint32_t texel = 0u; texel < extent_ * extent_; texel++) {
            const std::uint32_t mask = triangle.coverage(samples_, texel % extent_, texel / extent_);
            texels_[texel] |= one_sample_ ? (mask != 0u ? 1u : 0u) : mask;
        }
    }
    return 0;
}

std::int32_t ScriptedD3d11Tir::read_back(std::uint32_t *texels, std::uint32_t count)
{
    const std::int32_t refused = refusal(Call::read_back);
    if (refused != 0) return refused;
    CHECK_EQ(count, extent_ * extent_);
    for (std::uint32_t texel = 0u; texel < count && texel < most_extent * most_extent; texel++)
        texels[texel] = texels_[texel];
    return 0;
}

std::int32_t ScriptedD3d11Tir::removed_reason()
{
    const std::int32_t refused = refusal(Call::removed_reason);
    if (refused != 0) return refused;
    return lost_ ? device_hung : 0;
}

bool ScriptedD3d11Tir::layered()
{
    return layered_;
}

void ScriptedD3d11Tir::release()
{
    static_cast<void>(refusal(Call::release));
    live_ = false;
}

}  // namespace av
