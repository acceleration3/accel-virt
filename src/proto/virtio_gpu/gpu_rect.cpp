#include "proto/virtio_gpu/gpu_rect.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/wire/wire_reader.hpp"

namespace av {

GpuRect::GpuRect() : x_(0u), y_(0u), width_(0u), height_(0u), decoded_(false)
{
    AV_SCOPE("GpuRect::GpuRect");
}

Status GpuRect::decode(WireReader &reader)
{
    AV_SCOPE("GpuRect::decode");
    if (reader.remaining() < encoded_size) return Status::malformed;
    x_ = reader.read_u32();
    y_ = reader.read_u32();
    width_ = reader.read_u32();
    height_ = reader.read_u32();
    decoded_ = true;
    return Status::ok;
}

bool GpuRect::inside(uint32_t width, uint32_t height) const
{
    AV_SCOPE("GpuRect::inside");
    if (width_ == 0u || height_ == 0u) return false;
    const uint64_t right = static_cast<uint64_t>(x_) + width_;
    const uint64_t bottom = static_cast<uint64_t>(y_) + height_;
    return right <= width && bottom <= height;
}

uint32_t GpuRect::x() const
{
    AV_SCOPE("GpuRect::x");
    return x_;
}

uint32_t GpuRect::y() const
{
    AV_SCOPE("GpuRect::y");
    return y_;
}

uint32_t GpuRect::width() const
{
    AV_SCOPE("GpuRect::width");
    return width_;
}

uint32_t GpuRect::height() const
{
    AV_SCOPE("GpuRect::height");
    return height_;
}

bool GpuRect::decoded() const
{
    AV_SCOPE("GpuRect::decoded");
    return decoded_;
}

}  // namespace av
