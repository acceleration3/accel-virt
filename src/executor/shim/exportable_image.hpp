/* One image of DXVK's Vulkan device and the memory bound to it, under a
 * Direct3D 11 texture of DXVK's that other APIs share (ExportableImages),
 * which holds it as private data (TextureHold) so the image outlives it; and
 * the DRM format modifier it is laid out by, which is what an importer of its
 * memory must be told. */
#ifndef AV_EXPORTABLE_IMAGE_HPP
#define AV_EXPORTABLE_IMAGE_HPP

#include "executor/shim/texture_hold.hpp"

#include <stdint.h>
#include <vulkan/vulkan.h>

namespace av {

class DxvkVulkanDevice;
class ExportableImages;

class ExportableImage final : public TextureHold {
public:
    /* `modifier` is DRM_FORMAT_MOD_INVALID for an image no modifier lays out
     * (one vkd3d-proton made), whose memory is no other API's to read. */
    ExportableImage(ExportableImages &owner, VkImage image, VkDeviceMemory memory, uint64_t modifier);

    VkImage        image() const;
    VkDeviceMemory memory() const;
    uint64_t       modifier() const;
    /* Laid out as rows (DRM_FORMAT_MOD_LINEAR), which an API told only a
     * pitch reads right. */
    bool           rows() const;
    /* The image and its memory destroyed: the GPU is done with them. */
    void           free(const DxvkVulkanDevice &vulkan);

private:
    VkImage        image_;
    VkDeviceMemory memory_;
    uint64_t       modifier_;
};

}  // namespace av

#endif  // AV_EXPORTABLE_IMAGE_HPP
