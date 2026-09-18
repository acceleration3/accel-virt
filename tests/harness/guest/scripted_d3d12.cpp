#include "harness/guest/scripted_d3d12.hpp"

#include "harness/framework/test_registry.hpp"

namespace {

/* No channel: what a frame that comes back whole is asked for. */
constexpr unsigned no_channel = 4u;

}  // namespace

namespace av {

ScriptedD3d12::ScriptedD3d12()
    : calls_{}, fail_at_{}, fail_with_{}, cleared_{}, width_(0u), height_(0u), live_(false), queue_(false),
      target_(false), copied_(false), waited_(false), miscoloured_(no_channel)
{
}

void ScriptedD3d12::fail(Call call, std::uint32_t nth, std::int32_t result)
{
    fail_at_[static_cast<unsigned>(call)] = nth;
    fail_with_[static_cast<unsigned>(call)] = result;
}

void ScriptedD3d12::miscolour(unsigned channel)
{
    miscoloured_ = channel;
}

std::uint32_t ScriptedD3d12::calls(Call call) const
{
    return calls_[static_cast<unsigned>(call)];
}

bool ScriptedD3d12::live() const
{
    return live_;
}

std::uint32_t ScriptedD3d12::width() const
{
    return width_;
}

std::int32_t ScriptedD3d12::refusal(Call call)
{
    const unsigned index = static_cast<unsigned>(call);
    calls_[index]++;
    if (fail_at_[index] == 0u || calls_[index] < fail_at_[index]) return 0;
    return fail_with_[index];
}

std::int32_t ScriptedD3d12::create_device(std::uint32_t &feature_level)
{
    const std::int32_t refused = refusal(Call::create_device);
    if (refused != 0) return refused;
    live_ = true;
    feature_level = feature_level_12_0;
    return 0;
}

std::int32_t ScriptedD3d12::adapter_luid(std::uint64_t &luid)
{
    const std::int32_t refused = refusal(Call::adapter_luid);
    if (refused != 0) return refused;
    CHECK(live_);
    luid = adapter;
    return 0;
}

std::int32_t ScriptedD3d12::create_queue()
{
    const std::int32_t refused = refusal(Call::create_queue);
    if (refused != 0) return refused;
    CHECK(live_);
    queue_ = true;
    return 0;
}

std::int32_t ScriptedD3d12::create_render_target(std::uint32_t width, std::uint32_t height)
{
    const std::int32_t refused = refusal(Call::create_render_target);
    if (refused != 0) return refused;
    CHECK(queue_);
    width_ = width;
    height_ = height;
    target_ = true;
    return 0;
}

std::int32_t ScriptedD3d12::clear_and_copy(const std::uint8_t *colour)
{
    const std::int32_t refused = refusal(Call::clear_and_copy);
    if (refused != 0) return refused;
    CHECK(target_);
    for (unsigned channel = 0u; channel < 4u; channel++) cleared_[channel] = colour[channel];
    copied_ = true;
    return 0;
}

std::int32_t ScriptedD3d12::wait()
{
    const std::int32_t refused = refusal(Call::wait);
    if (refused != 0) return refused;
    CHECK(copied_);
    waited_ = true;
    return 0;
}

std::int32_t ScriptedD3d12::read_back(std::uint8_t *pixels, std::uint32_t bytes)
{
    const std::int32_t refused = refusal(Call::read_back);
    if (refused != 0) return refused;
    /* Read before the fence was reached would be a race a test must not
     * pass. */
    CHECK(waited_);
    for (std::uint32_t pixel = 0u; pixel * 4u < bytes; pixel++)
        for (unsigned channel = 0u; channel < 4u; channel++)
            pixels[pixel * 4u + channel] = cleared_[channel];
    if (miscoloured_ < 4u && bytes >= 4u)
        pixels[miscoloured_] = static_cast<std::uint8_t>(pixels[miscoloured_] + 8u);
    return 0;
}

void ScriptedD3d12::release()
{
    calls_[static_cast<unsigned>(Call::release)]++;
    live_ = false;
    queue_ = false;
    target_ = false;
    copied_ = false;
    waited_ = false;
}

}  // namespace av
