#include "harness/guest/scripted_overlay.hpp"

namespace av {

ScriptedOverlay::ScriptedOverlay()
    : calls_{}, fail_at_{}, fail_with_{}, overlays_(true), support_(1u), mode_(1u), live_(false), x_(-1), width_(0u)
{}

void ScriptedOverlay::fail(Call call, std::uint32_t nth, std::int32_t result)
{
    fail_at_[static_cast<unsigned>(call)] = nth;
    fail_with_[static_cast<unsigned>(call)] = result;
}

void ScriptedOverlay::answer(bool overlays, std::uint32_t support, std::uint32_t mode)
{
    overlays_ = overlays;
    support_ = support;
    mode_ = mode;
}

std::uint32_t ScriptedOverlay::calls(Call call) const { return calls_[static_cast<unsigned>(call)]; }
bool          ScriptedOverlay::live() const { return live_; }
std::int32_t  ScriptedOverlay::x() const { return x_; }
std::uint32_t ScriptedOverlay::width() const { return width_; }

std::int32_t ScriptedOverlay::refusal(Call call)
{
    const unsigned index = static_cast<unsigned>(call);
    calls_[index]++;
    return fail_at_[index] != 0u && calls_[index] >= fail_at_[index] ? fail_with_[index] : 0;
}

std::int32_t ScriptedOverlay::create_device()
{
    const std::int32_t refused = refusal(Call::create_device);
    if (refused == 0) live_ = true;
    return refused;
}

std::int32_t ScriptedOverlay::supports_overlays(bool &supported)
{
    supported = overlays_;
    return refusal(Call::supports_overlays);
}

std::int32_t ScriptedOverlay::overlay_support(std::uint32_t &flags)
{
    flags = support_;
    return refusal(Call::overlay_support);
}

std::int32_t ScriptedOverlay::create_swap_chain(std::int32_t x, std::int32_t, std::uint32_t width, std::uint32_t)
{
    x_ = x;
    width_ = width;
    return refusal(Call::create_swap_chain);
}

std::int32_t ScriptedOverlay::present(const std::uint8_t *) { return refusal(Call::present); }

std::int32_t ScriptedOverlay::composition(std::uint32_t &mode)
{
    mode = mode_;
    return refusal(Call::composition);
}

void ScriptedOverlay::release()
{
    calls_[static_cast<unsigned>(Call::release)]++;
    live_ = false;
}

}  // namespace av
