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

/* The pixels cross through a system-memory surface of the primary's own,
 * which GetRenderTargetData fills and waits for: the Direct3D 9 device has
 * drawn the frame by the time it is written into the Direct3D 11 texture. */
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
    if (FAILED(device->GetRenderTargetData(source, shown->staging))) return Status::invalid_state;
    D3DLOCKED_RECT locked = {};
    if (FAILED(shown->staging->LockRect(&locked, nullptr, D3DLOCK_READONLY))) return Status::invalid_state;
    device11_->context()->UpdateSubresource(shown->texture, 0u, nullptr, locked.pBits,
                                            static_cast<UINT>(locked.Pitch), 0u);
    shown->staging->UnlockRect();
    shares_->written(key);
    shown->used = ++flips_;
    return Status::ok;
}

/* Under the Direct3D 11 lock. A key already published by someone else
 * (conflict) is not this primary's to write: the slot is given back. */
D3d9FlipShares::Primary *D3d9FlipShares::primary(IDirect3DDevice9 *device, uint64_t key, uint32_t width,
                                                 uint32_t height, uint32_t format)
{
    AV_SCOPE("D3d9FlipShares::primary");
    Primary *oldest = &primaries_[0];
    for (Primary &held : primaries_) {
        if (held.key == key && held.device == device && held.width == width && held.height == height &&
            held.format == format)
            return &held;
        if (held.key == key) forget(held);
        if (held.used < oldest->used) oldest = &held;
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
    Primary made = {key, nullptr, nullptr, device, width, height, format, 0u};
    made.texture = device11_->make_exportable(described);
    if (made.texture == nullptr) return nullptr;
    if (FAILED(device->CreateOffscreenPlainSurface(width, height, static_cast<D3DFORMAT>(format), D3DPOOL_SYSTEMMEM,
                                                   &made.staging, nullptr)) ||
        shares_->publish_resource(key, made.texture, ShareOrigin::d3d11) != Status::ok) {
        forget(made);
        return nullptr;
    }
    *oldest = made;
    return oldest;
}

void D3d9FlipShares::forget(Primary &primary)
{
    AV_SCOPE("D3d9FlipShares::forget");
    if (primary.texture != nullptr) primary.texture->Release();
    if (primary.staging != nullptr) primary.staging->Release();
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
