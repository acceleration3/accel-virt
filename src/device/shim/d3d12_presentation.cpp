#include "device/shim/d3d12_presentation.hpp"

#include "core/trace/function_scope.hpp"
#include "device/shim/d3d11_host_device.hpp"
#include "device/shim/d3d12_host_device.hpp"
#include "device/shim/d3d_headers.hpp"
#include "device/shim/dxvk_vulkan_device.hpp"
#include "device/shim/vkd3d_image_export.hpp"

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

/* A swap chain buffer is one 2D image of one mip; what DWM makes of it is a
 * texture it samples, and the view formats vkd3d-proton made it with are what
 * DXVK may view it as. */
bool D3d12Presentation::share(ID3D12Resource *resource, uint64_t key)
{
    AV_SCOPE("D3d12Presentation::share");
    if (device12_ == nullptr || device11_ == nullptr || resource == nullptr || key == 0u) return false;
    const D3D12_RESOURCE_DESC desc = resource->GetDesc();
    if (desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D || desc.MipLevels != 1u || desc.DepthOrArraySize != 1u ||
        desc.SampleDesc.Count != 1u)
        return false;
    Vkd3dImageExport image;
    if (!device12_->export_image(resource, image)) return false;
    D3D11_TEXTURE2D_DESC1 shown = {};
    shown.Width = static_cast<UINT>(desc.Width);
    shown.Height = desc.Height;
    shown.MipLevels = 1u;
    shown.ArraySize = 1u;
    shown.Format = desc.Format;
    shown.SampleDesc = {1u, 0u};
    shown.Usage = D3D11_USAGE_DEFAULT;
    shown.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    if ((desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET) != 0) shown.BindFlags |= D3D11_BIND_RENDER_TARGET;
    if (device11_->share_image(key, image, shown)) return true;
    fprintf(stderr,
            "virtio-gpu-rvk: d3d12: a swap chain buffer %ux%u format %u flags 0x%x could not be shown under key "
            "0x%016llx\n",
            shown.Width, shown.Height, static_cast<unsigned>(shown.Format), static_cast<unsigned>(desc.Flags),
            static_cast<unsigned long long>(key));
    return false;
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
        fprintf(stderr, "virtio-gpu-rvk: d3d12: no timeline shared with DXVK; presents cannot be shown\n");
        return false;
    }
    fence_ = made;
    value_ = 0u;
    return true;
}

}  // namespace av
