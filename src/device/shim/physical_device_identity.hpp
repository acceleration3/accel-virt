/* Which host GPU a Vulkan physical device is: its deviceUUID, read once
 * through the instance that enumerated it, since a physical device means
 * nothing to any other. DXVK's DXGI and Direct3D 9 each hand out their own
 * instance and physical device for an adapter, and this is how the executor
 * puts a device on the GPU a guest's adapter stands for. */
#ifndef AV_PHYSICAL_DEVICE_IDENTITY_HPP
#define AV_PHYSICAL_DEVICE_IDENTITY_HPP

#include <stdint.h>
#include <vulkan/vulkan_core.h>

namespace av {

class PhysicalDeviceIdentity {
public:
    static constexpr uint32_t uuid_bytes = 16u;

    /* An instance with no vkGetPhysicalDeviceProperties2 is no GPU at all. */
    PhysicalDeviceIdentity(VkInstance instance, VkPhysicalDevice physical);

    /* Whether this is the GPU `uuid` names. */
    bool is(const uint8_t *uuid) const;

private:
    uint8_t uuid_[uuid_bytes];
    bool    known_;
};

}  // namespace av

#endif  // AV_PHYSICAL_DEVICE_IDENTITY_HPP
