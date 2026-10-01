#include "device/shim/d3d11_frame_pool.hpp"

#include "core/trace/function_scope.hpp"
#include "device/shim/d3d11_host_device.hpp"
#include "device/shim/d3d_headers.hpp"

namespace av {

D3d11FramePool::D3d11FramePool(D3d11HostDevice &device) : frames_()
{
    AV_SCOPE("D3d11FramePool::D3d11FramePool");
    frames_.each([&](Frame &frame) { frame.bind(device); });
}

D3d11FramePool::~D3d11FramePool()
{
    AV_SCOPE("D3d11FramePool::~D3d11FramePool");
}

StagedFrame *D3d11FramePool::stage(ID3D11Texture2D *source, uint32_t subresource, uint32_t width, uint32_t height,
                                   uint32_t format)
{
    AV_SCOPE("D3d11FramePool::stage");
    Frame *const chosen = frames_.claim([&](const Frame &frame) { return frame.fits(width, height, format); });
    if (chosen == nullptr) return nullptr;
    if (!chosen->take(source, subresource, width, height, format)) {
        chosen->release();
        return nullptr;
    }
    return chosen;
}

void D3d11FramePool::clear()
{
    AV_SCOPE("D3d11FramePool::clear");
    frames_.each([](Frame &frame) { frame.drop(); });
}

D3d11FramePool::Frame::Frame()
    : device_(nullptr), texture_(nullptr), width_(0u), height_(0u), format_(0u), mapped_(false), claim_()
{
    AV_SCOPE("D3d11FramePool::Frame::Frame");
}

D3d11FramePool::Frame::~Frame()
{
    AV_SCOPE("D3d11FramePool::Frame::~Frame");
    if (texture_ != nullptr) texture_->Release();
}

void D3d11FramePool::Frame::bind(D3d11HostDevice &device)
{
    AV_SCOPE("D3d11FramePool::Frame::bind");
    device_ = &device;
}

bool D3d11FramePool::Frame::out() const
{
    AV_SCOPE("D3d11FramePool::Frame::out");
    return claim_.out();
}

bool D3d11FramePool::Frame::claim()
{
    AV_SCOPE("D3d11FramePool::Frame::claim");
    return claim_.claim();
}

bool D3d11FramePool::Frame::fits(uint32_t width, uint32_t height, uint32_t format) const
{
    AV_SCOPE("D3d11FramePool::Frame::fits");
    return texture_ != nullptr && width_ == width && height_ == height && format_ == format;
}

bool D3d11FramePool::Frame::take(ID3D11Texture2D *source, uint32_t subresource, uint32_t width, uint32_t height,
                                 uint32_t format)
{
    AV_SCOPE("D3d11FramePool::Frame::take");
    if (!fits(width, height, format)) {
        drop();
        const D3D11_TEXTURE2D_DESC staging = {width, height, 1u, 1u, static_cast<DXGI_FORMAT>(format), {1u, 0u},
                                              D3D11_USAGE_STAGING, 0u, D3D11_CPU_ACCESS_READ, 0u};
        if (FAILED(device_->device()->CreateTexture2D(&staging, nullptr, &texture_))) {
            texture_ = nullptr;
            return false;
        }
        width_ = width;
        height_ = height;
        format_ = format;
    }
    device_->context()->CopySubresourceRegion(texture_, 0u, 0u, 0u, 0u, source, subresource, nullptr);
    device_->context()->Flush();
    return true;
}

void D3d11FramePool::Frame::drop()
{
    AV_SCOPE("D3d11FramePool::Frame::drop");
    if (texture_ != nullptr && mapped_) device_->context()->Unmap(texture_, 0u);
    if (texture_ != nullptr) texture_->Release();
    texture_ = nullptr;
    mapped_ = false;
    width_ = height_ = format_ = 0u;
}

StagedFrame::Reading D3d11FramePool::Frame::read(AvFrameView &view)
{
    AV_SCOPE("D3d11FramePool::Frame::read");
    device_->lock(nullptr);
    D3D11_MAPPED_SUBRESOURCE mapped = {};
    const HRESULT result = device_->context()->Map(texture_, 0u, D3D11_MAP_READ, D3D11_MAP_FLAG_DO_NOT_WAIT, &mapped);
    if (SUCCEEDED(result)) mapped_ = true;
    device_->unlock();
    if (result == DXGI_ERROR_WAS_STILL_DRAWING) return Reading::copying;
    if (FAILED(result)) return Reading::failed;
    view.pixels = static_cast<const uint8_t *>(mapped.pData);
    view.row_pitch = mapped.RowPitch;
    view.width = width_;
    view.height = height_;
    view.reserved = 0u;
    return Reading::mapped;
}

void D3d11FramePool::Frame::release()
{
    AV_SCOPE("D3d11FramePool::Frame::release");
    if (mapped_) {
        device_->lock(nullptr);
        device_->context()->Unmap(texture_, 0u);
        mapped_ = false;
        device_->unlock();
    }
    claim_.give_back();
}

}  // namespace av
