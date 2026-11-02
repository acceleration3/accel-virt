#ifndef AV_PHYSICAL_DEVICE_IDENTITY_HPP
#define AV_PHYSICAL_DEVICE_IDENTITY_HPP

#include "proto/context/device_uuid.hpp"

#include <stdint.h>
#include <vulkan/vulkan_core.h>

namespace av {

class PhysicalDeviceIdentity {
public:
    PhysicalDeviceIdentity(VkInstance instance, VkPhysicalDevice physical);

    bool              is(const uint8_t *uuid) const;
    const DeviceUuid &uuid() const;
    int32_t           render_minor() const;

private:
    DeviceUuid uuid_;
    int32_t    render_minor_;
    bool       known_;
};

}

#endif
