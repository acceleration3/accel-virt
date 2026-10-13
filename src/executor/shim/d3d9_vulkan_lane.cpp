#include "executor/shim/d3d9_vulkan_lane.hpp"

#include "core/support/scope_exit.hpp"
#include "core/trace/function_scope.hpp"
#include "executor/shim/d3d_headers.hpp"

namespace av {

D3d9VulkanLane::D3d9VulkanLane() : device_(nullptr), interop_(nullptr)
{
    AV_SCOPE("D3d9VulkanLane::D3d9VulkanLane");
}

D3d9VulkanLane::~D3d9VulkanLane()
{
    AV_SCOPE("D3d9VulkanLane::~D3d9VulkanLane");
    release();
}

bool D3d9VulkanLane::connect(IDirect3DDevice9 *device)
{
    AV_SCOPE("D3d9VulkanLane::connect");
    if (interop_ != nullptr) return device == device_;
    if (device == nullptr) return false;
    ID3D9VkInteropDevice *interop = nullptr;
    if (FAILED(device->QueryInterface(__uuidof(ID3D9VkInteropDevice), reinterpret_cast<void **>(&interop))))
        return false;
    VkInstance       instance = VK_NULL_HANDLE;
    VkPhysicalDevice physical = VK_NULL_HANDLE;
    VkDevice         vulkan = VK_NULL_HANDLE;
    interop->GetVulkanHandles(&instance, &physical, &vulkan);
    interop_ = interop;
    device_ = device;
    queue_.borrow(interop);
    if (table_.load(instance, physical, vulkan) && work_.open(table_, queue_)) return true;
    release();
    return false;
}

bool D3d9VulkanLane::connected() const
{
    AV_SCOPE("D3d9VulkanLane::connected");
    return work_.opened();
}

IDirect3DDevice9 *D3d9VulkanLane::device() const
{
    AV_SCOPE("D3d9VulkanLane::device");
    return device_;
}

void D3d9VulkanLane::release()
{
    AV_SCOPE("D3d9VulkanLane::release");
    work_.close();
    table_.forget();
    queue_.borrow(nullptr);
    if (interop_ != nullptr) interop_->Release();
    interop_ = nullptr;
    device_ = nullptr;
}

const VulkanDeviceTable &D3d9VulkanLane::table() const
{
    AV_SCOPE("D3d9VulkanLane::table");
    return table_;
}

QueueWork &D3d9VulkanLane::work()
{
    AV_SCOPE("D3d9VulkanLane::work");
    return work_;
}

/* A transition to the layout the image is already in makes DXVK prepare the
 * image, which carries out a clear it deferred. */
bool D3d9VulkanLane::surface_image(IDirect3DSurface9 *surface, VkImage &image, VkImageLayout &layout)
{
    AV_SCOPE("D3d9VulkanLane::surface_image");
    image = VK_NULL_HANDLE;
    layout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (!connected() || surface == nullptr) return false;
    ID3D9VkInteropTexture *texture = nullptr;
    if (FAILED(surface->QueryInterface(__uuidof(ID3D9VkInteropTexture), reinterpret_cast<void **>(&texture))))
        return false;
    ScopeExit release_texture([texture] { texture->Release(); });
    if (FAILED(texture->GetVulkanImageInfo(&image, &layout, nullptr)) || image == VK_NULL_HANDLE) return false;
    const VkImageSubresourceRange whole = {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u};
    interop_->TransitionTextureLayout(texture, &whole, layout, layout);
    return true;
}

}  // namespace av
