#include "device/shim/d3d9_frame_pool.hpp"

#include "core/trace/function_scope.hpp"
#include "device/shim/d3d_headers.hpp"

namespace av {

D3d9FramePool::D3d9FramePool() : frames_()
{
    AV_SCOPE("D3d9FramePool::D3d9FramePool");
}

D3d9FramePool::~D3d9FramePool()
{
    AV_SCOPE("D3d9FramePool::~D3d9FramePool");
}

StagedFrame *D3d9FramePool::stage(IDirect3DDevice9 *device, IDirect3DSurface9 *source, uint32_t width,
                                  uint32_t height, uint32_t format, bool multisampled)
{
    AV_SCOPE("D3d9FramePool::stage");
    Frame *const chosen = frames_.claim(
        [&](const Frame &frame) { return frame.fits(device, width, height, format, multisampled); });
    if (chosen == nullptr) return nullptr;
    if (!chosen->take(device, source, width, height, format, multisampled)) {
        chosen->release();
        return nullptr;
    }
    return chosen;
}

void D3d9FramePool::clear()
{
    AV_SCOPE("D3d9FramePool::clear");
    frames_.each([](Frame &frame) {
        if (!frame.out()) frame.drop();
    });
}

D3d9FramePool::Frame::Frame()
    : device_(nullptr), surface_(nullptr), resolved_(nullptr), copied_(nullptr), width_(0u), height_(0u), format_(0u), mapped_(false),
      pixels_(nullptr), pitch_(0u), claim_()
{
    AV_SCOPE("D3d9FramePool::Frame::Frame");
}

D3d9FramePool::Frame::~Frame()
{
    AV_SCOPE("D3d9FramePool::Frame::~Frame");
    drop();
}

bool D3d9FramePool::Frame::out() const
{
    AV_SCOPE("D3d9FramePool::Frame::out");
    return claim_.out();
}

bool D3d9FramePool::Frame::claim()
{
    AV_SCOPE("D3d9FramePool::Frame::claim");
    return claim_.claim();
}

bool D3d9FramePool::Frame::fits(IDirect3DDevice9 *device, uint32_t width, uint32_t height, uint32_t format,
                                bool multisampled) const
{
    AV_SCOPE("D3d9FramePool::Frame::fits");
    return surface_ != nullptr && device_ == device && width_ == width && height_ == height && format_ == format &&
           (resolved_ != nullptr) == multisampled;
}

bool D3d9FramePool::Frame::make(IDirect3DDevice9 *device, uint32_t width, uint32_t height, uint32_t format,
                                bool multisampled)
{
    AV_SCOPE("D3d9FramePool::Frame::make");
    const D3DFORMAT as = static_cast<D3DFORMAT>(format);
    const bool made = SUCCEEDED(device->CreateOffscreenPlainSurface(width, height, as, D3DPOOL_SYSTEMMEM, &surface_,
                                                                    nullptr)) &&
                      SUCCEEDED(device->CreateQuery(D3DQUERYTYPE_EVENT, &copied_)) &&
                      (!multisampled || SUCCEEDED(device->CreateRenderTarget(width, height, as, D3DMULTISAMPLE_NONE,
                                                                             0u, FALSE, &resolved_, nullptr)));
    if (!made) {
        drop();
        return false;
    }
    device->AddRef();
    device_ = device;
    width_ = width;
    height_ = height;
    format_ = format;
    return true;
}

bool D3d9FramePool::Frame::take(IDirect3DDevice9 *device, IDirect3DSurface9 *source, uint32_t width,
                                uint32_t height, uint32_t format, bool multisampled)
{
    AV_SCOPE("D3d9FramePool::Frame::take");
    if (!fits(device, width, height, format, multisampled)) {
        drop();
        if (!make(device, width, height, format, multisampled)) return false;
    }
    if (resolved_ != nullptr && FAILED(device_->StretchRect(source, nullptr, resolved_, nullptr, D3DTEXF_NONE)))
        return false;
    if (FAILED(device_->GetRenderTargetData(resolved_ != nullptr ? resolved_ : source, surface_))) return false;
    copied_->Issue(D3DISSUE_END);
    return true;
}

void D3d9FramePool::Frame::drop()
{
    AV_SCOPE("D3d9FramePool::Frame::drop");
    if (surface_ != nullptr && mapped_) surface_->UnlockRect();
    if (surface_ != nullptr) surface_->Release();
    if (resolved_ != nullptr) resolved_->Release();
    if (copied_ != nullptr) copied_->Release();
    if (device_ != nullptr) device_->Release();
    surface_ = nullptr;
    resolved_ = nullptr;
    copied_ = nullptr;
    device_ = nullptr;
    mapped_ = false;
    width_ = height_ = format_ = 0u;
}

/* The first read flushes the copy to the GPU; a later one finds it done. */
StagedFrame::Reading D3d9FramePool::Frame::read(AvFrameView &view)
{
    AV_SCOPE("D3d9FramePool::Frame::read");
    if (!mapped_) {
        const HRESULT done = copied_->GetData(nullptr, 0u, D3DGETDATA_FLUSH);
        if (done == S_FALSE) return Reading::copying;
        D3DLOCKED_RECT locked = {};
        if (FAILED(done) || FAILED(surface_->LockRect(&locked, nullptr, D3DLOCK_READONLY))) return Reading::failed;
        mapped_ = true;
        pixels_ = static_cast<const uint8_t *>(locked.pBits);
        pitch_ = static_cast<uint32_t>(locked.Pitch);
    }
    view.pixels = pixels_;
    view.row_pitch = pitch_;
    view.width = width_;
    view.height = height_;
    view.reserved = 0u;
    return Reading::mapped;
}

void D3d9FramePool::Frame::release()
{
    AV_SCOPE("D3d9FramePool::Frame::release");
    if (mapped_) {
        surface_->UnlockRect();
        mapped_ = false;
    }
    claim_.give_back();
}

}  // namespace av
