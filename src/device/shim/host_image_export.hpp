/* A host GPU's images handed to QEMU's display as dma-bufs: a linear,
 * exportable image of the size and format being shown, which a scanout's
 * image is copied into on the GPU, so the display composes straight
 * from video memory - no readback into guest-sized CPU memory on QEMU's main
 * loop, and no upload back into a texture there. Made on DXVK's own Vulkan
 * device (DxvkVulkanDevice). */
#ifndef AV_HOST_IMAGE_EXPORT_HPP
#define AV_HOST_IMAGE_EXPORT_HPP

#include "device/abi/av_executor.h"

#include <stdint.h>
#include <vulkan/vulkan.h>

struct ID3D11Device5;
struct ID3D11DeviceContext4;
struct ID3D11Texture2D;
struct ID3D11Query;

namespace av {

class DxvkVulkanDevice;

class HostImageExport {
public:
    /* One, kept bound: switching the display between two exported images
     * every frame made gtk show a blank frame at each switch. */
    static constexpr uint32_t images = 1u;

    explicit HostImageExport(DxvkVulkanDevice &vulkan);
    ~HostImageExport();
    HostImageExport(const HostImageExport &) = delete;
    HostImageExport &operator=(const HostImageExport &) = delete;

    /* Under the device's lock: `source` copied into the next image, and the
     * copy queued. False where DXVK cannot export or `source` is no 32-bit
     * colour image. */
    [[nodiscard]] bool stage(ID3D11Device5 *device, ID3D11DeviceContext4 *context, ID3D11Texture2D *source,
                             uint32_t &slot);
    /* Under the device's lock: whether the GPU has finished the slot's copy. */
    bool copied(ID3D11DeviceContext4 *context, uint32_t slot) const;
    void describe(uint32_t slot, AvExportedImage &image) const;
    void release();

private:
    struct Image {
        VkImage          image_;
        VkDeviceMemory   memory_;
        ID3D11Texture2D *texture_;
        ID3D11Query     *done_;
        int              fd_;
        uint32_t         stride_;
        uint32_t         offset_;
    };

    [[nodiscard]] bool make(ID3D11Device5 *device, uint32_t width, uint32_t height, uint32_t format);
    [[nodiscard]] bool make_image(ID3D11Device5 *device, Image &image);
    void               drop(Image &image);

    DxvkVulkanDevice          &vulkan_;
    Image                      images_[images];
    uint32_t                   width_;
    uint32_t                   height_;
    uint32_t                   format_;
    uint32_t                   next_;
    bool                       unavailable_;
};

}  // namespace av

#endif  // AV_HOST_IMAGE_EXPORT_HPP
