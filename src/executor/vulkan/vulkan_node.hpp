#ifndef AV_VULKAN_NODE_HPP
#define AV_VULKAN_NODE_HPP

#include "executor/vulkan/vulkan_destroy_kind.hpp"

#include <stdint.h>

namespace av {

class VulkanLifetimes;

class VulkanNode {
public:
    VulkanNode(VulkanLifetimes &owner, uint64_t handle, uint64_t device, uint64_t parent);
    VulkanNode(const VulkanNode &) = delete;
    VulkanNode &operator=(const VulkanNode &) = delete;

    void AddRef();
    void Release();
    void request(VulkanDestroyKind kind, uint64_t device, uint64_t parent);

    uint64_t          handle() const;
    uint64_t          device() const;
    uint64_t          parent() const;
    VulkanDestroyKind kind() const;
    uint32_t          references() const;

    bool invariants_hold() const;

private:
    VulkanLifetimes  &owner_;
    uint64_t          handle_;
    uint64_t          device_;
    uint64_t          parent_;
    uint32_t          references_;
    VulkanDestroyKind kind_;
};

}

#endif
