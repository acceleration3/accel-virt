#ifndef AV_VULKAN_GUARD_HPP
#define AV_VULKAN_GUARD_HPP

#include "core/support/keyed_slots.hpp"
#include "core/support/object_pool.hpp"
#include "core/support/status.hpp"
#include "executor/vulkan/vulkan_destroy_kind.hpp"
#include "executor/vulkan/vulkan_lifetimes.hpp"
#include "proto/generated/vulkan/vk_handle_translation.h"

#include <vulkan/vulkan.h>

#include <stdint.h>

namespace av {

class BlockSource;
class VulkanDestroyer;
class TeardownBudget;
class VulkanProgress;

class VulkanGuard {
public:
    VulkanGuard(BlockSource &blocks, VulkanDestroyer &destroyer, VulkanProgress &progress);
    VulkanGuard(const VulkanGuard &) = delete;
    VulkanGuard &operator=(const VulkanGuard &) = delete;

    void               begin_frame(bool recording);
    [[nodiscard]] bool reference(vkproto::HandleKind kind, uint64_t wire, uint64_t parent, uint64_t device);
    void               reap();
    [[nodiscard]] bool release_all(uint64_t device);

    [[nodiscard]] bool release(uint64_t owner, uint64_t device);
    void               orphan_children(uint64_t pool, uint64_t device);
    void               destroy(VulkanDestroyKind kind, uint64_t device, uint64_t parent, uint64_t handle);
    [[nodiscard]] bool depends(uint64_t owner, uint64_t handle, uint64_t device);
    [[nodiscard]] bool written(const VkWriteDescriptorSet *writes, uint32_t count, uint64_t device);
    [[nodiscard]] bool bound(uint64_t set, uint32_t binding, uint32_t element, uint64_t first, uint64_t second,
                             uint64_t device);
    [[nodiscard]] bool copied(const VkCopyDescriptorSet *copies, uint32_t count, uint64_t device);
    [[nodiscard]] uint64_t reserve(uint64_t device);
    void                   cancel(uint64_t mark);
    void                   submitted(uint64_t mark, uint64_t device, uint64_t queue, const VkSubmitInfo *submits,
                                     uint32_t count);
    void                   submitted(uint64_t mark, uint64_t device, uint64_t queue, const VkSubmitInfo2 *submits,
                                     uint32_t count);
    void               held(uint64_t mark, uint64_t device, const VkSubmitInfo *submits, uint32_t count);
    void               held(uint64_t mark, uint64_t device, const VkSubmitInfo2 *submits, uint32_t count);
    [[nodiscard]] bool await_reset(uint64_t owner, uint64_t device);
    [[nodiscard]] bool quiesce(uint64_t device);
    [[nodiscard]] bool quiesce_marks(const uint64_t *marks, uint32_t count, uint64_t device);
    bool               reset_pending(uint64_t pool, uint64_t device) const;
    uint64_t           reached(uint64_t device);
    uint64_t           marked() const;
    void               defer_pool_reset(uint64_t pool, uint64_t device, uint64_t mark);
    uint64_t           mark_of(uint64_t owner, uint64_t device) const;
    void               end_batch();
    [[nodiscard]] bool drain(TeardownBudget &budget);

    bool holding(uint64_t owner, uint64_t handle) const;
    bool tracked(uint64_t handle) const;
    bool linked(uint64_t handle, uint64_t pool) const;
    bool described(uint64_t set, uint32_t binding, uint32_t element, uint64_t handle) const;
    uint32_t devices() const;
    bool     lost() const;

    bool invariants_hold() const;

private:
    [[nodiscard]] bool hold(uint64_t owner, uint64_t handle, uint64_t device, uint64_t pool);
    VulkanLifetimes   *lifetimes_of(uint64_t device) const;
    VulkanLifetimes   *lifetimes_for(uint64_t device);

    BlockSource                       &blocks_;
    VulkanDestroyer                   &destroyer_;
    VulkanProgress                    &progress_;
    ObjectPool<VulkanLifetimes>        pool_;
    KeyedSlots<uint64_t, VulkanLifetimes *> devices_;
    uint64_t                           owner_;
    uint64_t                           owner_device_;
    bool                               recording_;
};

}

#endif
