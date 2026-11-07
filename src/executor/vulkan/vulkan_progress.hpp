#ifndef AV_VULKAN_PROGRESS_HPP
#define AV_VULKAN_PROGRESS_HPP

#include <stdint.h>

namespace av {

class VulkanProgress {
public:
    virtual uint64_t marked() const = 0;
    virtual uint64_t reached() = 0;
    virtual uint64_t mark(uint64_t device, uint64_t queue) = 0;
    virtual void     wait_idle() = 0;

protected:
    VulkanProgress() = default;
    ~VulkanProgress() = default;
};

}

#endif
