#include "executor/d3d/d3d9_flip_shares.hpp"

#include "core/support/scope_exit.hpp"
#include "core/platform/poll_budget.hpp"
#include "core/trace/function_scope.hpp"
#include "executor/owned_fd.hpp"
#include "executor/sharing/session_shares.hpp"
#include "executor/sharing/share.hpp"
#include "executor/shim/compositor_device.hpp"
#include "executor/shim/d3d11_host_device.hpp"
#include "executor/shim/d3d_headers.hpp"
#include "proto/d3d/d3d9_present_format.hpp"
#include "proto/vulkan/exportable_image_info.hpp"

#include <unistd.h>

namespace av {

namespace {
D3D11_TEXTURE2D_DESC1 shared_texture_desc(uint32_t width, uint32_t height, uint32_t format)
{
    AV_SCOPE("shared_texture_desc");
    D3D11_TEXTURE2D_DESC1 described = {};
    described.Width = width;
    described.Height = height;
    described.MipLevels = 1u;
    described.ArraySize = 1u;
    D3d9PresentFormat presented(DXGI_FORMAT_B8G8R8A8_UNORM);
    static_cast<void>(presented.take_ddi(format));
    described.Format = static_cast<DXGI_FORMAT>(presented.dxgi());
    described.SampleDesc.Count = 1u;
    described.Usage = D3D11_USAGE_DEFAULT;
    described.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    described.MiscFlags = D3D11_RESOURCE_MISC_SHARED;
    return described;
}

}

D3d9FlipShares::D3d9FlipShares(BlockSource &blocks)
    : device11_(nullptr), shares_(nullptr), lanes_(blocks), lane_of_(blocks, Pool::paged), flips_(0u), cpu_frames_(), clock_()
{
    AV_SCOPE("D3d9FlipShares::D3d9FlipShares");
    for (Primary &primary : primaries_) {
        primary.texture_ = nullptr;
        primary.lane_ = nullptr;
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
    const Status flipped = shown->lane_ != nullptr ? flip_on_gpu(*shown, source)
        : flip_on_cpu(*shown, source, desc.MultiSampleType != D3DMULTISAMPLE_NONE);
    if (flipped == Status::ok) shown->used_ = ++flips_;
    return flipped;
}

Status D3d9FlipShares::flip_on_gpu(Primary &shown, IDirect3DSurface9 *source)
{
    AV_SCOPE("D3d9FlipShares::flip_on_gpu");
    Lane         &lane = *shown.lane_;
    VkImage       image = VK_NULL_HANDLE;
    VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
    device11_->lock(nullptr);
    ScopeExit unlock([this] { device11_->unlock(); });
    if (!device11_->vulkan().signal(lane.consumed_, lane.order_.next()) ||
        !lane.vulkan_.work().wait(lane.reusable_, lane.order_.next()) ||
        !lane.vulkan_.surface_image(source, image, layout) ||
        !lane.vulkan_.work().copy(image, layout, shown.adopted_.image(), shown.width_, shown.height_,
                                  lane.signalled_, lane.order_.next()) ||
        !lane.order_.presented())
        return Status::invalid_state;
    shares_->written(shown.key_);
    return Status::ok;
}

Status D3d9FlipShares::flip_on_cpu(Primary &shown, IDirect3DSurface9 *source, bool multisampled)
{
    AV_SCOPE("D3d9FlipShares::flip_on_cpu");
    StagedFrame *const frame = cpu_frames_.stage(shown.device_, source, shown.width_, shown.height_,
                                                shown.format_, multisampled);
    if (frame == nullptr) return Status::invalid_state;
    ScopeExit release([frame] { frame->release(); });
    constexpr uint64_t pause_nanoseconds = 50000u;
    constexpr uint64_t deadline_nanoseconds = 5000000000u;
    PollBudget polls(clock_, 0u, pause_nanoseconds, deadline_nanoseconds);
    AvFrameView view = {};
    while (polls.next()) {
        const StagedFrame::Reading reading = frame->read(view);
        if (reading == StagedFrame::Reading::failed) return Status::invalid_state;
        if (reading != StagedFrame::Reading::mapped) continue;
        device11_->lock(nullptr);
        ScopeExit unlock([this] { device11_->unlock(); });
        if (device11_->lost() || device11_->context() == nullptr) return Status::invalid_state;
        device11_->context()->UpdateSubresource(shown.texture_, 0u, nullptr, view.pixels, view.row_pitch, 0u);
        shares_->written(shown.key_);
        return Status::ok;
    }
    return Status::invalid_state;
}

D3d9FlipShares::Lane *D3d9FlipShares::lane_for(IDirect3DDevice9 *device)
{
    AV_SCOPE("D3d9FlipShares::lane_for");
    const uint64_t key = reinterpret_cast<uintptr_t>(device);
    Lane *const   *known = lane_of_.find(key);
    if (known != nullptr) return (*known)->refused_ ? nullptr : *known;
    CompositorDevice *const compositor = device11_->compositor();
    if (compositor == nullptr || !compositor->opened() || lane_of_.reserve(lane_of_.count() + 1u) != Status::ok)
        return nullptr;
    Lane *made = nullptr;
    if (lanes_.create(made) != Status::ok) return nullptr;
    uint32_t slot = 0u;
    static_cast<void>(lane_of_.insert(key, made, slot));
    return connect(device, *made, *compositor);
}

D3d9FlipShares::Lane *D3d9FlipShares::connect(IDirect3DDevice9 *device, Lane &lane, CompositorDevice &compositor)
{
    AV_SCOPE("D3d9FlipShares::connect");
    OwnedFd for_d3d9;
    OwnedFd for_d3d11;
    OwnedFd for_reuse;
    bool    connected = lane.vulkan_.connect(device) && lane.timeline_.make_exportable(compositor.table()) &&
                        lane.timeline_.export_fd(for_d3d9) &&
                        lane.signalled_.import_fd(lane.vulkan_.table(), for_d3d9) && lane.timeline_.export_fd(for_d3d11);
    if (connected) connected = lane.order_.connect(*device11_, for_d3d11);
    if (connected) connected = lane.consumed_.make_exportable(device11_->vulkan().table()) &&
                               lane.consumed_.export_fd(for_reuse) &&
                               lane.reusable_.import_fd(lane.vulkan_.table(), for_reuse);
    if (connected) return &lane;
    close_lane(lane);
    lane.refused_ = true;
    return nullptr;
}

void D3d9FlipShares::close_lane(Lane &lane)
{
    AV_SCOPE("D3d9FlipShares::close_lane");
    lane.order_.release();
    lane.vulkan_.work().idle();
    lane.reusable_.release();
    lane.consumed_.release();
    lane.signalled_.release();
    lane.vulkan_.release();
    lane.timeline_.release();
}

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

bool D3d9FlipShares::make_on_gpu(Primary &made, Lane &lane)
{
    AV_SCOPE("D3d9FlipShares::make_on_gpu");
    CompositorDevice *const     compositor = device11_->compositor();
    const D3D11_TEXTURE2D_DESC1 desc = shared_texture_desc(made.width_, made.height_, made.format_);
    const ExportableImageInfo   info(desc.Width, desc.Height, 1u, 1u, 1u, static_cast<uint32_t>(desc.Format),
                                     desc.BindFlags);
    OwnedFd for_d3d9;
    OwnedFd for_d3d11;
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
    return made.texture_ != nullptr;
}

void D3d9FlipShares::forget(Primary &primary)
{
    AV_SCOPE("D3d9FlipShares::forget");
    if (primary.texture_ != nullptr) {
        device11_->lock(nullptr);
        primary.texture_->Release();
        device11_->unlock();
    }
    if (primary.lane_ != nullptr) static_cast<void>(primary.lane_->signalled_.wait(primary.lane_->order_.value(), 1000000000u));
    primary.adopted_.release();
    primary.image_.release();
    primary.texture_ = nullptr;
    primary.lane_ = nullptr;
    primary.key_ = primary.used_ = 0u;
    primary.device_ = nullptr;
    primary.width_ = primary.height_ = primary.format_ = 0u;
}

void D3d9FlipShares::forget_device(IDirect3DDevice9 *device)
{
    AV_SCOPE("D3d9FlipShares::forget_device");
    if (device11_ == nullptr) return;
    for (Primary &primary : primaries_)
        if (primary.device_ == device) forget(primary);
    cpu_frames_.clear();
    Lane *going = nullptr;
    if (!lane_of_.take(reinterpret_cast<uintptr_t>(device), going)) return;
    close_lane(*going);
    static_cast<void>(lanes_.destroy(going));
}

void D3d9FlipShares::release()
{
    AV_SCOPE("D3d9FlipShares::release");
    if (device11_ == nullptr) return;
    for (Primary &primary : primaries_) forget(primary);
    cpu_frames_.clear();
    lane_of_.for_each([this](uint64_t, Lane *lane) { close_lane(*lane); });
    lane_of_.clear();
    lanes_.destroy_all();
}

}
