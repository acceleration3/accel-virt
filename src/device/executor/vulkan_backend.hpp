/* One guest context's Vulkan, run on the host's own.
 *
 * The generated `RealVulkanDispatch` already forwards all 386 commands the
 * profile carries into the host loader, so what this adds is not forwarding:
 * it is the three answers the ABI needs and the generated code does not give -
 * whether an id is carried at all, whether a decode failed, and whether the
 * host device is gone. Deriving from the generated dispatch rather than
 * reimplementing it is also what makes widening feature 063's profile a
 * behaviour change instead of a build break. */
#ifndef AV_VULKAN_BACKEND_HPP
#define AV_VULKAN_BACKEND_HPP

#include "device/executor/session_backend.hpp"
#include "proto/generated/vulkan/vk_real_dispatch.h"

#include <stdint.h>

namespace av {

class ImportedBlob;

class VulkanBackend final : public vkproto::RealVulkanDispatch, public SessionBackend {
public:
    VulkanBackend();

    bool knows(uint32_t command_id) const override;
    [[nodiscard]] bool run(uint32_t command_id, WireReader &payload, WireWriter &reply,
                           WireArena &arena) override;
    bool lost() const override;

    /* The two commands this backend interposes rather than forwards
     * untouched: an imported blob has to become memory of some device, and
     * nothing else says which. */
    VkResult on_vkCreateDevice(VkPhysicalDevice physicalDevice,
                               const VkDeviceCreateInfo *pCreateInfo,
                               const VkAllocationCallbacks *pAllocator, VkDevice *pDevice) override;
    void     on_vkDestroyDevice(VkDevice device, const VkAllocationCallbacks *pAllocator) override;

    [[nodiscard]] bool adopt(ImportedBlob &blob) override;
    void abandon(ImportedBlob &blob) override;

    /* Commands run since this backend was made, which is what a conformance
     * run counts. */
    uint64_t ran() const;

private:
    /* Every Vulkan command id has this in its top 16 bits; the device filters
     * by API before calling, so anything else is the device disagreeing with
     * itself rather than a guest doing something. */
    static constexpr uint32_t vulkan_api = 1u;

    /* The memory type an imported pointer may be bound through, from
     * vkGetMemoryHostPointerPropertiesEXT and never assumed: the mask is 0x7f
     * on one GPU here and 0x18 on the other, so an imported pointer cannot be
     * device-local on the second. `found` is set only when one exists. */
    bool memory_type_for(void *pointer, uint32_t &type) const;

    /* The device the imports belong to. A VkDeviceMemory is a device's, and
     * the guest makes exactly one per context through vkCreateDevice, so this
     * is taken from the command that made it rather than guessed. */
    VkDevice device_;
    uint64_t ran_;
    bool     lost_;
};

}  // namespace av

#endif  // AV_VULKAN_BACKEND_HPP
