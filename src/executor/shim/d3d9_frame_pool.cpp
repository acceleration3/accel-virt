#include "executor/shim/d3d9_frame_pool.hpp"

#include "core/log/driver_log.hpp"
#include "core/platform/thread_lock_hold.hpp"
#include "core/trace/function_scope.hpp"
#include "executor/shim/d3d_headers.hpp"

namespace av {

D3d9FramePool::D3d9FramePool() : log_(nullptr), full_told_(false), copiers_lock_(), copiers_{}, frames_()
{
    AV_SCOPE("D3d9FramePool::D3d9FramePool");
    frames_.each([&](Frame &frame) { frame.bind(*this); });
}

D3d9FramePool::~D3d9FramePool()
{
    AV_SCOPE("D3d9FramePool::~D3d9FramePool");
}

void D3d9FramePool::bind(DriverLog &log)
{
    AV_SCOPE("D3d9FramePool::bind");
    log_ = &log;
}

D3d9FrameCopier *D3d9FramePool::lend_copier(IDirect3DDevice9 *device)
{
    AV_SCOPE("D3d9FramePool::lend_copier");
    const ThreadLockHold held(copiers_lock_);
    Copier *free = nullptr;
    for (Copier &slot : copiers_) {
        if (slot.uses_ != 0u && slot.copier_.device() == device) {
            slot.uses_++;
            return &slot.copier_;
        }
        if (slot.uses_ == 0u && free == nullptr) free = &slot;
    }
    if (free == nullptr) {
        if (log_ != nullptr && !full_told_) log_->write("Direct3D 9 presents are read back: the GPU copy serves only a few guest devices at once");
        full_told_ = true;
        return nullptr;
    }
    if (!free->copier_.connect(device)) return nullptr;
    free->uses_ = 1u;
    return &free->copier_;
}

void D3d9FramePool::return_copier(D3d9FrameCopier *copier)
{
    AV_SCOPE("D3d9FramePool::return_copier");
    const ThreadLockHold held(copiers_lock_);
    for (Copier &slot : copiers_) {
        if (&slot.copier_ != copier) continue;
        if (--slot.uses_ == 0u) slot.copier_.close();
        return;
    }
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
    : pool_(nullptr), device_(nullptr), surface_(nullptr), held_(nullptr), copied_(nullptr), multisampled_(false),
      staged_(false), format_(0u), mapped_(false), pixels_(nullptr), pitch_(0u), copier_(nullptr), copier_asked_(false), copy_value_(0u)
{
    AV_SCOPE("D3d9FramePool::Frame::Frame");
}

void D3d9FramePool::Frame::bind(D3d9FramePool &pool)
{
    AV_SCOPE("D3d9FramePool::Frame::bind");
    pool_ = &pool;
}

D3d9FramePool::Frame::~Frame()
{
    AV_SCOPE("D3d9FramePool::Frame::~Frame");
    drop();
}

bool D3d9FramePool::Frame::fits(IDirect3DDevice9 *device, uint32_t width, uint32_t height, uint32_t format,
                                bool multisampled) const
{
    AV_SCOPE("D3d9FramePool::Frame::fits");
    return surface_ != nullptr && device_ == device && this->width() == width && this->height() == height && format_ == format &&
           multisampled_ == multisampled;
}

bool D3d9FramePool::Frame::make(IDirect3DDevice9 *device, uint32_t width, uint32_t height, uint32_t format,
                                bool multisampled)
{
    AV_SCOPE("D3d9FramePool::Frame::make");
    const D3DFORMAT as = static_cast<D3DFORMAT>(format);
    const bool made = SUCCEEDED(device->CreateOffscreenPlainSurface(width, height, as, D3DPOOL_SYSTEMMEM, &surface_,
                                                                    nullptr)) &&
                      SUCCEEDED(device->CreateQuery(D3DQUERYTYPE_EVENT, &copied_)) &&
                      SUCCEEDED(device->CreateRenderTarget(width, height, as, D3DMULTISAMPLE_NONE, 0u, FALSE, &held_,
                                                           nullptr));
    if (!made) {
        drop();
        return false;
    }
    device->AddRef();
    device_ = device;
    multisampled_ = multisampled;
    size(width, height);
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
    if (FAILED(device_->StretchRect(source, nullptr, held_, nullptr, D3DTEXF_NONE))) return false;
    staged_ = false;
    return true;
}

bool D3d9FramePool::Frame::copy_to_memory()
{
    AV_SCOPE("D3d9FramePool::Frame::copy_to_memory");
    if (FAILED(device_->GetRenderTargetData(held_, surface_))) return false;
    if (FAILED(copied_->Issue(D3DISSUE_END))) return false;
    staged_ = true;
    return true;
}

void D3d9FramePool::Frame::drop()
{
    AV_SCOPE("D3d9FramePool::Frame::drop");
    if (surface_ != nullptr && mapped_) surface_->UnlockRect();
    /* A copy submitted for the lent pages may still be reading held_. Past a
     * second it is not waited for any longer, and held_ and the device it
     * belongs to are left alone - a leak, where a release is a use after free. */
    const bool pinned = copier_ != nullptr && copy_value_ != 0u && !copier_->wait(copy_value_, settle_ms);
    copy_value_ = 0u;
    if (copier_ != nullptr) pool_->return_copier(copier_);
    copier_ = nullptr;
    copier_asked_ = false;
    if (surface_ != nullptr) surface_->Release();
    if (held_ != nullptr && !pinned) held_->Release();
    if (copied_ != nullptr) copied_->Release();
    if (device_ != nullptr && !pinned) device_->Release();
    surface_ = nullptr;
    held_ = nullptr;
    multisampled_ = false;
    staged_ = false;
    copied_ = nullptr;
    device_ = nullptr;
    mapped_ = false;
    size(0u, 0u);
    format_ = 0u;
}

/* The first read copies the frame into memory and flushes it to the GPU; a
 * later one finds it done. */
StagedFrame::Reading D3d9FramePool::Frame::map_when_copied(const uint8_t *&pixels, uint32_t &row_pitch)
{
    AV_SCOPE("D3d9FramePool::Frame::map_when_copied");
    if (!staged_ && !copy_to_memory()) return Reading::failed;
    if (!mapped_) {
        const HRESULT done = copied_->GetData(nullptr, 0u, D3DGETDATA_FLUSH);
        if (done == S_FALSE) return Reading::copying;
        D3DLOCKED_RECT locked = {};
        if (FAILED(done) || FAILED(surface_->LockRect(&locked, nullptr, D3DLOCK_READONLY))) return Reading::failed;
        mapped_ = true;
        pixels_ = static_cast<const uint8_t *>(locked.pBits);
        pitch_ = static_cast<uint32_t>(locked.Pitch);
    }
    pixels = pixels_;
    row_pitch = pitch_;
    return Reading::mapped;
}

/* The copier is asked for once per device: a device DXVK gives no interop
 * is read back as before. */
StagedFrame::Copying D3d9FramePool::Frame::copy_into(const AvFrameCopy &target, uint32_t wait_ms)
{
    AV_SCOPE("D3d9FramePool::Frame::copy_into");
    if (held_ == nullptr) return Copying::declined;
    if (copier_ == nullptr && !copier_asked_) {
        copier_asked_ = true;
        copier_ = pool_->lend_copier(device_);
    }
    if (copier_ == nullptr) return Copying::declined;
    uint64_t value = 0u;
    const Copying done = copier_->copy(held_, target, wait_ms, value);
    if (value != 0u) copy_value_ = value;
    return done;
}

void D3d9FramePool::Frame::unmap()
{
    AV_SCOPE("D3d9FramePool::Frame::unmap");
    if (!mapped_) return;
    surface_->UnlockRect();
    mapped_ = false;
}

}  // namespace av
