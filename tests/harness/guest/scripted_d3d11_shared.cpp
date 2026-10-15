#include "harness/guest/scripted_d3d11_shared.hpp"

#include "harness/framework/test_registry.hpp"

namespace av {

ScriptedD3d11Shared::ScriptedD3d11Shared()
    : calls_{}, fail_at_{}, fail_with_{}, targets_{}, lose_below_(0u), last_x_(0u), last_y_(0u), misplace_(false),
      lost_(false), live_(false)
{
}

void ScriptedD3d11Shared::fail(Call call, std::uint32_t nth, std::int32_t result)
{
    fail_at_[static_cast<unsigned>(call)] = nth;
    fail_with_[static_cast<unsigned>(call)] = result;
}

void ScriptedD3d11Shared::lose_device_below(std::uint32_t bytes)
{
    lose_below_ = bytes;
}

void ScriptedD3d11Shared::misplace_rows()
{
    misplace_ = true;
}

std::uint32_t ScriptedD3d11Shared::calls(Call call) const
{
    return calls_[static_cast<unsigned>(call)];
}

bool ScriptedD3d11Shared::live() const
{
    return live_;
}

std::uint32_t ScriptedD3d11Shared::width(std::uint32_t target) const
{
    return target < most_targets ? targets_[target].width : 0u;
}

std::uint32_t ScriptedD3d11Shared::height(std::uint32_t target) const
{
    return target < most_targets ? targets_[target].height : 0u;
}

std::uint32_t ScriptedD3d11Shared::last_x() const
{
    return last_x_;
}

std::uint32_t ScriptedD3d11Shared::last_y() const
{
    return last_y_;
}

std::int32_t ScriptedD3d11Shared::refusal(Call call)
{
    const unsigned index = static_cast<unsigned>(call);
    calls_[index]++;
    if (fail_at_[index] == 0u || calls_[index] < fail_at_[index]) return 0;
    return fail_with_[index];
}

std::int32_t ScriptedD3d11Shared::create_device()
{
    const std::int32_t refused = refusal(Call::create_device);
    if (refused != 0) return refused;
    live_ = true;
    return 0;
}

std::int32_t ScriptedD3d11Shared::create_target(std::uint32_t target, std::uint32_t width, std::uint32_t height)
{
    const std::int32_t refused = refusal(Call::create_target);
    if (refused != 0) return refused;
    CHECK(live_);
    CHECK(target < most_targets);
    CHECK(width != 0u && height != 0u);
    if (target < most_targets) targets_[target] = Target{width, height, {}};
    return 0;
}

void ScriptedD3d11Shared::clear(std::uint32_t target, const std::uint8_t *colour)
{
    static_cast<void>(refusal(Call::clear));
    CHECK(target < most_targets && targets_[target].width != 0u);
    if (target >= most_targets) return;
    for (std::uint32_t channel = 0u; channel < 4u; channel++) targets_[target].colour[channel] = colour[channel];
    if (lose_below_ != 0u && targets_[target].width * 4u % lose_below_ != 0u) lost_ = true;
}

std::int32_t ScriptedD3d11Shared::read_texel(std::uint32_t target, std::uint32_t x, std::uint32_t y,
                                             std::uint8_t *texel)
{
    const std::int32_t refused = refusal(Call::read_texel);
    if (refused != 0) return refused;
    if (lost_) return device_removed;
    CHECK(target < most_targets);
    CHECK(x < targets_[target].width && y < targets_[target].height);
    last_x_ = x;
    last_y_ = y;
    const std::uint8_t *const colour = targets_[target].colour;
    const bool past_the_rows = misplace_ && (x + 1u == targets_[target].width) && y != 0u;
    texel[0] = past_the_rows ? 0u : colour[2];
    texel[1] = past_the_rows ? 0u : colour[1];
    texel[2] = past_the_rows ? 0u : colour[0];
    texel[3] = past_the_rows ? 0u : colour[3];
    return 0;
}

std::int32_t ScriptedD3d11Shared::removed_reason()
{
    const std::int32_t refused = refusal(Call::removed_reason);
    if (refused != 0) return refused;
    return lost_ ? device_hung : 0;
}

void ScriptedD3d11Shared::release()
{
    static_cast<void>(refusal(Call::release));
    live_ = false;
}

}  // namespace av
