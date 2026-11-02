#include "harness/guest/scripted_window_frame.hpp"

#include <cstring>

namespace av {

ScriptedWindowFrame::ScriptedWindowFrame()
    : window_(nullptr), width_(0u), height_(0u), refuse_size_(false), limited_sizes_(false), size_limit_(0u),
      sizes_(0u), refuse_read_(false), refuse_resize_(false), reads_(0u), resizes_(0u), pixels_{}, shown_{}
{
}

void ScriptedWindowFrame::set_window(void *window, std::uint32_t width, std::uint32_t height)
{
    window_ = window;
    width_ = width;
    height_ = height;
}

void ScriptedWindowFrame::refuse_size() { refuse_size_ = true; }

void ScriptedWindowFrame::refuse_size_after(std::uint32_t count)
{
    limited_sizes_ = true;
    size_limit_ = count;
}

void ScriptedWindowFrame::refuse_read() { refuse_read_ = true; }
void ScriptedWindowFrame::refuse_resize() { refuse_resize_ = true; }

void ScriptedWindowFrame::show(const std::uint8_t bgra[4])
{
    for (std::uint32_t at = 0u; at + 4u <= width_ * height_ * 4u && at + 4u <= max_bytes; at += 4u)
        std::memcpy(&pixels_[at], bgra, 4u);
}

void ScriptedWindowFrame::show_frame(const std::uint8_t drawn[4], std::uint32_t width, std::uint32_t height,
                                     const std::uint8_t cleared[4])
{
    for (std::uint32_t row = 0u; row < height_ && (row + 1u) * width_ * 4u <= max_bytes; row++)
        for (std::uint32_t column = 0u; column < width_; column++) {
            const bool inside = height_ - row <= height && column < width;
            std::memcpy(&pixels_[(row * width_ + column) * 4u], inside ? drawn : cleared, 4u);
        }
}

void ScriptedWindowFrame::lose_the_last_pixel()
{
    const std::uint32_t last = width_ * height_ * 4u;
    if (last >= 4u && last <= max_bytes) std::memset(&pixels_[last - 4u], 0, 4u);
}

std::uint32_t ScriptedWindowFrame::reads() const { return reads_; }
std::uint32_t ScriptedWindowFrame::resizes() const { return resizes_; }

bool ScriptedWindowFrame::size(void *window, std::uint32_t &width, std::uint32_t &height)
{
    if (refuse_size_ || window != window_) return false;
    if (limited_sizes_ && sizes_ >= size_limit_) return false;
    sizes_++;
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

bool ScriptedWindowFrame::resize(void *window, std::uint32_t width, std::uint32_t height)
{
    if (refuse_resize_ || window != window_ || width > max_side || height > max_side) return false;
    resizes_++;
    std::memcpy(shown_, pixels_, sizeof shown_);
    for (std::uint32_t row = 0u; row < height; row++)
        for (std::uint32_t column = 0u; column < width; column++) {
            const std::uint32_t from = (row * height_ / height) * width_ + column * width_ / width;
            std::memcpy(&pixels_[(row * width + column) * 4u], &shown_[from * 4u], 4u);
        }
    width_ = width;
    height_ = height;
    return true;
}

}
