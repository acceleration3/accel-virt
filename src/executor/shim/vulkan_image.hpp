/* An image and the memory it alone is bound to, on one device
 * (VulkanDeviceTable): made there over memory exported as a dma-buf - the
 * compositor's images, the textures other APIs share (ExportableImage), the
 * display's (HostImageExport) - or made there over memory another device
 * exported - every API's view of one. Made from one description on every
 * device (ExportableImageInfo), so each reads one layout from the memory.
 * OpenGL and OpenCL open the same dma-buf through their own import
 * (GlHostImport). */
#ifndef AV_VULKAN_IMAGE_HPP
#define AV_VULKAN_IMAGE_HPP

#include <stdint.h>
#include <vulkan/vulkan_core.h>

namespace av {

class OwnedFd;
class VulkanDeviceTable;

class VulkanImage {
public:
    VulkanImage();
    ~VulkanImage();
    VulkanImage(const VulkanImage &) = delete;
    VulkanImage &operator=(const VulkanImage &) = delete;

    /* `info` made on `table`'s device, with no memory yet. */
    [[nodiscard]] bool   create(const VulkanDeviceTable &table, const VkImageCreateInfo &info);
    /* What the made image asks of its memory. */
    VkMemoryRequirements requirements() const;
    /* Memory of `type`, the image's alone and exportable as a dma-buf, bound
     * to it. On a refusal the image is released. */
    [[nodiscard]] bool   bind_exportable(uint32_t type);
    /* `size` bytes of `type` imported from `descriptor` as `handle`, the
     * image's alone, bound to it; the descriptor is the memory's once it is
     * made, and stays held in `descriptor` otherwise. On a refusal the image
     * is released. */
    [[nodiscard]] bool   bind_imported(OwnedFd &descriptor, VkExternalMemoryHandleTypeFlagBits handle,
                                       VkDeviceSize size, uint32_t type);

    /* `info` over memory of its own that exports as a dma-buf, of the type
     * the table prefers for images. */
    [[nodiscard]] bool make_exportable(const VulkanDeviceTable &table, const VkImageCreateInfo &info);
    /* `info` over the dma-buf `descriptor`, as make_exportable made the image
     * whose memory it is. */
    [[nodiscard]] bool import_dma_buf(const VulkanDeviceTable &table, const VkImageCreateInfo &info,
                                      OwnedFd &descriptor);
    /* A dma-buf of the memory, held in `descriptor`. */
    [[nodiscard]] bool export_dma_buf(OwnedFd &descriptor) const;
    /* The DRM format modifier the image is laid out by, which an importer of
     * its memory must be told; DRM_FORMAT_MOD_INVALID where the device cannot
     * say. */
    uint64_t            modifier() const;
    /* Where the first row of its one memory plane starts, and its pitch. */
    VkSubresourceLayout row_layout() const;
    /* The image and memory let go of; the device's work with them must be
     * done. */
    void                release();

    bool           made() const;
    VkImage        image() const;
    VkDeviceMemory memory() const;
    VkDeviceSize   size() const;

private:
    /* `memory_`, just allocated, bound; the image released on a refusal. */
    [[nodiscard]] bool bound(VkDeviceSize size);

    const VulkanDeviceTable *table_;
    VkImage                  image_;
    VkDeviceMemory           memory_;
    VkDeviceSize             size_;
};

}  // namespace av

#endif  // AV_VULKAN_IMAGE_HPP
