/* Which host GPU a Vulkan physical device is: its deviceUUID and its DRM
 * render node, read once through the instance that enumerated it, since a
 * physical device means nothing to any other. DXVK's DXGI and Direct3D 9
 * each hand out their own instance and physical device for an adapter, and
 * this is how the executor puts a device on the GPU a guest's adapter stands
 * for - and how it lists the host's GPUs in the first place. */
#ifndef AV_PHYSICAL_DEVICE_IDENTITY_HPP
#define AV_PHYSICAL_DEVICE_IDENTITY_HPP

#include "proto/context/device_uuid.hpp"

#include <stdint.h>
#include <vulkan/vulkan_core.h>

namespace av {

class PhysicalDeviceIdentity {
public:
    /* An instance with no vkGetPhysicalDeviceProperties2 is no GPU at all. */
    PhysicalDeviceIdentity(VkInstance instance, VkPhysicalDevice physical);

    /* Whether this is the GPU `uuid` names. */
    bool              is(const uint8_t *uuid) const;
    /* Its deviceUUID; zeroes when it is no GPU. */
    const DeviceUuid &uuid() const;
    /* Its DRM render node's minor, -1 for none (no VK_EXT_physical_device_drm). */
    int32_t           render_minor() const;

private:
    DeviceUuid uuid_;
    int32_t    render_minor_;
    bool       known_;
};

}  // namespace av

#endif  // AV_PHYSICAL_DEVICE_IDENTITY_HPP
