#include "executor/shim/d3d11_frame_pool.hpp"

#include "core/trace/function_scope.hpp"
#include "executor/shim/d3d11_host_device.hpp"
#include "executor/shim/d3d_headers.hpp"

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
    : device_(nullptr), held_(nullptr), texture_(nullptr), staged_(false), format_(0u), mapped_(false)
{
    AV_SCOPE("D3d11FramePool::Frame::Frame");
}

D3d11FramePool::Frame::~Frame()
{
    AV_SCOPE("D3d11FramePool::Frame::~Frame");
    if (held_ != nullptr) held_->Release();
    if (texture_ != nullptr) texture_->Release();
}

void D3d11FramePool::Frame::bind(D3d11HostDevice &device)
{
    AV_SCOPE("D3d11FramePool::Frame::bind");
    device_ = &device;
}

bool D3d11FramePool::Frame::fits(uint32_t width, uint32_t height, uint32_t format) const
{
    AV_SCOPE("D3d11FramePool::Frame::fits");
    return held_ != nullptr && this->width() == width && this->height() == height && format_ == format;
}

bool D3d11FramePool::Frame::take(ID3D11Texture2D *source, uint32_t subresource, uint32_t width, uint32_t height,
                                 uint32_t format)
{
    AV_SCOPE("D3d11FramePool::Frame::take");
    if (!fits(width, height, format)) {
        drop();
        const D3D11_TEXTURE2D_DESC held = {width, height, 1u, 1u, static_cast<DXGI_FORMAT>(format), {1u, 0u},
                                           D3D11_USAGE_DEFAULT, 0u, 0u, 0u};
        if (FAILED(device_->device()->CreateTexture2D(&held, nullptr, &held_))) {
            held_ = nullptr;
            return false;
        }
        size(width, height);
        format_ = format;
    }
    device_->context()->CopySubresourceRegion(held_, 0u, 0u, 0u, 0u, source, subresource, nullptr);
    device_->context()->Flush();
    staged_ = false;
    return true;
}

bool D3d11FramePool::Frame::copy_to_staging()
{
    AV_SCOPE("D3d11FramePool::Frame::copy_to_staging");
    if (texture_ == nullptr) {
        const D3D11_TEXTURE2D_DESC staging = {width(), height(), 1u, 1u, static_cast<DXGI_FORMAT>(format_), {1u, 0u},
                                              D3D11_USAGE_STAGING, 0u, D3D11_CPU_ACCESS_READ, 0u};
        if (FAILED(device_->device()->CreateTexture2D(&staging, nullptr, &texture_))) {
            texture_ = nullptr;
            return false;
        }
    }
    device_->context()->CopyResource(texture_, held_);
    device_->context()->Flush();
    staged_ = true;
    return true;
}

void D3d11FramePool::Frame::drop()
{
    AV_SCOPE("D3d11FramePool::Frame::drop");
    if (texture_ != nullptr && mapped_) device_->context()->Unmap(texture_, 0u);
    if (texture_ != nullptr) texture_->Release();
    if (held_ != nullptr) held_->Release();
    texture_ = nullptr;
    held_ = nullptr;
    staged_ = false;
    mapped_ = false;
    size(0u, 0u);
    format_ = 0u;
}

StagedFrame::Reading D3d11FramePool::Frame::map_when_copied(const uint8_t *&pixels, uint32_t &row_pitch)
{
    AV_SCOPE("D3d11FramePool::Frame::map_when_copied");
    device_->lock(nullptr);
    if (held_ == nullptr || device_->lost() || (!staged_ && !copy_to_staging())) {
        device_->unlock();
        return Reading::failed;
    }
    D3D11_MAPPED_SUBRESOURCE mapped = {};
    const HRESULT result = device_->context()->Map(texture_, 0u, D3D11_MAP_READ, D3D11_MAP_FLAG_DO_NOT_WAIT, &mapped);
    if (SUCCEEDED(result)) mapped_ = true;
    device_->unlock();
    if (result == DXGI_ERROR_WAS_STILL_DRAWING) return Reading::copying;
    if (FAILED(result)) return Reading::failed;
    pixels = static_cast<const uint8_t *>(mapped.pData);
    row_pitch = mapped.RowPitch;
    return Reading::mapped;
}

void D3d11FramePool::Frame::unmap()
{
    AV_SCOPE("D3d11FramePool::Frame::unmap");
    if (!mapped_) return;
    device_->lock(nullptr);
    device_->context()->Unmap(texture_, 0u);
    mapped_ = false;
    device_->unlock();
}

}
