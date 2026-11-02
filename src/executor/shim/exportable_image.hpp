#ifndef AV_EXPORTABLE_IMAGE_HPP
#define AV_EXPORTABLE_IMAGE_HPP

#include "executor/shim/texture_hold.hpp"
#include "executor/shim/vulkan_image.hpp"

#include <stdint.h>

namespace av {

class ExportableImages;

class ExportableImage final : public TextureHold {
public:
    explicit ExportableImage(ExportableImages &owner);

    VulkanImage       &image();
    const VulkanImage &image() const;
    void               laid_out_by(uint64_t modifier);
    uint64_t           modifier() const;
    bool               rows() const;
    void               free();

private:
    VulkanImage image_;
    uint64_t    modifier_;
};

}

#endif
