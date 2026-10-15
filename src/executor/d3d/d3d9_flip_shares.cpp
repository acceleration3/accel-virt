#include "executor/d3d/d3d9_flip_shares.hpp"

#include "core/support/scope_exit.hpp"
#include "core/trace/function_scope.hpp"
#include "executor/sharing/session_shares.hpp"
#include "executor/sharing/share.hpp"
#include "executor/shim/compositor_device.hpp"
#include "executor/shim/d3d11_host_device.hpp"
#include "executor/shim/d3d_headers.hpp"
#include "proto/vulkan/exportable_image_info.hpp"

#include <unistd.h>

namespace av {

namespace {

/* The texture DWM and the scanout read a primary as. */
D3D11_TEXTURE2D_DESC1 shared_texture_desc(uint32_t width, uint32_t height, uint32_t format)
{
    AV_SCOPE("shared_texture_desc");
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
    return described;
}

}  // namespace

D3d9FlipShares::D3d9FlipShares() : device11_(nullptr), shares_(nullptr), flips_(0u)
{
    AV_SCOPE("D3d9FlipShares::D3d9FlipShares");
    for (Lane &lane : lanes_) {
        lane.value_ = 0u;
        lane.refused_ = false;
    }
    for (Primary &primary : primaries_) {
        primary.texture_ = nullptr;
        primary.lane_ = nullptr;
        for (Copy &copy : primary.copies_) copy = Copy{nullptr, nullptr, 0u};
        primary.key_ = primary.used_ = 0u;
        primary.device_ = nullptr;
        primary.width_ = primary.height_ = primary.format_ = 0u;
    }
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

Status D3d9FlipShares::flip(IDirect3DDevice9 *device, IDirect3DSurface9 *source, uint64_t key)
{
    AV_SCOPE("D3d9FlipShares::flip");
    if (device11_ == nullptr || shares_ == nullptr || device == nullptr || source == nullptr || key == 0u)
        return Status::not_supported;
    D3DSURFACE_DESC desc = {};
    if (FAILED(source->GetDesc(&desc))) return Status::invalid_parameter;
    if (desc.Format != D3DFMT_X8R8G8B8 && desc.Format != D3DFMT_A8R8G8B8) return Status::not_supported;
    Primary *const shown = primary(device, key, desc.Width, desc.Height, static_cast<uint32_t>(desc.Format));
    if (shown == nullptr) return Status::no_memory;
    const Status flipped = shown->lane_ != nullptr ? flip_on_gpu(*shown, source) : flip_on_cpu(*shown, source);
    if (flipped == Status::ok) shown->used_ = ++flips_;
    return flipped;
}

/* The copy goes on the Direct3D 9 device's queue behind the frame; Direct3D
 * 11's queue waits for it before anything it runs after - DWM's composition,
 * the scanout's export - reads the texture. */
Status D3d9FlipShares::flip_on_gpu(Primary &shown, IDirect3DSurface9 *source)
{
    AV_SCOPE("D3d9FlipShares::flip_on_gpu");
    Lane         &lane = *shown.lane_;
    VkImage       image = VK_NULL_HANDLE;
    VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (!lane.vulkan_.surface_image(source, image, layout) ||
        !lane.vulkan_.work().copy(image, layout, shown.adopted_.image(), shown.width_, shown.height_,
                                  lane.signalled_, lane.value_ + 1u))
        return Status::invalid_state;
    lane.value_++;
    device11_->lock(nullptr);
    ScopeExit unlock([this] { device11_->unlock(); });
    if (!device11_->vulkan().wait(lane.waited_, lane.value_)) return Status::invalid_state;
    shares_->written(shown.key_);
    return Status::ok;
}

/* A flip writes what is finished and queues its own copy: nothing here waits
 * for the GPU unless both of a primary's copies are still in flight. */
Status D3d9FlipShares::flip_on_cpu(Primary &shown, IDirect3DSurface9 *source)
{
    AV_SCOPE("D3d9FlipShares::flip_on_cpu");
    device11_->lock(nullptr);
    ScopeExit unlock([this] { device11_->unlock(); });
    write_finished(shown, false);
    Copy *const copy = free_copy(shown);
    if (FAILED(shown.device_->GetRenderTargetData(source, copy->staging_))) return Status::invalid_state;
    copy->copied_->Issue(D3DISSUE_END);
    copy->flip_ = flips_ + 1u;
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

/* A device's lane is connected once: a device DXVK gives no interop, or a GPU
 * with no compositor, is asked no more and flips on the CPU. */
D3d9FlipShares::Lane *D3d9FlipShares::lane_for(IDirect3DDevice9 *device)
{
    AV_SCOPE("D3d9FlipShares::lane_for");
    Lane *spare = nullptr;
    for (Lane &lane : lanes_) {
        if (lane.vulkan_.device() == device) return lane.refused_ ? nullptr : &lane;
        if (spare == nullptr && lane.vulkan_.device() == nullptr && !lane.refused_) spare = &lane;
    }
    CompositorDevice *const compositor = device11_->compositor();
    if (spare == nullptr || compositor == nullptr || !compositor->opened()) return nullptr;
    Lane &lane = *spare;
    int   for_d3d9 = -1, for_d3d11 = -1;
    bool  connected = lane.vulkan_.connect(device) && lane.timeline_.make_exportable(compositor->table()) &&
                     lane.timeline_.export_fd(for_d3d9) && lane.signalled_.import_fd(lane.vulkan_.table(), for_d3d9) &&
                     lane.timeline_.export_fd(for_d3d11);
    if (connected) {
        device11_->lock(nullptr);
        connected = device11_->vulkan().import_timeline(for_d3d11, lane.waited_);
        device11_->unlock();
    }
    if (connected) return &lane;
    close_lane(lane);
    lane.refused_ = true;
    return nullptr;
}

void D3d9FlipShares::close_lane(Lane &lane)
{
    AV_SCOPE("D3d9FlipShares::close_lane");
    if (lane.waited_.made()) {
        device11_->lock(nullptr);
        device11_->vulkan().idle();
        lane.waited_.release();
        device11_->unlock();
    }
    lane.vulkan_.work().idle();
    lane.signalled_.release();
    lane.vulkan_.release();
    lane.timeline_.release();
    lane.value_ = 0u;
}

/* A key already published by someone else (conflict) is not this primary's
 * to write: the slot is given back. */
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
    Primary &made = *oldest;
    made.key_ = key;
    made.device_ = device;
    made.width_ = width;
    made.height_ = height;
    made.format_ = format;
    Lane *const lane = lane_for(device);
    const bool  whole = lane != nullptr ? make_on_gpu(made, *lane) : make_on_cpu(made);
    if (!whole || shares_->publish_resource(key, made.texture_, ShareOrigin::d3d11) != Status::ok) {
        forget(made);
        return nullptr;
    }
    return &made;
}

/* One image of the compositor's, opened by both devices: Direct3D 11 as the
 * shared texture, Direct3D 9 as what its flips copy into. */
bool D3d9FlipShares::make_on_gpu(Primary &made, Lane &lane)
{
    AV_SCOPE("D3d9FlipShares::make_on_gpu");
    CompositorDevice *const     compositor = device11_->compositor();
    const D3D11_TEXTURE2D_DESC1 desc = shared_texture_desc(made.width_, made.height_, made.format_);
    const ExportableImageInfo   info(desc.Width, desc.Height, 1u, 1u, 1u, static_cast<uint32_t>(desc.Format),
                                     desc.BindFlags);
    int for_d3d9 = -1, for_d3d11 = -1;
    if (!info.supported() || !made.image_.make_exportable(compositor->table(), info.info()) ||
        !made.image_.export_dma_buf(for_d3d9) || !made.adopted_.import_dma_buf(lane.vulkan_.table(), info.info(), for_d3d9) ||
        !made.image_.export_dma_buf(for_d3d11))
        return false;
    device11_->lock(nullptr);
    made.texture_ = device11_->adopt_exportable(for_d3d11, desc);
    device11_->unlock();
    if (made.texture_ == nullptr) return false;
    made.lane_ = &lane;
    return true;
}

bool D3d9FlipShares::make_on_cpu(Primary &made)
{
    AV_SCOPE("D3d9FlipShares::make_on_cpu");
    const D3D11_TEXTURE2D_DESC1 desc = shared_texture_desc(made.width_, made.height_, made.format_);
    device11_->lock(nullptr);
    made.texture_ = device11_->make_tiled(desc);
    device11_->unlock();
    if (made.texture_ == nullptr) return false;
    for (Copy &copy : made.copies_)
        if (FAILED(made.device_->CreateOffscreenPlainSurface(made.width_, made.height_,
                                                             static_cast<D3DFORMAT>(made.format_),
                                                             D3DPOOL_SYSTEMMEM, &copy.staging_, nullptr)) ||
            FAILED(made.device_->CreateQuery(D3DQUERYTYPE_EVENT, &copy.copied_)))
            return false;
    return true;
}

void D3d9FlipShares::forget(Primary &primary)
{
    AV_SCOPE("D3d9FlipShares::forget");
    if (primary.texture_ != nullptr) {
        device11_->lock(nullptr);
        primary.texture_->Release();
        device11_->unlock();
    }
    for (Copy &copy : primary.copies_) {
        if (copy.staging_ != nullptr) copy.staging_->Release();
        if (copy.copied_ != nullptr) copy.copied_->Release();
        copy = Copy{nullptr, nullptr, 0u};
    }
    /* A copy may still be writing the image: it is done before the image
     * goes. */
    if (primary.lane_ != nullptr) static_cast<void>(primary.lane_->signalled_.wait(primary.lane_->value_, 1000000000u));
    primary.adopted_.release();
    primary.image_.release();
    primary.texture_ = nullptr;
    primary.lane_ = nullptr;
    primary.key_ = primary.used_ = 0u;
    primary.device_ = nullptr;
    primary.width_ = primary.height_ = primary.format_ = 0u;
}

void D3d9FlipShares::release()
{
    AV_SCOPE("D3d9FlipShares::release");
    if (device11_ == nullptr) return;
    for (Primary &primary : primaries_) forget(primary);
    for (Lane &lane : lanes_) {
        close_lane(lane);
        lane.refused_ = false;
    }
}

}  // namespace av
