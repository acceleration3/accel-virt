#include "harness/guest/scripted_window_frame.hpp"

#include <cstring>

namespace av {

ScriptedWindowFrame::ScriptedWindowFrame()
    : window_(nullptr), width_(0u), height_(0u), refuse_size_(false), refuse_read_(false), refuse_resize_(false),
      reads_(0u), resizes_(0u), pixels_{}
{
}

void ScriptedWindowFrame::set_window(void *window, std::uint32_t width, std::uint32_t height)
{
    window_ = window;
    width_ = width;
    height_ = height;
}

void ScriptedWindowFrame::refuse_size() { refuse_size_ = true; }
void ScriptedWindowFrame::refuse_read() { refuse_read_ = true; }
void ScriptedWindowFrame::refuse_resize() { refuse_resize_ = true; }

void ScriptedWindowFrame::show(const std::uint8_t bgra[4])
{
    for (std::uint32_t at = 0u; at + 4u <= width_ * height_ * 4u && at + 4u <= max_bytes; at += 4u)
        std::memcpy(&pixels_[at], bgra, 4u);
}

void ScriptedWindowFrame::show_but_the_last(const std::uint8_t bgra[4])
{
    show(bgra);
    const std::uint32_t last = width_ * height_ * 4u;
    if (last >= 4u && last <= max_bytes) std::memset(&pixels_[last - 4u], 0, 4u);
}

std::uint32_t ScriptedWindowFrame::reads() const { return reads_; }
std::uint32_t ScriptedWindowFrame::resizes() const { return resizes_; }

bool ScriptedWindowFrame::size(void *window, std::uint32_t &width, std::uint32_t &height)
{
    if (refuse_size_ || window != window_) return false;
    width = width_;
    height = height_;
    return true;
}

bool ScriptedWindowFrame::read(void *window, std::uint32_t width, std::uint32_t height, std::uint8_t *bgra)
{
    if (refuse_read_ || window != window_ || width != width_ || height != height_) return false;
    reads_++;
    std::memcpy(bgra, pixels_, width * height * 4u);
    return true;
}

/* A resized window keeps nothing of what it showed: the desktop repaints it. */
bool ScriptedWindowFrame::resize(void *window, std::uint32_t width, std::uint32_t height)
{
    if (refuse_resize_ || window != window_ || width > max_side || height > max_side) return false;
    resizes_++;
    width_ = width;
    height_ = height;
    std::memset(pixels_, 0, sizeof pixels_);
    return true;
}

}  // namespace av
