/* One image vkd3d-proton exported (vkd3d_proton_export_image,
 * tools/vkd3d-proton-patches): the OPAQUE_FD of a shared committed texture's
 * memory, which this owns until it is taken, and what makes the same image on
 * another Vulkan device over that memory.
 *
 * Its leading members are vkd3d-proton's struct vkd3d_image_export, member for
 * member and in order, which the export writes; they are this project's own
 * declaration of it because vkd3d-proton's headers are its build's, not the
 * executor's. What follows them is this side's: the chain the image is made
 * again with. */
#ifndef AV_VKD3D_IMAGE_EXPORT_HPP
#define AV_VKD3D_IMAGE_EXPORT_HPP

#include <stdint.h>
#include <vulkan/vulkan.h>

namespace av {

class OwnedFd;

class Vkd3dImageExport {
public:
    static constexpr uint32_t max_view_formats = 16u;
    static constexpr uint32_t max_queue_families = 8u;

    Vkd3dImageExport();
    ~Vkd3dImageExport();
    Vkd3dImageExport(const Vkd3dImageExport &) = delete;
    Vkd3dImageExport &operator=(const Vkd3dImageExport &) = delete;

    /* The fd held in `memory` from now on, and none here. */
    void                     take(OwnedFd &memory);
    uint32_t                 memory_type() const;
    VkDeviceSize             size() const;
    /* The image as vkd3d-proton made it, chained to the view formats and the
     * external memory it is made over. Valid while this is. */
    const VkImageCreateInfo &image();
    /* Whether what vkd3d-proton wrote is an image that can be made again: a
     * count past its array would be a layout this does not match. */
    bool                     whole() const;

private:
    int                             fd_;
    uint32_t                        memory_type_;
    uint64_t                        size_;
    VkImageCreateInfo               image_;
    uint32_t                        view_format_count_;
    VkFormat                        view_formats_[max_view_formats];
    uint32_t                        queue_family_count_;
    uint32_t                        queue_families_[max_queue_families];
    VkExternalMemoryImageCreateInfo external_;
    VkImageFormatListCreateInfo     formats_;
};

}  // namespace av

#endif  // AV_VKD3D_IMAGE_EXPORT_HPP
