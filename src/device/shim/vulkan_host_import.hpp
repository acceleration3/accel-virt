/* A host page turned into Vulkan device memory, and back.
 *
 * The driver's half of VulkanBackend::adopt. Everything that decides whether an
 * import should happen - the handle's kind, whether a device exists, whether
 * the type asked for is one this pointer may be imported into - stays in the
 * backend where a test can judge it; what is here is only the calls the driver
 * answers, whose failures depend on the driver and cannot be provoked from a
 * unit test without handing it memory it must not touch. */
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

    /* The memory types `pointer` may be imported into on `device`, as
     * vkGetMemoryHostPointerPropertiesEXT answers them: 0x7f on one GPU
     * measured and 0x18 on the other, so never assumed. 0 when the device has
     * no such entry point or the driver refuses the pointer. */
    uint32_t           importable_types(VkDevice device, void *pointer);
    /* `size` bytes at `pointer` as memory of `device` in `memory_type`, or
     * false. */
    [[nodiscard]] bool import(VkDevice device, void *pointer, uint64_t size, uint32_t memory_type,
                              uint64_t &memory);
    void               release(VkDevice device, uint64_t memory);

    /* Imports that succeeded and have not been released, which is what a test
     * or a teardown can ask about without a driver. */
    uint32_t held() const;

private:
    uint32_t held_;
};

}  // namespace av

#endif  // AV_VULKAN_HOST_IMPORT_HPP
