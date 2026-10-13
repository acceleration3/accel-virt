/* One host GPU's compositing context: a Vulkan device of the executor's own,
 * on the GPU the guest's adapter stands for, that owns the images the APIs
 * hand each other and the timelines they order their work by.
 *
 * Each API runs on a device of its own - DXVK's Direct3D 11, a DXVK Direct3D 9
 * device per guest device, vkd3d-proton's Direct3D 12 - so a picture one of
 * them drew reached another through the CPU: read back, then uploaded. An
 * image made here (VulkanImage::make_exportable) is laid out linear over
 * memory exported as a dma-buf, which every one of those devices imports as
 * an image of its own over the same memory (VulkanImage::import_dma_buf), and
 * OpenGL as a memory object; a timeline made here (VulkanTimeline) is
 * imported by the device that writes the image and the one that reads it. A
 * picture crosses as a copy on the GPU (QueueWork), or not at all, and
 * nothing waits for it on the CPU. */
#ifndef AV_COMPOSITOR_DEVICE_HPP
#define AV_COMPOSITOR_DEVICE_HPP

#include "executor/shim/vulkan_device_table.hpp"

#include <stdint.h>
#include <vulkan/vulkan_core.h>

namespace av {

class CompositorDevice {
public:
    CompositorDevice();
    ~CompositorDevice();
    CompositorDevice(const CompositorDevice &) = delete;
    CompositorDevice &operator=(const CompositorDevice &) = delete;

    /* A device on the GPU `uuid` names, with external memory as dma-bufs and
     * external timelines as fds; false where the GPU or its driver has
     * neither, and the APIs then keep crossing through the CPU. */
    [[nodiscard]] bool       open(const uint8_t *uuid);
    bool                     opened() const;
    /* Every image and timeline made on the device must be gone first. */
    void                     close();
    const VulkanDeviceTable &table() const;

private:
    VkInstance        instance_;
    VkDevice          device_;
    VulkanDeviceTable table_;
};

}  // namespace av

#endif  // AV_COMPOSITOR_DEVICE_HPP
