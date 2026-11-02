#ifndef AV_EXPORTED_IMAGE_LAYOUT_HPP
#define AV_EXPORTED_IMAGE_LAYOUT_HPP

#include "device/abi/av_executor.h"

#include <stdint.h>

namespace av {

class VulkanImage;

class ExportedImageLayout {
public:
    ExportedImageLayout(const VulkanImage &image, uint32_t width, uint32_t height, uint32_t format,
                        uint64_t modifier);
    ExportedImageLayout(const ExportedImageLayout &) = delete;
    ExportedImageLayout &operator=(const ExportedImageLayout &) = delete;

    [[nodiscard]] bool describe(int fd, AvExportedImage &described) const;

private:
    const VulkanImage &image_;
    uint32_t           width_;
    uint32_t           height_;
    uint32_t           format_;
    uint64_t           modifier_;
};

}

#endif
