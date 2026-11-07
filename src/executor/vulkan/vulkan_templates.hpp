#ifndef AV_VULKAN_TEMPLATES_HPP
#define AV_VULKAN_TEMPLATES_HPP

#include "core/support/growable_array.hpp"
#include "core/support/keyed_slots.hpp"
#include "core/support/object_pool.hpp"
#include "core/support/status.hpp"
#include "core/trace/function_scope.hpp"
#include "proto/generated/vulkan/vk_handle_translation.h"

#include <vulkan/vulkan.h>

#include <stdint.h>

namespace av {

class BlockSource;

class VulkanTemplates {
public:
    struct Reference {
        vkproto::HandleKind kind_;
        uint64_t            handle_;
        bool                named_;
    };

    struct Descriptor {
        uint32_t  binding_;
        uint32_t  element_;
        Reference first_;
        Reference second_;
    };

    explicit VulkanTemplates(BlockSource &blocks);
    VulkanTemplates(const VulkanTemplates &) = delete;
    VulkanTemplates &operator=(const VulkanTemplates &) = delete;

    [[nodiscard]] Status take(uint64_t made, uint64_t device, const VkDescriptorUpdateTemplateEntry *entries,
                              uint32_t count);
    void                 forget(uint64_t made);
    void                 forget_device(uint64_t device);
    void                 clear();

    template <typename Visitor>
    [[nodiscard]] bool each(uint64_t made, const void *data, uint64_t bytes, Visitor visit) const
    {
        AV_SCOPE("VulkanTemplates::each");
        Entries *const *const found = templates_.find(made);
        if (found == nullptr) return false;
        const Entries &listed = **found;
        for (uint32_t entry = 0u; entry < listed.entries_.size(); entry++) {
            const VkDescriptorUpdateTemplateEntry &named = listed.entries_.at(entry);
            const uint32_t                         repeats = named.descriptorType == VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK ? 0u : named.descriptorCount;
            for (uint32_t index = 0u; index < repeats; index++) {
                Descriptor described = {};
                if (describe(named, index, data, bytes, described) != Status::ok) return false;
                if ((described.first_.named_ || described.second_.named_) && !visit(described)) return false;
            }
        }
        return true;
    }

    bool     known(uint64_t made) const;
    uint32_t count() const;

    bool invariants_hold() const;

private:
    struct Entries {
        Entries(BlockSource &blocks, uint64_t device);

        GrowableArray<VkDescriptorUpdateTemplateEntry> entries_;
        uint64_t                                       device_;
    };

    Status describe(const VkDescriptorUpdateTemplateEntry &entry, uint32_t index, const void *data, uint64_t bytes,
                    Descriptor &described) const;

    BlockSource                      &blocks_;
    ObjectPool<Entries>               pool_;
    KeyedSlots<uint64_t, Entries *>   templates_;
};

}

#endif
