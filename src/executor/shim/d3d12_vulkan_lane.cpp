#include "executor/shim/d3d12_vulkan_lane.hpp"

#include "core/trace/function_scope.hpp"
#include "executor/shim/vkd3d_interop.hpp"

namespace av {

D3d12VulkanLane::D3d12VulkanLane() : interop_(nullptr), queue_(nullptr), table_(), borrowed_(), work_()
{
    AV_SCOPE("D3d12VulkanLane::D3d12VulkanLane");
}

D3d12VulkanLane::~D3d12VulkanLane()
{
    AV_SCOPE("D3d12VulkanLane::~D3d12VulkanLane");
    release();
}

/* A copy queue: the lane only ever copies, and vkd3d puts one on a transfer
 * family where the GPU has one. */
bool D3d12VulkanLane::connect(ID3D12Device *device)
{
    AV_SCOPE("D3d12VulkanLane::connect");
    if (connected()) return true;
    if (device == nullptr ||
        FAILED(device->QueryInterface(__uuidof(ID3D12DXVKInteropDevice), reinterpret_cast<void **>(&interop_)))) {
        interop_ = nullptr;
        return false;
    }
    VkInstance       instance = VK_NULL_HANDLE;
    VkPhysicalDevice physical = VK_NULL_HANDLE;
    VkDevice         vulkan = VK_NULL_HANDLE;
    D3D12_COMMAND_QUEUE_DESC desc = {};
    desc.Type = D3D12_COMMAND_LIST_TYPE_COPY;
    const bool made =
        SUCCEEDED(interop_->GetVulkanHandles(&instance, &physical, &vulkan)) &&
        table_.load(instance, physical, vulkan) &&
        SUCCEEDED(device->CreateCommandQueue(&desc, __uuidof(ID3D12CommandQueue), reinterpret_cast<void **>(&queue_)));
    if (made) borrowed_.borrow(interop_, queue_);
    if (made && borrowed_.queue() != VK_NULL_HANDLE && work_.open(table_, borrowed_)) return true;
    release();
    return false;
}

bool D3d12VulkanLane::connected() const
{
    AV_SCOPE("D3d12VulkanLane::connected");
    return work_.opened();
}

void D3d12VulkanLane::release()
{
    AV_SCOPE("D3d12VulkanLane::release");
    work_.close();
    borrowed_.borrow(nullptr, nullptr);
    table_.forget();
    if (queue_ != nullptr) queue_->Release();
    if (interop_ != nullptr) interop_->Release();
    queue_ = nullptr;
    interop_ = nullptr;
}

const VulkanDeviceTable &D3d12VulkanLane::table() const
{
    AV_SCOPE("D3d12VulkanLane::table");
    return table_;
}

QueueWork &D3d12VulkanLane::work()
{
    AV_SCOPE("D3d12VulkanLane::work");
    return work_;
}

bool D3d12VulkanLane::buffer_of(ID3D12Resource *resource, VkBuffer &buffer, uint64_t &offset) const
{
    AV_SCOPE("D3d12VulkanLane::buffer_of");
    buffer = VK_NULL_HANDLE;
    offset = 0u;
    UINT64 handle = 0u, start = 0u;
    if (interop_ == nullptr || resource == nullptr || FAILED(interop_->GetVulkanResourceInfo(resource, &handle, &start)))
        return false;
    buffer = reinterpret_cast<VkBuffer>(handle);
    offset = start;
    return buffer != VK_NULL_HANDLE;
}

}  // namespace av
