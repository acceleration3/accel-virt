#ifndef AV_VULKAN_PROGRESS_HPP
#define AV_VULKAN_PROGRESS_HPP

#include <stdint.h>

namespace av {

class TeardownBudget;

class VulkanProgress {
public:
    virtual uint64_t marked() const = 0;
    virtual uint64_t reached(uint64_t device) = 0;
    virtual uint64_t reserve(uint64_t device) = 0;
    virtual void     commit(uint64_t mark, uint64_t queue) = 0;
    virtual void     cancel(uint64_t mark) = 0;
    virtual bool     wait_idle(uint64_t device) = 0;
    virtual bool     wait_marks(uint64_t device, const uint64_t *marks, uint32_t count) = 0;
    virtual bool     drain(uint64_t device, TeardownBudget &budget) = 0;
    virtual void     begin_poll() = 0;
    virtual bool     lost() const = 0;

protected:
    VulkanProgress() = default;
    ~VulkanProgress() = default;
};

}

#endif
