#include "harness/guest/scripted_d3d9ex.hpp"

#include "harness/framework/test_registry.hpp"
#include "harness/guest/scripted_window_frame.hpp"

namespace av {

ScriptedD3d9Ex::ScriptedD3d9Ex(ScriptedWindowFrame &window)
    : window_frame_(window), calls_{}, fail_at_{}, fail_with_{}, drawn_{}, window_(nullptr), width_(0u),
      height_(0u), blanks_to_arrival_(0u), lost_(false), pending_(false), live_(false)
{
}

void ScriptedD3d9Ex::fail(Call call, std::uint32_t nth, std::int32_t result)
{
    fail_at_[static_cast<unsigned>(call)] = nth;
    fail_with_[static_cast<unsigned>(call)] = result;
}

void ScriptedD3d9Ex::arrive_after(std::uint32_t count)
{
    blanks_to_arrival_ = count;
}

void ScriptedD3d9Ex::lose_the_frame()
{
    lost_ = true;
}

std::uint32_t ScriptedD3d9Ex::calls(Call call) const
{
    return calls_[static_cast<unsigned>(call)];
}

bool ScriptedD3d9Ex::live() const
{
    return live_;
}

void *ScriptedD3d9Ex::window() const
{
    return window_;
}

std::uint32_t ScriptedD3d9Ex::width() const
{
    return width_;
}

std::uint32_t ScriptedD3d9Ex::height() const
{
    return height_;
}

std::int32_t ScriptedD3d9Ex::refusal(Call call)
{
    const unsigned index = static_cast<unsigned>(call);
    calls_[index]++;
    if (fail_at_[index] == 0u || calls_[index] < fail_at_[index]) return 0;
    return fail_with_[index];
}

std::int32_t ScriptedD3d9Ex::create_device(void *window)
{
    const std::int32_t refused = refusal(Call::create_device);
    if (refused != 0) return refused;
    window_ = window;
    live_ = true;
    return 0;
}

std::int32_t ScriptedD3d9Ex::create_swap_chain(void *window, std::uint32_t width, std::uint32_t height)
{
    const std::int32_t refused = refusal(Call::create_swap_chain);
    if (refused != 0) return refused;
    CHECK(live_);
    CHECK(window == window_);
    width_ = width;
    height_ = height;
    return 0;
}

std::int32_t ScriptedD3d9Ex::draw(const std::uint8_t *colour)
{
    const std::int32_t refused = refusal(Call::draw);
    if (refused != 0) return refused;
    CHECK(width_ != 0u);
    for (unsigned channel = 0u; channel < 4u; channel++) drawn_[channel] = colour[channel];
    return 0;
}

std::int32_t ScriptedD3d9Ex::present(std::uint32_t width, std::uint32_t height)
{
    const std::int32_t refused = refusal(Call::present);
    if (refused != 0) return refused;
    CHECK_EQ(width, width_);
    CHECK_EQ(height, height_);
    pending_ = !lost_;
    if (blanks_to_arrival_ == 0u) arrive();
    return 0;
}

std::int32_t ScriptedD3d9Ex::wait_for_blank()
{
    const std::int32_t refused = refusal(Call::wait_for_blank);
    if (refused != 0) return refused;
    if (blanks_to_arrival_ != 0u && --blanks_to_arrival_ == 0u) arrive();
    return 0;
}

void ScriptedD3d9Ex::release()
{
    calls_[static_cast<unsigned>(Call::release)]++;
    live_ = false;
}

/* What a window's DIB holds of the frame: blue, green, red, and the
 * desktop's own alpha. */
void ScriptedD3d9Ex::arrive()
{
    if (!pending_) return;
    pending_ = false;
    const std::uint8_t bgra[4] = {drawn_[2], drawn_[1], drawn_[0], 0xFFu};
    window_frame_.show(bgra);
}

}  // namespace av
