#ifndef AV_VULKAN_PARK_HOST_HPP
#define AV_VULKAN_PARK_HOST_HPP

#include <stddef.h>
#include <stdint.h>

namespace av {

class VulkanParkHost {
public:
    virtual bool event_unset(uint64_t device, uint64_t event) = 0;
    virtual bool replay(uint32_t command, const uint8_t *frame, size_t bytes) = 0;
    virtual void release_mark(uint64_t mark) = 0;
    virtual void lose() = 0;

protected:
    VulkanParkHost() = default;
    ~VulkanParkHost() = default;
};

}

#endif
