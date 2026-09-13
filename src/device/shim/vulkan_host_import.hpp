/* A host page turned into Vulkan device memory, and back.
 *
 * The driver's half of VulkanBackend::adopt. Everything that decides whether an
 * import should happen - the handle's kind, whether a device exists - stays in
 * the backend where a test can judge it; what is here is only the three calls
 * the driver answers, whose failures depend on the driver and cannot be
 * provoked from a unit test without handing it memory it must not touch. */
#ifndef AV_VULKAN_HOST_IMPORT_HPP
#define AV_VULKAN_HOST_IMPORT_HPP

#include <stdint.h>
#include <vulkan/vulkan_core.h>

namespace av {

class VulkanHostImport {
public:
    VulkanHostImport();
    VulkanHostImport(const VulkanHostImport &) = delete;
    VulkanHostImport &operator=(const VulkanHostImport &) = delete;

    /* `size` bytes at `pointer` as memory of `device`, or false. The memory
     * type comes from vkGetMemoryHostPointerPropertiesEXT for this pointer on
     * this device and is never assumed: the mask was 0x7f on one GPU measured
     * and 0x18 on the other. */
    [[nodiscard]] bool import(VkDevice device, void *pointer, uint64_t size, uint64_t &memory);
    void               release(VkDevice device, uint64_t memory);

    /* Imports that succeeded and have not been released, which is what a test
     * or a teardown can ask about without a driver. */
    uint32_t held() const;

private:
    uint32_t held_;
};

}  // namespace av

#endif  // AV_VULKAN_HOST_IMPORT_HPP
