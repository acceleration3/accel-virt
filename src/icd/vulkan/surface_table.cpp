#include "icd/vulkan/surface_table.hpp"

#include "core/trace/function_scope.hpp"
#include "icd/vulkan/handle_bits.hpp"

namespace av::icd {

SurfaceTable::SurfaceTable(BlockSource &memory) : slots_(memory, Pool::paged), next_(1u)
{
    AV_SCOPE("SurfaceTable::SurfaceTable");
}

VkResult SurfaceTable::create(VkInstance instance, void *window, VkSurfaceKHR &surface)
{
    AV_SCOPE("SurfaceTable::create");
    surface = VK_NULL_HANDLE;
    if (window == nullptr) return VK_ERROR_INITIALIZATION_FAILED;
    uint32_t slot = 0u;
    if (slots_.insert(next_, SurfaceSlot{instance, window}, slot) != Status::ok) return VK_ERROR_OUT_OF_HOST_MEMORY;
    surface = HandleBits(next_++).as<VkSurfaceKHR>();
    return VK_SUCCESS;
}

void SurfaceTable::destroy(VkInstance instance, VkSurfaceKHR surface)
{
    AV_SCOPE("SurfaceTable::destroy");
    const uint64_t           key = HandleBits(surface).value();
    const SurfaceSlot *const slot = slots_.find(key);
    if (slot != nullptr && slot->instance_ == instance) static_cast<void>(slots_.erase(key));
}

void SurfaceTable::forget(VkInstance instance)
{
    AV_SCOPE("SurfaceTable::forget");
    slots_.erase_if([instance](uint64_t, SurfaceSlot &slot) { return slot.instance_ == instance; });
}

void *SurfaceTable::window_of(VkSurfaceKHR surface) const
{
    AV_SCOPE("SurfaceTable::window_of");
    const SurfaceSlot *const slot = slots_.find(HandleBits(surface).value());
    return slot != nullptr ? slot->window_ : nullptr;
}

uint32_t SurfaceTable::live() const
{
    AV_SCOPE("SurfaceTable::live");
    return slots_.count();
}

bool SurfaceTable::invariants_hold() const
{
    AV_SCOPE("SurfaceTable::invariants_hold");
    bool whole = true;
    slots_.for_each([this, &whole](uint64_t key, const SurfaceSlot &slot) {
        whole = whole && slot.window_ != nullptr && slot.instance_ != VK_NULL_HANDLE && key != 0u && key < next_;
    });
    return whole && slots_.invariants_hold();
}

}
