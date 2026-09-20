#include "device/display/scanout_framebuffer.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/virtio_gpu/set_scanout_blob_request.hpp"

namespace av {

ScanoutFramebuffer::ScanoutFramebuffer()
    : format_(0u), width_(0u), height_(0u), stride_(0u), offset_(0u), blob_size_(0u), adopted_(false)
{
    AV_SCOPE("ScanoutFramebuffer::ScanoutFramebuffer");
}

Status ScanoutFramebuffer::adopt(const SetScanoutBlobRequest &request, uint64_t blob_size)
{
    AV_SCOPE("ScanoutFramebuffer::adopt");
    forget();
    const ScanoutFormat format(request.format());
    if (!format.scannable()) return Status::invalid_parameter;
    if (request.width() == 0u || request.height() == 0u) return Status::invalid_parameter;
    if (request.stride() > max_stride) return Status::invalid_parameter;
    const uint64_t row = static_cast<uint64_t>(request.width()) * format.pixel_bytes();
    if (request.stride() < row) return Status::invalid_parameter;
    const uint64_t last_row =
        static_cast<uint64_t>(request.offset()) + static_cast<uint64_t>(request.height() - 1u) * request.stride();
    if (last_row + row > blob_size) return Status::invalid_parameter;
    if (!request.rect().inside(request.width(), request.height())) return Status::invalid_parameter;
    format_ = format;
    rect_ = request.rect();
    width_ = request.width();
    height_ = request.height();
    stride_ = request.stride();
    offset_ = request.offset();
    blob_size_ = blob_size;
    adopted_ = true;
    return Status::ok;
}

void ScanoutFramebuffer::forget()
{
    AV_SCOPE("ScanoutFramebuffer::forget");
    format_ = ScanoutFormat(0u);
    rect_ = GpuRect();
    width_ = 0u;
    height_ = 0u;
    stride_ = 0u;
    offset_ = 0u;
    blob_size_ = 0u;
    adopted_ = false;
}

bool ScanoutFramebuffer::adopted() const
{
    AV_SCOPE("ScanoutFramebuffer::adopted");
    return adopted_;
}

const GpuRect &ScanoutFramebuffer::rect() const
{
    AV_SCOPE("ScanoutFramebuffer::rect");
    return rect_;
}

uint32_t ScanoutFramebuffer::width() const
{
    AV_SCOPE("ScanoutFramebuffer::width");
    return width_;
}

uint32_t ScanoutFramebuffer::height() const
{
    AV_SCOPE("ScanoutFramebuffer::height");
    return height_;
}

uint32_t ScanoutFramebuffer::stride() const
{
    AV_SCOPE("ScanoutFramebuffer::stride");
    return stride_;
}

uint32_t ScanoutFramebuffer::offset() const
{
    AV_SCOPE("ScanoutFramebuffer::offset");
    return offset_;
}

uint32_t ScanoutFramebuffer::fourcc() const
{
    AV_SCOPE("ScanoutFramebuffer::fourcc");
    return format_.fourcc();
}

uint64_t ScanoutFramebuffer::end() const
{
    AV_SCOPE("ScanoutFramebuffer::end");
    if (!adopted_) return 0u;
    return static_cast<uint64_t>(offset_) + static_cast<uint64_t>(height_ - 1u) * stride_ +
           static_cast<uint64_t>(width_) * format_.pixel_bytes();
}

uint64_t ScanoutFramebuffer::blob_size() const
{
    AV_SCOPE("ScanoutFramebuffer::blob_size");
    return blob_size_;
}

bool ScanoutFramebuffer::invariants_hold() const
{
    AV_SCOPE("ScanoutFramebuffer::invariants_hold");
    if (!format_.invariants_hold() || !rect_.invariants_hold()) return false;
    if (!adopted_)
        return width_ == 0u && height_ == 0u && stride_ == 0u && offset_ == 0u && blob_size_ == 0u &&
               !format_.scannable();
    if (!format_.scannable() || width_ == 0u || height_ == 0u) return false;
    if (stride_ > max_stride || stride_ < static_cast<uint64_t>(width_) * format_.pixel_bytes()) return false;
    if (end() > blob_size_) return false;
    return rect_.inside(width_, height_);
}

}  // namespace av
