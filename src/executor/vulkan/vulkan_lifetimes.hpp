#ifndef AV_VULKAN_LIFETIMES_HPP
#define AV_VULKAN_LIFETIMES_HPP

#include "core/support/keyed_slots.hpp"
#include "core/support/object_pool.hpp"
#include "core/support/status.hpp"
#include "executor/object_holds.hpp"
#include "executor/vulkan/vulkan_destroy_kind.hpp"
#include "executor/vulkan/vulkan_node.hpp"

#include <stdint.h>

namespace av {

class BlockSource;
class VulkanDestroyer;

class VulkanLifetimes {
public:
    VulkanLifetimes(BlockSource &blocks, VulkanDestroyer &destroyer);
    VulkanLifetimes(const VulkanLifetimes &) = delete;
    VulkanLifetimes &operator=(const VulkanLifetimes &) = delete;

    [[nodiscard]] Status hold(uint64_t owner, uint64_t handle, uint64_t device, uint64_t pool);
    [[nodiscard]] Status track(uint64_t handle, uint64_t device);
    void                 submitted(uint64_t owner, uint64_t mark);
    [[nodiscard]] Status release(uint64_t owner, uint64_t reached);
    [[nodiscard]] Status destroy(VulkanDestroyKind kind, uint64_t device, uint64_t parent, uint64_t handle,
                                 uint64_t marked, uint64_t reached);
    [[nodiscard]] Status copy(uint64_t to, uint64_t from);
    void                 reap(uint64_t reached);
    void                 release_all();
    void                 finish(VulkanNode &node);

    bool     holding(uint64_t owner, uint64_t handle) const;
    bool     tracked(uint64_t handle) const;
    uint64_t mark_of(uint64_t owner) const;
    uint32_t nodes() const;
    uint32_t retired() const;

    bool invariants_hold() const;

private:
    using Holds = ObjectHolds<VulkanNode, void>;

    VulkanNode *node_for(uint64_t handle, uint64_t device, uint64_t parent);

    VulkanDestroyer             &destroyer_;
    ObjectPool<VulkanNode>       pool_;
    KeyedSlots<uint64_t, VulkanNode *> nodes_;
    Holds                        holds_;
    uint64_t                     reached_;
};

}

#endif
