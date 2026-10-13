/* An image and the memory it alone is bound to, on one device
 * (VulkanDeviceTable): either made there over memory exported as a dma-buf -
 * the compositor's images - or made there over a dma-buf another device
 * exported - every API's view of one. Made from one description on every
 * device (ExportableImageInfo, linear), so each reads one layout from the
 * memory. OpenGL and OpenCL open the same dma-buf through their own import
 * (GlHostImport). */
#ifndef AV_VULKAN_IMAGE_HPP
#define AV_VULKAN_IMAGE_HPP

#include <vulkan/vulkan_core.h>

namespace av {

class VulkanDeviceTable;

class VulkanImage {
public:
    VulkanImage();
    ~VulkanImage();
    VulkanImage(const VulkanImage &) = delete;
    VulkanImage &operator=(const VulkanImage &) = delete;

    /* `info` on `table`'s device over memory of its own that exports as a
     * dma-buf. */
    [[nodiscard]] bool make_exportable(const VulkanDeviceTable &table, const VkImageCreateInfo &info);
    /* `info` on `table`'s device over the dma-buf `fd`, which is this
     * image's whatever happens: kept by the memory, or closed. */
    [[nodiscard]] bool import_dma_buf(const VulkanDeviceTable &table, const VkImageCreateInfo &info, int fd);
    /* A dma-buf of the memory, the caller's to close. */
    [[nodiscard]] bool export_dma_buf(int &fd) const;
    /* The image and memory let go of; the device's work with them must be
     * done. */
    void               release();

    bool           made() const;
    VkImage        image() const;
    VkDeviceMemory memory() const;
    VkDeviceSize   size() const;

private:
    /* False with `took` saying whether memory was allocated - and so took an
     * imported fd - before something refused. */
    [[nodiscard]] bool bind(const VulkanDeviceTable &table, const VkImageCreateInfo &info, const void *chain,
                            bool &took);

    const VulkanDeviceTable *table_;
    VkImage                  image_;
    VkDeviceMemory           memory_;
    VkDeviceSize             size_;
};

}  // namespace av

#endif  // AV_VULKAN_IMAGE_HPP
