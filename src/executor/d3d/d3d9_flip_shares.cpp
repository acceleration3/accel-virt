#include "executor/d3d/d3d9_flip_shares.hpp"

#include "core/support/scope_exit.hpp"
#include "core/trace/function_scope.hpp"
#include "executor/sharing/session_shares.hpp"
#include "executor/sharing/share.hpp"
#include "executor/shim/d3d11_host_device.hpp"
#include "executor/shim/d3d_headers.hpp"

namespace av {

D3d9FlipShares::D3d9FlipShares() : device11_(nullptr), shares_(nullptr), primaries_{}, flips_(0u)
{
    AV_SCOPE("D3d9FlipShares::D3d9FlipShares");
}

D3d9FlipShares::~D3d9FlipShares()
{
    AV_SCOPE("D3d9FlipShares::~D3d9FlipShares");
    release();
}

void D3d9FlipShares::bind(D3d11HostDevice *device11, SessionShares *shares)
{
    AV_SCOPE("D3d9FlipShares::bind");
    release();
    device11_ = device11 != nullptr && device11->opened() ? device11 : nullptr;
    shares_ = shares;
}

/* A flip writes what is finished and queues its own copy: nothing here waits
 * for the GPU unless both of a primary's copies are still in flight. */
Status D3d9FlipShares::flip(IDirect3DDevice9 *device, IDirect3DSurface9 *source, uint64_t key)
{
    AV_SCOPE("D3d9FlipShares::flip");
    if (device11_ == nullptr || shares_ == nullptr || device == nullptr || source == nullptr || key == 0u)
        return Status::not_supported;
    D3DSURFACE_DESC desc = {};
    if (FAILED(source->GetDesc(&desc))) return Status::invalid_parameter;
    if (desc.Format != D3DFMT_X8R8G8B8 && desc.Format != D3DFMT_A8R8G8B8) return Status::not_supported;
    device11_->lock(nullptr);
    ScopeExit unlock([this] { device11_->unlock(); });
    Primary *const shown = primary(device, key, desc.Width, desc.Height, static_cast<uint32_t>(desc.Format));
    if (shown == nullptr) return Status::no_memory;
    write_finished(*shown, false);
    Copy *const copy = free_copy(*shown);
    if (FAILED(device->GetRenderTargetData(source, copy->staging_))) return Status::invalid_state;
    copy->copied_->Issue(D3DISSUE_END);
    copy->flip_ = ++flips_;
    shown->used_ = flips_;
    return Status::ok;
}

/* Under the Direct3D 11 lock. A copy the GPU has not finished is left for a
 * later flip, unless `wait` asks for the oldest, whose finishing then frees
 * a slot. */
void D3d9FlipShares::write_finished(Primary &primary, bool wait)
{
    AV_SCOPE("D3d9FlipShares::write_finished");
    for (;;) {
        Copy *oldest = nullptr;
        for (Copy &copy : primary.copies_)
            if (copy.flip_ != 0u && (oldest == nullptr || copy.flip_ < oldest->flip_)) oldest = &copy;
        if (oldest == nullptr) return;
        HRESULT done = oldest->copied_->GetData(nullptr, 0u, D3DGETDATA_FLUSH);
        while (wait && done == S_FALSE) done = oldest->copied_->GetData(nullptr, 0u, D3DGETDATA_FLUSH);
        if (done == S_FALSE) return;
        wait = false;
        oldest->flip_ = 0u;
        D3DLOCKED_RECT locked = {};
        if (FAILED(done) || FAILED(oldest->staging_->LockRect(&locked, nullptr, D3DLOCK_READONLY))) continue;
        device11_->context()->UpdateSubresource(primary.texture_, 0u, nullptr, locked.pBits,
                                                static_cast<UINT>(locked.Pitch), 0u);
        oldest->staging_->UnlockRect();
        shares_->written(primary.key_);
    }
}

D3d9FlipShares::Copy *D3d9FlipShares::free_copy(Primary &primary)
{
    AV_SCOPE("D3d9FlipShares::free_copy");
    for (Copy &copy : primary.copies_)
        if (copy.flip_ == 0u) return &copy;
    write_finished(primary, true);
    for (Copy &copy : primary.copies_)
        if (copy.flip_ == 0u) return &copy;
    return &primary.copies_[0];
}

/* Under the Direct3D 11 lock. A key already published by someone else
 * (conflict) is not this primary's to write: the slot is given back. */
D3d9FlipShares::Primary *D3d9FlipShares::primary(IDirect3DDevice9 *device, uint64_t key, uint32_t width,
                                                 uint32_t height, uint32_t format)
{
    AV_SCOPE("D3d9FlipShares::primary");
    Primary *oldest = &primaries_[0];
    for (Primary &held : primaries_) {
        if (held.key_ == key && held.device_ == device && held.width_ == width && held.height_ == height &&
            held.format_ == format)
            return &held;
        if (held.key_ == key) forget(held);
        if (held.used_ < oldest->used_) oldest = &held;
    }
    forget(*oldest);
    D3D11_TEXTURE2D_DESC1 described = {};
    described.Width = width;
    described.Height = height;
    described.MipLevels = 1u;
    described.ArraySize = 1u;
    described.Format = format == D3DFMT_X8R8G8B8 ? DXGI_FORMAT_B8G8R8X8_UNORM : DXGI_FORMAT_B8G8R8A8_UNORM;
    described.SampleDesc.Count = 1u;
    described.Usage = D3D11_USAGE_DEFAULT;
    described.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    described.MiscFlags = D3D11_RESOURCE_MISC_SHARED;
    Primary made = {};
    made.key_ = key;
    made.device_ = device;
    made.width_ = width;
    made.height_ = height;
    made.format_ = format;
    made.texture_ = device11_->make_exportable(described);
    bool whole = made.texture_ != nullptr;
    for (Copy &copy : made.copies_)
        whole = whole &&
                SUCCEEDED(device->CreateOffscreenPlainSurface(width, height, static_cast<D3DFORMAT>(format),
                                                              D3DPOOL_SYSTEMMEM, &copy.staging_, nullptr)) &&
                SUCCEEDED(device->CreateQuery(D3DQUERYTYPE_EVENT, &copy.copied_));
    if (!whole || shares_->publish_resource(key, made.texture_, ShareOrigin::d3d11) != Status::ok) {
        forget(made);
        return nullptr;
    }
    *oldest = made;
    return oldest;
}

void D3d9FlipShares::forget(Primary &primary)
{
    AV_SCOPE("D3d9FlipShares::forget");
    if (primary.texture_ != nullptr) primary.texture_->Release();
    for (Copy &copy : primary.copies_) {
        if (copy.staging_ != nullptr) copy.staging_->Release();
        if (copy.copied_ != nullptr) copy.copied_->Release();
    }
    primary = Primary{};
}

void D3d9FlipShares::release()
{
    AV_SCOPE("D3d9FlipShares::release");
    if (device11_ == nullptr) return;
    device11_->lock(nullptr);
    for (Primary &primary : primaries_) forget(primary);
    device11_->unlock();
}

}  // namespace av
