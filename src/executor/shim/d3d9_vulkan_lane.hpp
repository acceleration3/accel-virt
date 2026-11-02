#ifndef AV_D3D9_VULKAN_LANE_HPP
#define AV_D3D9_VULKAN_LANE_HPP

#include "executor/shim/dxvk_queues.hpp"
#include "executor/shim/queue_work.hpp"
#include "executor/shim/vulkan_device_table.hpp"

#include <vulkan/vulkan_core.h>

struct IDirect3DDevice9;
struct IDirect3DSurface9;
struct ID3D9VkInteropDevice;

namespace av {

class D3d9VulkanLane {
public:
    D3d9VulkanLane();
    ~D3d9VulkanLane();
    D3d9VulkanLane(const D3d9VulkanLane &) = delete;
    D3d9VulkanLane &operator=(const D3d9VulkanLane &) = delete;

    [[nodiscard]] bool       connect(IDirect3DDevice9 *device);
    bool                     connected() const;
    IDirect3DDevice9        *device() const;
    void                     release();
    const VulkanDeviceTable &table() const;
    QueueWork               &work();

    [[nodiscard]] bool surface_image(IDirect3DSurface9 *surface, VkImage &image, VkImageLayout &layout);

private:
    IDirect3DDevice9     *device_;
    ID3D9VkInteropDevice *interop_;
    VulkanDeviceTable     table_;
    DxvkD3d9Queue         queue_;
    QueueWork             work_;
};

}

#endif
