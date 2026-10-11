/* One image of DXVK's Vulkan device and the memory bound to it, under a
 * Direct3D 11 texture of DXVK's that other APIs share (ExportableImages),
 * which holds it as private data (TextureHold) so the image outlives it. */
#ifndef AV_EXPORTABLE_IMAGE_HPP
#define AV_EXPORTABLE_IMAGE_HPP

#include "executor/shim/texture_hold.hpp"

#include <vulkan/vulkan.h>

namespace av {

class DxvkVulkanDevice;
class ExportableImages;

class ExportableImage final : public TextureHold {
public:
    ExportableImage(ExportableImages &owner, VkImage image, VkDeviceMemory memory);

    VkImage        image() const;
    VkDeviceMemory memory() const;
    /* The image and its memory destroyed: the GPU is done with them. */
    void           free(const DxvkVulkanDevice &vulkan);

private:
    VkImage        image_;
    VkDeviceMemory memory_;
};

}  // namespace av

#endif  // AV_EXPORTABLE_IMAGE_HPP
