#ifndef AV_VULKAN_GUARD_HPP
#define AV_VULKAN_GUARD_HPP

#include "core/support/status.hpp"
#include "executor/vulkan/vulkan_destroy_kind.hpp"
#include "executor/vulkan/vulkan_lifetimes.hpp"
#include "proto/generated/vulkan/vk_handle_translation.h"

#include <vulkan/vulkan.h>

#include <stdint.h>

namespace av {

class BlockSource;
class VulkanDestroyer;
class VulkanProgress;

class VulkanGuard {
public:
    VulkanGuard(BlockSource &blocks, VulkanDestroyer &destroyer, VulkanProgress &progress);
    VulkanGuard(const VulkanGuard &) = delete;
    VulkanGuard &operator=(const VulkanGuard &) = delete;

    void               begin_frame(bool recording);
    [[nodiscard]] bool reference(vkproto::HandleKind kind, uint64_t wire, uint64_t parent);
    void               reap();
    void               release_all();

    [[nodiscard]] bool release(uint64_t owner);
    void               destroy(VulkanDestroyKind kind, uint64_t device, uint64_t parent, uint64_t handle);
    [[nodiscard]] bool depends(uint64_t owner, uint64_t handle, uint64_t device);
    [[nodiscard]] bool written(const VkWriteDescriptorSet *writes, uint32_t count, uint64_t device);
    [[nodiscard]] bool copied(const VkCopyDescriptorSet *copies, uint32_t count);
    [[nodiscard]] bool submitted(uint64_t device, uint64_t queue, const VkSubmitInfo *submits, uint32_t count);
    [[nodiscard]] bool submitted(uint64_t device, uint64_t queue, const VkSubmitInfo2 *submits, uint32_t count);
    void               await_reset(uint64_t owner);

    bool holding(uint64_t owner, uint64_t handle) const;
    bool tracked(uint64_t handle) const;

    bool invariants_hold() const;

private:
    [[nodiscard]] bool hold(uint64_t owner, uint64_t handle, uint64_t device, uint64_t pool);
    [[nodiscard]] bool mark_all(uint64_t device, uint64_t queue, const uint64_t *buffers, uint32_t count);

    VulkanProgress  &progress_;
    VulkanLifetimes  lifetimes_;
    uint64_t         owner_;
    bool             recording_;
};

}

#endif
