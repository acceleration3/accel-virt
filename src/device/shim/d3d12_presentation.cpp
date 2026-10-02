#include "device/shim/d3d12_presentation.hpp"

#include "core/log/driver_log.hpp"
#include "core/trace/function_scope.hpp"
#include "device/host/stdio_log.hpp"
#include "device/shim/d3d11_host_device.hpp"
#include "device/shim/d3d12_host_device.hpp"
#include "device/shim/d3d_headers.hpp"
#include "device/shim/dxvk_vulkan_device.hpp"
#include "device/shim/surface_identity.hpp"

#include <stdio.h>
#include <unistd.h>

namespace av {

D3d12Presentation::D3d12Presentation()
    : device12_(nullptr), device11_(nullptr), fence_(nullptr), waited_(VK_NULL_HANDLE), value_(0u)
{
    AV_SCOPE("D3d12Presentation::D3d12Presentation");
}

D3d12Presentation::~D3d12Presentation()
{
    AV_SCOPE("D3d12Presentation::~D3d12Presentation");
    release();
}

void D3d12Presentation::bind(D3d12HostDevice *device12, D3d11HostDevice *device11)
{
    AV_SCOPE("D3d12Presentation::bind");
    release();
    device12_ = device12;
    device11_ = device11;
}

/* Any committed texture made shared is marked as the surface it is: a blt
 * of it is staged by that (SharedSurfaces), whatever its shape. Only one the
 * compositor can open - one 2D image of one mip and one sample, a swap chain
 * buffer - is published for it, and its view is made the first time it is
 * opened (D3d11HostDevice::find_shared): Direct3D 9 on 12 shares every
 * texture it makes, and only its presents are ever read. A table too full to
 * publish one costs that one the compositor, not the session its device. */
bool D3d12Presentation::share(ID3D12Resource *resource, uint64_t key)
{
    AV_SCOPE("D3d12Presentation::share");
    if (device12_ == nullptr || device11_ == nullptr || resource == nullptr || key == 0u) return false;
    const D3D12_RESOURCE_DESC desc = resource->GetDesc();
    D3D12_HEAP_PROPERTIES     heap = {};
    D3D12_HEAP_FLAGS          heap_flags = D3D12_HEAP_FLAG_NONE;
    if (desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D || FAILED(resource->GetHeapProperties(&heap, &heap_flags)) ||
        (heap_flags & D3D12_HEAP_FLAG_SHARED) == 0)
        return false;
    /* A key already shared is another surface's: taking it is refused. */
    const bool composable = desc.MipLevels == 1u && desc.DepthOrArraySize == 1u && desc.SampleDesc.Count == 1u;
    if (composable && device11_->shares(key)) return false;
    if (!SurfaceIdentity(key).mark(resource)) return false;
    if (composable) static_cast<void>(device11_->publish_shared(key, resource));
    return true;
}

bool D3d12Presentation::present(ID3D12CommandQueue *queue)
{
    AV_SCOPE("D3d12Presentation::present");
    if (queue == nullptr || !connect()) return false;
    if (FAILED(queue->Signal(fence_, value_ + 1u))) return false;
    value_++;
    return device11_->vulkan().wait(waited_, value_);
}

void D3d12Presentation::release()
{
    AV_SCOPE("D3d12Presentation::release");
    if (waited_ != VK_NULL_HANDLE) {
        device11_->lock(nullptr);
        device11_->vulkan().idle();
        device11_->vulkan().destroy_semaphore(waited_);
        device11_->unlock();
    }
    if (fence_ != nullptr) fence_->Release();
    fence_ = nullptr;
    waited_ = VK_NULL_HANDLE;
    value_ = 0u;
}

bool D3d12Presentation::connect()
{
    AV_SCOPE("D3d12Presentation::connect");
    if (waited_ != VK_NULL_HANDLE) return true;
    if (device12_ == nullptr || device11_ == nullptr || !device11_->vulkan().connected()) return false;
    ID3D12Fence *made = nullptr;
    if (FAILED(device12_->device()->CreateFence(0u, D3D12_FENCE_FLAG_SHARED, __uuidof(ID3D12Fence),
                                                reinterpret_cast<void **>(&made))))
        return false;
    int fd = -1;
    if (!device12_->export_fence(made, fd) || !device11_->vulkan().import_timeline(fd, waited_)) {
        if (fd >= 0) close(fd);
        made->Release();
        StdioLog sink(stderr);
        DriverLog(sink, "virtio-gpu-av: d3d12: ").write("no timeline shared with DXVK; presents cannot be shown");
        return false;
    }
    fence_ = made;
    value_ = 0u;
    return true;
}

}  // namespace av
