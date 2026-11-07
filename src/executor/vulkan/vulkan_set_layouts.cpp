#include "executor/vulkan/vulkan_set_layouts.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

VulkanSetLayouts::VulkanSetLayouts(BlockSource &blocks) : blocks_(blocks), pool_(blocks), layouts_(blocks, Pool::paged)
{
    AV_SCOPE("VulkanSetLayouts::VulkanSetLayouts");
}

VulkanSetLayouts::Layout::Layout(BlockSource &blocks, uint64_t device)
    : bindings_(blocks, Pool::paged), sets_(blocks, Pool::paged), device_(device)
{
    AV_SCOPE("VulkanSetLayouts::Layout::Layout");
}

Status VulkanSetLayouts::keep(uint64_t made, Layout *&layout, uint64_t device)
{
    AV_SCOPE("VulkanSetLayouts::keep");
    if (made == 0u) return Status::invalid_parameter;
    forget(made);
    layout = nullptr;
    if (pool_.create(layout, blocks_, device) != Status::ok) return Status::no_memory;
    uint32_t slot = 0u;
    const Status inserted = layouts_.insert(made, layout, slot);
    if (inserted != Status::ok) static_cast<void>(pool_.destroy(layout));
    return inserted;
}

Status VulkanSetLayouts::take_layout(uint64_t made, uint64_t device, const VkDescriptorSetLayoutBinding *bindings,
                                     uint32_t count)
{
    AV_SCOPE("VulkanSetLayouts::take_layout");
    if (bindings == nullptr && count != 0u) return Status::invalid_parameter;
    Layout *layout = nullptr;
    const Status made_room = keep(made, layout, device);
    if (made_room != Status::ok) return made_room;
    Status kept = Status::ok;
    for (uint32_t index = 0u; index < count && kept == Status::ok; index++) {
        const VkDescriptorSetLayoutBinding &named = bindings[index];
        const bool sampled = named.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER ||
                             named.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        if (sampled && named.pImmutableSamplers != nullptr) kept = layout->bindings_.append(named.binding);
    }
    if (kept != Status::ok) forget(made);
    return kept;
}

Status VulkanSetLayouts::take_pipeline(uint64_t made, uint64_t device, const VkDescriptorSetLayout *layouts,
                                       uint32_t count)
{
    AV_SCOPE("VulkanSetLayouts::take_pipeline");
    if (layouts == nullptr && count != 0u) return Status::invalid_parameter;
    Layout *layout = nullptr;
    const Status made_room = keep(made, layout, device);
    if (made_room != Status::ok) return made_room;
    Status kept = Status::ok;
    for (uint32_t index = 0u; index < count && kept == Status::ok; index++)
        kept = layout->sets_.append(reinterpret_cast<uint64_t>(layouts[index]));
    if (kept != Status::ok) forget(made);
    return kept;
}

void VulkanSetLayouts::forget(uint64_t made)
{
    AV_SCOPE("VulkanSetLayouts::forget");
    Layout *layout = nullptr;
    if (layouts_.take(made, layout)) static_cast<void>(pool_.destroy(layout));
}

void VulkanSetLayouts::forget_device(uint64_t device)
{
    AV_SCOPE("VulkanSetLayouts::forget_device");
    layouts_.erase_if([&](uint64_t, Layout *&layout) {
        if (layout->device_ != device) return false;
        static_cast<void>(pool_.destroy(layout));
        return true;
    });
}

void VulkanSetLayouts::clear()
{
    AV_SCOPE("VulkanSetLayouts::clear");
    layouts_.clear();
    pool_.destroy_all();
}

const uint32_t *VulkanSetLayouts::immutable(uint64_t layout, uint32_t &count) const
{
    AV_SCOPE("VulkanSetLayouts::immutable");
    count = 0u;
    Layout *const *const found = layouts_.find(layout);
    if (found == nullptr || (*found)->bindings_.size() == 0u) return nullptr;
    count = (*found)->bindings_.size();
    return (*found)->bindings_.begin();
}

uint64_t VulkanSetLayouts::layout_of(uint64_t pipeline, uint32_t set) const
{
    AV_SCOPE("VulkanSetLayouts::layout_of");
    Layout *const *const found = layouts_.find(pipeline);
    if (found == nullptr || set >= (*found)->sets_.size()) return 0u;
    return (*found)->sets_.at(set);
}

uint32_t VulkanSetLayouts::count() const
{
    AV_SCOPE("VulkanSetLayouts::count");
    return layouts_.count();
}

bool VulkanSetLayouts::invariants_hold() const
{
    AV_SCOPE("VulkanSetLayouts::invariants_hold");
    if (!pool_.invariants_hold() || !layouts_.invariants_hold() || pool_.live() != layouts_.count()) return false;
    bool sound = true;
    layouts_.for_each([&sound](uint64_t, Layout *const &layout) {
        sound = sound && layout != nullptr && layout->bindings_.invariants_hold() && layout->sets_.invariants_hold();
    });
    return sound;
}

}
