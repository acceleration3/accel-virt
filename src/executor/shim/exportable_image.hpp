/* One image of DXVK's Vulkan device and the memory bound to it (VulkanImage),
 * under a Direct3D 11 texture of DXVK's that other APIs share
 * (ExportableImages), which holds it as private data (TextureHold) so the
 * image outlives it; and the DRM format modifier it is laid out by, which is
 * what an importer of its memory must be told. */
#ifndef AV_EXPORTABLE_IMAGE_HPP
#define AV_EXPORTABLE_IMAGE_HPP

#include "executor/shim/texture_hold.hpp"
#include "executor/shim/vulkan_image.hpp"

#include <stdint.h>

namespace av {

class ExportableImages;

class ExportableImage final : public TextureHold {
public:
    /* Recorded before its image is made, so a texture's private data is
     * never anything but a live record. */
    explicit ExportableImage(ExportableImages &owner);

    /* The image, made by whoever recorded this. */
    VulkanImage       &image();
    const VulkanImage &image() const;
    /* The image is laid out by `modifier`: DRM_FORMAT_MOD_INVALID for one no
     * modifier lays out (one vkd3d-proton made), whose memory is no other
     * API's to read. */
    void               laid_out_by(uint64_t modifier);
    uint64_t           modifier() const;
    /* Laid out as rows (DRM_FORMAT_MOD_LINEAR), which an API told only a
     * pitch reads right. */
    bool               rows() const;
    /* The image and its memory destroyed: the GPU is done with them. */
    void               free();

private:
    VulkanImage image_;
    uint64_t    modifier_;
};

}  // namespace av

#endif  // AV_EXPORTABLE_IMAGE_HPP
