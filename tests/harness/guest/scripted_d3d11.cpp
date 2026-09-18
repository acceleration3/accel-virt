#include "harness/guest/scripted_d3d11.hpp"

#include "harness/framework/test_registry.hpp"

namespace av {

ScriptedD3d11::ScriptedD3d11()
    : calls_{}, fail_at_{}, fail_with_{}, cleared_{}, window_(nullptr), width_(0u), height_(0u), live_(false),
      target_(false), miscolour_(false)
{
}

void ScriptedD3d11::fail(Call call, std::uint32_t nth, std::int32_t result)
{
    fail_at_[static_cast<unsigned>(call)] = nth;
    fail_with_[static_cast<unsigned>(call)] = result;
}

void ScriptedD3d11::miscolour()
{
    miscolour_ = true;
}

std::uint32_t ScriptedD3d11::calls(Call call) const
{
    return calls_[static_cast<unsigned>(call)];
}

bool ScriptedD3d11::live() const
{
    return live_;
}

std::int32_t ScriptedD3d11::refusal(Call call)
{
    const unsigned index = static_cast<unsigned>(call);
    calls_[index]++;
    if (fail_at_[index] == 0u || calls_[index] < fail_at_[index]) return 0;
    return fail_with_[index];
}

std::int32_t ScriptedD3d11::create_device(void *window, std::uint32_t width, std::uint32_t height,
                                          std::uint32_t &feature_level)
{
    const std::int32_t refused = refusal(Call::create_device);
    if (refused != 0) return refused;
    window_ = window;
    width_ = width;
    height_ = height;
    live_ = true;
    feature_level = feature_level_11_0;
    return 0;
}

std::int32_t ScriptedD3d11::adapter_luid(std::uint64_t &luid)
{
    const std::int32_t refused = refusal(Call::adapter_luid);
    if (refused != 0) return refused;
    CHECK(live_);
    luid = adapter;
    return 0;
}

std::int32_t ScriptedD3d11::create_render_target()
{
    const std::int32_t refused = refusal(Call::create_render_target);
    if (refused != 0) return refused;
    CHECK(live_);
    target_ = true;
    return 0;
}

void ScriptedD3d11::clear(const std::uint8_t *colour)
{
    calls_[static_cast<unsigned>(Call::clear)]++;
    CHECK(target_);
    for (unsigned channel = 0u; channel < 4u; channel++) cleared_[channel] = colour[channel];
}

std::int32_t ScriptedD3d11::read_back(std::uint8_t *pixels, std::uint32_t bytes)
{
    const std::int32_t refused = refusal(Call::read_back);
    if (refused != 0) return refused;
    CHECK(target_);
    /* What a staging texture of the back buffer holds: BGRA rows. */
    for (std::uint32_t pixel = 0u; pixel * 4u < bytes; pixel++) {
        pixels[pixel * 4u] = cleared_[2];
        pixels[pixel * 4u + 1u] = cleared_[1];
        pixels[pixel * 4u + 2u] = cleared_[0];
        pixels[pixel * 4u + 3u] = cleared_[3];
    }
    if (miscolour_ && bytes >= 4u) pixels[0] = static_cast<std::uint8_t>(cleared_[2] + 8u);
    return 0;
}

std::int32_t ScriptedD3d11::present()
{
    const std::int32_t refused = refusal(Call::present);
    if (refused != 0) return refused;
    CHECK(target_);
    return 0;
}

void ScriptedD3d11::release()
{
    calls_[static_cast<unsigned>(Call::release)]++;
    live_ = false;
    target_ = false;
}

void *ScriptedD3d11::window() const
{
    return window_;
}

std::uint32_t ScriptedD3d11::width() const
{
    return width_;
}

}  // namespace av
