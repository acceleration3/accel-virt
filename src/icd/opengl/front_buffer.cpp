#include "icd/opengl/front_buffer.hpp"

#include "core/support/bgra_rows.hpp"
#include "core/trace/function_scope.hpp"

namespace av::glicd {

FrontBuffer::FrontBuffer() : pixels_(nullptr), width_(0u), height_(0u)
{
    AV_SCOPE("FrontBuffer::FrontBuffer");
}

void FrontBuffer::presented(const uint8_t *bgra_bottom_up, uint32_t width, uint32_t height)
{
    AV_SCOPE("FrontBuffer::presented");
    if (bgra_bottom_up == nullptr || width == 0u || height == 0u) {
        forget();
        return;
    }
    pixels_ = bgra_bottom_up;
    width_ = width;
    height_ = height;
}

void FrontBuffer::forget()
{
    AV_SCOPE("FrontBuffer::forget");
    pixels_ = nullptr;
    width_ = 0u;
    height_ = 0u;
}

bool FrontBuffer::presented() const
{
    AV_SCOPE("FrontBuffer::presented");
    return pixels_ != nullptr;
}

uint32_t FrontBuffer::width() const
{
    AV_SCOPE("FrontBuffer::width");
    return width_;
}

uint32_t FrontBuffer::height() const
{
    AV_SCOPE("FrontBuffer::height");
    return height_;
}

bool FrontBuffer::read(int32_t x, int32_t y, int32_t width, int32_t height, uint32_t format, uint32_t type,
                       void *pixels) const
{
    AV_SCOPE("FrontBuffer::read");
    if (pixels_ == nullptr || pixels == nullptr) return false;
    if (type != gl_unsigned_byte || (format != gl_rgba && format != gl_bgra)) return false;
    if (x < 0 || y < 0 || width <= 0 || height <= 0) return false;
    const uint64_t right = static_cast<uint64_t>(x) + static_cast<uint64_t>(width);
    const uint64_t top = static_cast<uint64_t>(y) + static_cast<uint64_t>(height);
    if (right > width_ || top > height_) return false;
    const uint64_t pitch = uint64_t{width_} * bytes_per_pixel;
    uint64_t frame_bytes = 0u;
    if (__builtin_mul_overflow(pitch, uint64_t{height_}, &frame_bytes) || frame_bytes > SIZE_MAX) return false;
    const uint64_t offset = static_cast<uint64_t>(y) * pitch + static_cast<uint64_t>(x) * bytes_per_pixel;
    const BgraRows from(pixels_ + static_cast<size_t>(offset), frame_bytes - offset, pitch);
    const uint64_t line = static_cast<uint64_t>(width) * bytes_per_pixel;
    BgraRows to(static_cast<uint8_t *>(pixels), line * static_cast<uint32_t>(height), line);
    return to.copy_from(from, static_cast<uint32_t>(width), static_cast<uint32_t>(height),
                        format == gl_rgba ? BgraRows::Channels::red_blue_swapped : BgraRows::Channels::unchanged) ==
           Status::ok;
}

bool FrontBuffer::invariants_hold() const
{
    AV_SCOPE("FrontBuffer::invariants_hold");
    /* An extent without pixels is a frame nobody could read. */
    return (pixels_ == nullptr) == (width_ == 0u);
}

}  // namespace av::glicd
