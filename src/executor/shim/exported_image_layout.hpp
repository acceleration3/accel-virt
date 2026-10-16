/* One exportable image described as a dma-buf importer must be told it - a
 * process outside QEMU (docs/external-export.md), QEMU's display: its size,
 * the DRM format of its pixels, the modifier it is laid out by, and where its
 * one plane starts and its pitch. Both images handed out of the executor are
 * described here: a shared texture's (ExportableImages::export_image) and the
 * display's (HostImageExport). */
#ifndef AV_EXPORTED_IMAGE_LAYOUT_HPP
#define AV_EXPORTED_IMAGE_LAYOUT_HPP

#include "device/abi/av_executor.h"

#include <stdint.h>

namespace av {

class VulkanImage;

class ExportedImageLayout {
public:
    /* `image`, `width` by `height` texels of the DXGI `format`, laid out by
     * `modifier`. */
    ExportedImageLayout(const VulkanImage &image, uint32_t width, uint32_t height, uint32_t format,
                        uint64_t modifier);
    ExportedImageLayout(const ExportedImageLayout &) = delete;
    ExportedImageLayout &operator=(const ExportedImageLayout &) = delete;

    /* The image over the descriptor `fd`, into `described`. False, with
     * nothing written, for a format no DRM format names. */
    [[nodiscard]] bool describe(int fd, AvExportedImage &described) const;

private:
    const VulkanImage &image_;
    uint32_t           width_;
    uint32_t           height_;
    uint32_t           format_;
    uint64_t           modifier_;
};

}  // namespace av

#endif  // AV_EXPORTED_IMAGE_LAYOUT_HPP
