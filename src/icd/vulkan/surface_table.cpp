#include "icd/vulkan/surface_table.hpp"

#include "core/trace/function_scope.hpp"
#include "icd/vulkan/handle_bits.hpp"

namespace av::icd {

SurfaceTable::SurfaceTable() : slots_{}
{
    AV_SCOPE("SurfaceTable::SurfaceTable");
}

VkResult SurfaceTable::create(VkInstance instance, void *window, VkSurfaceKHR &surface)
{
    AV_SCOPE("SurfaceTable::create");
    surface = VK_NULL_HANDLE;
    if (window == nullptr) return VK_ERROR_INITIALIZATION_FAILED;
    for (SurfaceSlot &slot : slots_) {
        if (slot.window_ != nullptr) continue;
        slot.instance_ = instance;
        slot.window_ = window;
        surface = HandleBits(&slot).as<VkSurfaceKHR>();
        return VK_SUCCESS;
    }
    return VK_ERROR_OUT_OF_HOST_MEMORY;
}

void SurfaceTable::destroy(VkInstance instance, VkSurfaceKHR surface)
{
    AV_SCOPE("SurfaceTable::destroy");
    for (SurfaceSlot &slot : slots_) {
        if (HandleBits(&slot).as<VkSurfaceKHR>() != surface || slot.instance_ != instance) continue;
        slot = SurfaceSlot{};
    }
}

void SurfaceTable::forget(VkInstance instance)
{
    AV_SCOPE("SurfaceTable::forget");
    for (SurfaceSlot &slot : slots_)
        if (slot.instance_ == instance) slot = SurfaceSlot{};
}

void *SurfaceTable::window_of(VkSurfaceKHR surface) const
{
    AV_SCOPE("SurfaceTable::window_of");
    const SurfaceSlot *const slot = slot_of(surface);
    return slot != nullptr ? slot->window_ : nullptr;
}

uint32_t SurfaceTable::live() const
{
    AV_SCOPE("SurfaceTable::live");
    uint32_t count = 0u;
    for (const SurfaceSlot &slot : slots_) count += slot.window_ != nullptr ? 1u : 0u;
    return count;
}

/* A slot holds a window exactly when it names an instance. */
bool SurfaceTable::invariants_hold() const
{
    AV_SCOPE("SurfaceTable::invariants_hold");
    for (const SurfaceSlot &slot : slots_)
        if ((slot.window_ == nullptr) != (slot.instance_ == VK_NULL_HANDLE)) return false;
    return true;
}

/* Only an address inside the table, at a slot's start, is a surface. */
const SurfaceSlot *SurfaceTable::slot_of(VkSurfaceKHR surface) const
{
    AV_SCOPE("SurfaceTable::slot_of");
    for (const SurfaceSlot &slot : slots_)
        if (HandleBits(surface).as<const SurfaceSlot *>() == &slot) return &slot;
    return nullptr;
}

}  // namespace av::icd
