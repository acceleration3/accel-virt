#include "executor/shim/host_surface_copy.hpp"

#include "core/platform/thread_lock_hold.hpp"
#include "core/support/scope_exit.hpp"
#include "core/trace/function_scope.hpp"
#include "executor/shim/d3d_headers.hpp"

#include <errno.h>
#include <poll.h>
#include <sys/eventfd.h>
#include <unistd.h>

namespace av {

namespace {

constexpr uint64_t nanoseconds_per_ms = 1000000u;

}

HostSurfaceCopy::HostSurfaceCopy() : fence_(nullptr), value_(0u), event_(), waiting_(), clock_()
{
    AV_SCOPE("HostSurfaceCopy::HostSurfaceCopy");
}

HostSurfaceCopy::~HostSurfaceCopy()
{
    AV_SCOPE("HostSurfaceCopy::~HostSurfaceCopy");
    release();
}

void HostSurfaceCopy::release()
{
    AV_SCOPE("HostSurfaceCopy::release");
    if (fence_ != nullptr) fence_->Release();
    fence_ = nullptr;
    value_ = 0u;
}

bool HostSurfaceCopy::compatible(ID3D11Texture2D *source, ID3D11Texture2D *destination, uint32_t &width,
                                 uint32_t &height) const
{
    AV_SCOPE("HostSurfaceCopy::compatible");
    D3D11_TEXTURE2D_DESC from = {};
    D3D11_TEXTURE2D_DESC into = {};
    source->GetDesc(&from);
    destination->GetDesc(&into);
    if (from.Format != into.Format || from.SampleDesc.Count != 1u || into.SampleDesc.Count != 1u) return false;
    if (from.Format != DXGI_FORMAT_B8G8R8A8_UNORM && from.Format != DXGI_FORMAT_B8G8R8A8_UNORM_SRGB &&
        from.Format != DXGI_FORMAT_B8G8R8A8_TYPELESS)
        return false;
    width = from.Width < into.Width ? from.Width : into.Width;
    height = from.Height < into.Height ? from.Height : into.Height;
    return true;
}

bool HostSurfaceCopy::fits(const AvCopyRegion &region, uint32_t width, uint32_t height) const
{
    AV_SCOPE("HostSurfaceCopy::fits");
    if (region.width == 0u || region.height == 0u) return false;
    return uint64_t{region.source_x} + region.width <= width && uint64_t{region.source_y} + region.height <= height &&
           uint64_t{region.destination_x} + region.width <= width &&
           uint64_t{region.destination_y} + region.height <= height;
}

bool HostSurfaceCopy::arm(ID3D11Device5 *device)
{
    AV_SCOPE("HostSurfaceCopy::arm");
    if (fence_ != nullptr) return true;
    value_ = 0u;
    return SUCCEEDED(device->CreateFence(0u, D3D11_FENCE_FLAG_NONE, __uuidof(ID3D11Fence),
                                         reinterpret_cast<void **>(&fence_)));
}

bool HostSurfaceCopy::submit(ID3D11Device5 *device, ID3D11DeviceContext4 *context, ID3D11Texture2D *source,
                             ID3D11Texture2D *destination, const AvCopyRegion *regions, uint32_t count,
                             ID3D11Fence *&fence, uint64_t &value)
{
    AV_SCOPE("HostSurfaceCopy::submit");
    fence = nullptr;
    value = 0u;
    uint32_t width = 0u;
    uint32_t height = 0u;
    if (device == nullptr || context == nullptr || source == nullptr || destination == nullptr || regions == nullptr)
        return false;
    if (!compatible(source, destination, width, height) || !arm(device)) return false;
    if (count == 0u) return false;
    for (uint32_t index = 0u; index < count; index++)
        if (!fits(regions[index], width, height)) return false;
    for (uint32_t index = 0u; index < count; index++) {
        const AvCopyRegion &region = regions[index];
        const D3D11_BOX     box = {region.source_x, region.source_y, 0u, region.source_x + region.width,
                                   region.source_y + region.height, 1u};
        context->CopySubresourceRegion(destination, 0u, region.destination_x, region.destination_y, 0u, source, 0u,
                                       &box);
    }
    value = ++value_;
    if (FAILED(context->Signal(fence_, value))) return false;
    context->Flush();
    fence_->AddRef();
    fence = fence_;
    return true;
}

void HostSurfaceCopy::drain()
{
    AV_SCOPE("HostSurfaceCopy::drain");
    uint64_t counted = 0u;
    while (read(event_.fd(), &counted, sizeof counted) > 0) {
    }
}

bool HostSurfaceCopy::wait(ID3D11Fence *fence, uint64_t value, uint32_t wait_ms)
{
    AV_SCOPE("HostSurfaceCopy::wait");
    if (fence == nullptr) return false;
    ScopeExit            let_go([fence] { fence->Release(); });
    const ThreadLockHold serial(waiting_);
    if (fence->GetCompletedValue() >= value) return true;
    if (!event_.held()) event_.hold(eventfd(0u, EFD_CLOEXEC | EFD_NONBLOCK));
    if (!event_.held()) return false;
    drain();
    if (FAILED(fence->SetEventOnCompletion(value, reinterpret_cast<void *>(static_cast<intptr_t>(event_.fd())))))
        return false;
    const uint64_t deadline = clock_.now_nanoseconds() + uint64_t{wait_ms} * nanoseconds_per_ms;
    for (;;) {
        const uint64_t now = clock_.now_nanoseconds();
        if (now >= deadline) return fence->GetCompletedValue() >= value;
        pollfd waiting = {event_.fd(), POLLIN, 0};
        const auto     left_ms = static_cast<int>((deadline - now + nanoseconds_per_ms - 1u) / nanoseconds_per_ms);
        const int      ready = poll(&waiting, 1, left_ms);
        if (ready < 0 && errno != EINTR) return false;
        if (ready > 0) drain();
        if (fence->GetCompletedValue() >= value) return true;
    }
}

}
