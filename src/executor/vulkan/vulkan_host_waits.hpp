#ifndef AV_VULKAN_HOST_WAITS_HPP
#define AV_VULKAN_HOST_WAITS_HPP

#include "core/support/growable_array.hpp"
#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class BlockSource;

class VulkanHostWaits {
public:
    explicit VulkanHostWaits(BlockSource &blocks);
    VulkanHostWaits(const VulkanHostWaits &) = delete;
    VulkanHostWaits &operator=(const VulkanHostWaits &) = delete;

    [[nodiscard]] Status record(uint64_t buffer, uint64_t event);
    [[nodiscard]] Status inherit(uint64_t primary, uint64_t secondary);
    void                 forget_buffer(uint64_t buffer);
    void                 forget_event(uint64_t event);
    void                 clear();

    template <typename Visitor>
    void each(uint64_t buffer, Visitor visit) const
    {
        for (const Wait &wait : waits_)
            if (wait.buffer_ == buffer) visit(wait.event_);
    }

    bool     waits(uint64_t buffer, uint64_t event) const;
    bool     any() const;
    uint32_t count() const;

    bool invariants_hold() const;

private:
    struct Wait {
        uint64_t buffer_;
        uint64_t event_;
    };

    GrowableArray<Wait> waits_;
};

}

#endif
