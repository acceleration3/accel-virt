#include "executor/shim/d3d12_dispatch_limit.hpp"

#include "core/trace/function_scope.hpp"
#include "executor/shim/d3d_headers.hpp"

#include <vulkan/vulkan.h>

namespace av {

namespace {
constexpr uint32_t direct3d_most_groups = 65535u;

}

D3d12DispatchLimit::D3d12DispatchLimit()
    : most_x_(direct3d_most_groups), most_y_(direct3d_most_groups), most_z_(direct3d_most_groups)
{
    AV_SCOPE("D3d12DispatchLimit::D3d12DispatchLimit");
}

void D3d12DispatchLimit::measure(ID3D12Device &device)
{
    AV_SCOPE("D3d12DispatchLimit::measure");
    ID3D12DeviceExt *ext = nullptr;
    if (FAILED(device.QueryInterface(__uuidof(ID3D12DeviceExt), reinterpret_cast<void **>(&ext)))) return;
    VkInstance       instance = VK_NULL_HANDLE;
    VkPhysicalDevice physical = VK_NULL_HANDLE;
    VkDevice         vulkan = VK_NULL_HANDLE;
    const HRESULT    asked = ext->GetVulkanHandles(&instance, &physical, &vulkan);
    ext->Release();
    if (FAILED(asked) || physical == VK_NULL_HANDLE) return;
    const auto properties_of = reinterpret_cast<PFN_vkGetPhysicalDeviceProperties>(
        vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceProperties"));
    if (properties_of == nullptr) return;
    VkPhysicalDeviceProperties properties = {};
    properties_of(physical, &properties);
    most_x_ = properties.limits.maxComputeWorkGroupCount[0];
    most_y_ = properties.limits.maxComputeWorkGroupCount[1];
    most_z_ = properties.limits.maxComputeWorkGroupCount[2];
}

bool D3d12DispatchLimit::admits(uint32_t x, uint32_t y, uint32_t z) const
{
    AV_SCOPE("D3d12DispatchLimit::admits");
    return x <= most_x_ && y <= most_y_ && z <= most_z_;
}

}
