/* vkd3d-proton's Direct3D 12 device as the executor borrows it through its
 * interop (ID3D12DXVKInteropDevice): the device's table (VulkanDeviceTable), a
 * queue of the lane's own made on the device and lent back as Vulkan
 * (Vkd3dQueue), and the work the executor adds to it (QueueWork). Its own
 * queue, because what it writes - a staged frame - outlives the session queue
 * that staged it; the frame's fence orders the two. */
#ifndef AV_D3D12_VULKAN_LANE_HPP
#define AV_D3D12_VULKAN_LANE_HPP

#include "executor/shim/queue_work.hpp"
#include "executor/shim/vkd3d_queue.hpp"
#include "executor/shim/vulkan_device_table.hpp"

#include <stdint.h>
#include <vulkan/vulkan_core.h>

struct ID3D12CommandQueue;
struct ID3D12DXVKInteropDevice;
struct ID3D12Device;
struct ID3D12Resource;

namespace av {

class D3d12VulkanLane {
public:
    D3d12VulkanLane();
    ~D3d12VulkanLane();
    D3d12VulkanLane(const D3d12VulkanLane &) = delete;
    D3d12VulkanLane &operator=(const D3d12VulkanLane &) = delete;

    /* `device`'s Vulkan device and a queue made on it; false where vkd3d
     * offers no interop or no queue. */
    [[nodiscard]] bool       connect(ID3D12Device *device);
    bool                     connected() const;
    void                     release();
    const VulkanDeviceTable &table() const;
    QueueWork               &work();
    /* The VkBuffer behind `resource`, a buffer, and where its bytes begin in
     * it. */
    [[nodiscard]] bool buffer_of(ID3D12Resource *resource, VkBuffer &buffer, uint64_t &offset) const;

private:
    ID3D12DXVKInteropDevice *interop_;   /* held */
    ID3D12CommandQueue      *queue_;     /* held */
    VulkanDeviceTable        table_;
    Vkd3dQueue               borrowed_;
    QueueWork                work_;
};

}  // namespace av

#endif  // AV_D3D12_VULKAN_LANE_HPP
