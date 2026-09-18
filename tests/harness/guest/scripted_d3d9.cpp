#include "harness/guest/scripted_d3d9.hpp"

#include "harness/framework/test_registry.hpp"

namespace av {

ScriptedD3d9::ScriptedD3d9()
    : calls_{}, fail_at_{}, fail_with_{}, cleared_{}, window_(nullptr), width_(0u), height_(0u), live_(false),
      cleared_once_(false), miscolour_(false)
{
}

void ScriptedD3d9::fail(Call call, std::uint32_t nth, std::int32_t result)
{
    fail_at_[static_cast<unsigned>(call)] = nth;
    fail_with_[static_cast<unsigned>(call)] = result;
}

void ScriptedD3d9::miscolour()
{
    miscolour_ = true;
}

std::uint32_t ScriptedD3d9::calls(Call call) const
{
    return calls_[static_cast<unsigned>(call)];
}

bool ScriptedD3d9::live() const
{
    return live_;
}

void *ScriptedD3d9::window() const
{
    return window_;
}

std::uint32_t ScriptedD3d9::width() const
{
    return width_;
}

std::int32_t ScriptedD3d9::refusal(Call call)
{
    const unsigned index = static_cast<unsigned>(call);
    calls_[index]++;
    if (fail_at_[index] == 0u || calls_[index] < fail_at_[index]) return 0;
    return fail_with_[index];
}

std::int32_t ScriptedD3d9::create_device(void *window, std::uint32_t width, std::uint32_t height)
{
    const std::int32_t refused = refusal(Call::create_device);
    if (refused != 0) return refused;
    window_ = window;
    width_ = width;
    height_ = height;
    live_ = true;
    return 0;
}

std::int32_t ScriptedD3d9::adapter_ids(std::uint32_t &vendor_id, std::uint32_t &device_id)
{
    const std::int32_t refused = refusal(Call::adapter_ids);
    if (refused != 0) return refused;
    CHECK(live_);
    vendor_id = vendor;
    device_id = device;
    return 0;
}

std::int32_t ScriptedD3d9::clear(const std::uint8_t *colour)
{
    const std::int32_t refused = refusal(Call::clear);
    if (refused != 0) return refused;
    CHECK(live_);
    for (unsigned channel = 0u; channel < 4u; channel++) cleared_[channel] = colour[channel];
    cleared_once_ = true;
    return 0;
}

std::int32_t ScriptedD3d9::read_back(std::uint8_t *pixels, std::uint32_t bytes)
{
    const std::int32_t refused = refusal(Call::read_back);
    if (refused != 0) return refused;
    CHECK(cleared_once_);
    for (std::uint32_t pixel = 0u; pixel * 4u < bytes; pixel++) {
        pixels[pixel * 4u] = cleared_[2];
        pixels[pixel * 4u + 1u] = cleared_[1];
        pixels[pixel * 4u + 2u] = cleared_[0];
        pixels[pixel * 4u + 3u] = cleared_[3];
    }
    if (miscolour_ && bytes >= 4u) pixels[1] = static_cast<std::uint8_t>(cleared_[1] + 8u);
    return 0;
}

std::int32_t ScriptedD3d9::present()
{
    const std::int32_t refused = refusal(Call::present);
    if (refused != 0) return refused;
    CHECK(cleared_once_);
    return 0;
}

void ScriptedD3d9::release()
{
    calls_[static_cast<unsigned>(Call::release)]++;
    live_ = false;
    cleared_once_ = false;
}

}  // namespace av
