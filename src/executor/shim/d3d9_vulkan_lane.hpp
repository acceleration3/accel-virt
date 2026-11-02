/* A DXVK Direct3D 9 device as the executor borrows it through its interop
 * (ID3D9VkInteropDevice): the device's table (VulkanDeviceTable), its queue
 * (DxvkD3d9Queue) and the work the executor adds to it (QueueWork) - and the
 * Vulkan image behind one of its surfaces, which a copy on that queue reads. */
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

    /* `device`'s Vulkan device and queue; false where DXVK offers no interop
     * or the device lacks the fd entry points. The device must outlive this. */
    [[nodiscard]] bool       connect(IDirect3DDevice9 *device);
    bool                     connected() const;
    IDirect3DDevice9        *device() const;
    void                     release();
    const VulkanDeviceTable &table() const;
    QueueWork               &work();
    /* DXVK's queue, for work of the executor's own (FrameBufferCopy). */
    BorrowedQueue           &queue();

    /* The image behind `surface` and the layout it is in, once DXVK has
     * carried out a clear it deferred: DXVK clears an image when it next uses
     * it itself, which a copy of the executor's is not. */
    [[nodiscard]] bool surface_image(IDirect3DSurface9 *surface, VkImage &image, VkImageLayout &layout);
    /* The same, and how DXVK made the image (`described`). */
    [[nodiscard]] bool surface_image(IDirect3DSurface9 *surface, VkImage &image, VkImageLayout &layout,
                                     VkImageCreateInfo &described);
    /* The Direct3D 9 device held, as a thread that is not the one calling it
     * must hold it to record into it (DXVK's multithreaded device lock), until
     * unlock_device. Neither nests: unlock_device lets go of the one lock. */
    void               lock_device();
    void               unlock_device();

private:
    IDirect3DDevice9     *device_;
    ID3D9VkInteropDevice *interop_;   /* held */
    VulkanDeviceTable     table_;
    DxvkD3d9Queue         queue_;
    QueueWork             work_;
};

}  // namespace av

#endif  // AV_D3D9_VULKAN_LANE_HPP
