#include "executor/vulkan/vulkan_guard.hpp"

#include "core/trace/function_scope.hpp"
#include "executor/vulkan/vulkan_destroyer.hpp"
#include "executor/vulkan/vulkan_progress.hpp"

namespace av {

namespace {

bool holdable(vkproto::HandleKind kind)
{
    AV_SCOPE("holdable");
    switch (kind) {
    case vkproto::HandleKind::buffer:
    case vkproto::HandleKind::acceleration_structure_khr:
    case vkproto::HandleKind::buffer_view:
    case vkproto::HandleKind::command_buffer:
    case vkproto::HandleKind::descriptor_set:
    case vkproto::HandleKind::event:
    case vkproto::HandleKind::framebuffer:
    case vkproto::HandleKind::image:
    case vkproto::HandleKind::image_view:
    case vkproto::HandleKind::pipeline:
    case vkproto::HandleKind::pipeline_layout:
    case vkproto::HandleKind::query_pool:
    case vkproto::HandleKind::render_pass:
    case vkproto::HandleKind::sampler:
    case vkproto::HandleKind::shader_ext:
    case vkproto::HandleKind::indirect_commands_layout_ext:
    case vkproto::HandleKind::indirect_execution_set_ext: return true;
    default: return false;
    }
}

uint64_t number(const void *handle)
{
    AV_SCOPE("number");
    return reinterpret_cast<uint64_t>(handle);
}

}

VulkanGuard::VulkanGuard(BlockSource &blocks, VulkanDestroyer &destroyer, VulkanProgress &progress)
    : blocks_(blocks), destroyer_(destroyer), progress_(progress), pool_(blocks), devices_(blocks, Pool::paged),
      owner_(0u), owner_device_(0u), recording_(false)
{
    AV_SCOPE("VulkanGuard::VulkanGuard");
}

VulkanLifetimes *VulkanGuard::lifetimes_of(uint64_t device) const
{
    AV_SCOPE("VulkanGuard::lifetimes_of");
    VulkanLifetimes *const *const found = devices_.find(device);
    return found != nullptr ? *found : nullptr;
}

VulkanLifetimes *VulkanGuard::lifetimes_for(uint64_t device)
{
    AV_SCOPE("VulkanGuard::lifetimes_for");
    VulkanLifetimes *const known = lifetimes_of(device);
    if (known != nullptr) return known;
    VulkanLifetimes *made = nullptr;
    if (pool_.create(made, blocks_, destroyer_) != Status::ok) return nullptr;
    uint32_t slot = 0u;
    if (devices_.insert(device, made, slot) == Status::ok) return made;
    static_cast<void>(pool_.destroy(made));
    return nullptr;
}

void VulkanGuard::begin_frame(bool recording)
{
    AV_SCOPE("VulkanGuard::begin_frame");
    recording_ = recording;
    owner_ = 0u;
    owner_device_ = 0u;
}

bool VulkanGuard::drain(TeardownBudget &budget)
{
    AV_SCOPE("VulkanGuard::drain");
    bool drained = true;
    devices_.for_each([&](uint64_t device, VulkanLifetimes *&) { drained = progress_.drain(device, budget) && drained; });
    return drained;
}

void VulkanGuard::end_batch()
{
    AV_SCOPE("VulkanGuard::end_batch");
    progress_.begin_poll();
}

bool VulkanGuard::hold(uint64_t owner, uint64_t handle, uint64_t device, uint64_t pool)
{
    AV_SCOPE("VulkanGuard::hold");
    if (handle == 0u) return true;
    VulkanLifetimes *const lifetimes = lifetimes_for(device);
    return lifetimes != nullptr && lifetimes->hold(owner, handle, device, pool) == Status::ok;
}

bool VulkanGuard::pooled(vkproto::HandleKind kind) const
{
    AV_SCOPE("VulkanGuard::pooled");
    return kind == vkproto::HandleKind::descriptor_set || kind == vkproto::HandleKind::command_buffer;
}

VulkanGuard::Needs VulkanGuard::needs(vkproto::HandleKind kind, uint64_t wire) const
{
    AV_SCOPE("VulkanGuard::needs");
    if (!recording_ || wire == 0u) return Needs{false, false, false};
    if (owner_ == 0u) {
        const bool owns = kind == vkproto::HandleKind::command_buffer;
        return Needs{owns, false, owns};
    }
    return Needs{holdable(kind), holdable(kind) && pooled(kind), false};
}

bool VulkanGuard::reference(vkproto::HandleKind kind, uint64_t wire, uint64_t parent, uint64_t device)
{
    AV_SCOPE("VulkanGuard::reference");
    if (!recording_ || wire == 0u) return true;
    if (owner_ == 0u) {
        if (kind != vkproto::HandleKind::command_buffer) return true;
        owner_ = wire;
        owner_device_ = device;
        return true;
    }
    if (!holdable(kind)) return true;
    return hold(owner_, wire, owner_device_, pooled(kind) ? parent : 0u);
}

void VulkanGuard::reap()
{
    AV_SCOPE("VulkanGuard::reap");
    devices_.for_each([this](uint64_t device, VulkanLifetimes *&lifetimes) {
        if (lifetimes->retired() != 0u) lifetimes->reap(progress_.reached(device));
    });
}

bool VulkanGuard::release_all(uint64_t device)
{
    AV_SCOPE("VulkanGuard::release_all");
    const bool       idle = progress_.wait_idle(device);
    VulkanLifetimes *const lifetimes = lifetimes_of(device);
    if (lifetimes == nullptr) return idle;
    lifetimes->release_all(idle);
    static_cast<void>(devices_.erase(device));
    static_cast<void>(pool_.destroy(lifetimes));
    return idle;
}

bool VulkanGuard::release(uint64_t owner, uint64_t device)
{
    AV_SCOPE("VulkanGuard::release");
    VulkanLifetimes *const lifetimes = lifetimes_of(device);
    if (lifetimes == nullptr) return true;
    if (lifetimes->release(owner, progress_.reached(device)) == Status::ok) return true;
    if (!progress_.wait_idle(device)) return false;
    return lifetimes->release(owner, progress_.reached(device)) == Status::ok;
}

void VulkanGuard::orphan_children(uint64_t pool, uint64_t device)
{
    AV_SCOPE("VulkanGuard::orphan_children");
    VulkanLifetimes *const lifetimes = lifetimes_of(device);
    if (lifetimes != nullptr) lifetimes->orphan_children(pool);
}

void VulkanGuard::destroy(VulkanDestroyKind kind, uint64_t device, uint64_t parent, uint64_t handle)
{
    AV_SCOPE("VulkanGuard::destroy");
    VulkanLifetimes *const lifetimes = lifetimes_for(device);
    const uint64_t         marked = progress_.marked();
    if (lifetimes != nullptr &&
        lifetimes->destroy(kind, device, parent, handle, marked, marked == 0u ? 0u : progress_.reached(device)) ==
            Status::ok)
        return;
    if (!progress_.wait_idle(device)) return;
    if (lifetimes != nullptr) static_cast<void>(lifetimes->destroy(kind, device, parent, handle, 0u, 0u));
    else destroyer_.destroy(kind, device, parent, handle);
}

bool VulkanGuard::depends(uint64_t owner, uint64_t handle, uint64_t device)
{
    AV_SCOPE("VulkanGuard::depends");
    if (handle == 0u) return true;
    VulkanLifetimes *const lifetimes = lifetimes_for(device);
    return lifetimes != nullptr && lifetimes->track(owner, device) == Status::ok && hold(owner, handle, device, 0u);
}

bool VulkanGuard::bound(uint64_t set, uint32_t binding, uint32_t element, uint64_t first, uint64_t second,
                        uint64_t device)
{
    AV_SCOPE("VulkanGuard::bound");
    if (set == 0u) return true;
    VulkanLifetimes *const lifetimes = lifetimes_for(device);
    return lifetimes != nullptr && bind(set, binding, element, first, second, *lifetimes, device, progress_.reached(device));
}

bool VulkanGuard::bind(uint64_t set, uint32_t binding, uint32_t element, uint64_t first, uint64_t second,
                       VulkanLifetimes &lifetimes, uint64_t device, uint64_t reached)
{
    AV_SCOPE("VulkanGuard::bind");
    return lifetimes.write(set, VulkanLifetimes::Descriptor{binding, element}, first, second, device, reached) == Status::ok;
}

bool VulkanGuard::written(const VkWriteDescriptorSet *writes, uint32_t count, uint64_t device)
{
    AV_SCOPE("VulkanGuard::written");
    VulkanLifetimes *lifetimes = nullptr;
    uint64_t         reached = 0u;
    for (uint32_t at = 0u; at < count; at++) {
        const VkWriteDescriptorSet &write = writes[at];
        const uint64_t              set = number(write.dstSet);
        if (set == 0u) continue;
        for (uint32_t index = 0u; index < write.descriptorCount; index++) {
            uint64_t first = 0u;
            uint64_t second = 0u;
            switch (write.descriptorType) {
            case VK_DESCRIPTOR_TYPE_SAMPLER:
                if (write.pImageInfo != nullptr) first = number(write.pImageInfo[index].sampler);
                break;
            case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
                if (write.pImageInfo != nullptr) {
                    first = number(write.pImageInfo[index].imageView);
                    second = number(write.pImageInfo[index].sampler);
                }
                break;
            case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
            case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
            case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
                if (write.pImageInfo != nullptr) first = number(write.pImageInfo[index].imageView);
                break;
            case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
            case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
                if (write.pTexelBufferView != nullptr) first = number(write.pTexelBufferView[index]);
                break;
            case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
            case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
            case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
            case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
                if (write.pBufferInfo != nullptr) first = number(write.pBufferInfo[index].buffer);
                break;
            case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR: {
                const auto *next = static_cast<const VkBaseInStructure *>(write.pNext);
                while (next != nullptr && next->sType != VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR)
                    next = next->pNext;
                const auto *const structures = reinterpret_cast<const VkWriteDescriptorSetAccelerationStructureKHR *>(next);
                if (structures != nullptr && index < structures->accelerationStructureCount)
                    first = number(structures->pAccelerationStructures[index]);
                break;
            }
            default: continue;
            }
            if (lifetimes == nullptr) {
                lifetimes = lifetimes_for(device);
                if (lifetimes == nullptr) return false;
                reached = progress_.reached(device);
            }
            if (!bind(set, write.dstBinding, write.dstArrayElement + index, first, second, *lifetimes, device, reached))
                return false;
        }
    }
    return true;
}

bool VulkanGuard::copied(const VkCopyDescriptorSet *copies, uint32_t count, uint64_t device)
{
    AV_SCOPE("VulkanGuard::copied");
    VulkanLifetimes *const lifetimes = lifetimes_for(device);
    if (lifetimes == nullptr) return count == 0u;
    const uint64_t reached = count != 0u ? progress_.reached(device) : 0u;
    for (uint32_t at = 0u; at < count; at++) {
        const VkCopyDescriptorSet &copy = copies[at];
        const Status               done = lifetimes->copy(
            number(copy.dstSet), VulkanLifetimes::Descriptor{copy.dstBinding, copy.dstArrayElement}, number(copy.srcSet),
            VulkanLifetimes::Descriptor{copy.srcBinding, copy.srcArrayElement}, copy.descriptorCount, reached);
        if (done != Status::ok) return false;
    }
    return true;
}

uint64_t VulkanGuard::reserve(uint64_t device)
{
    AV_SCOPE("VulkanGuard::reserve");
    if (lifetimes_for(device) == nullptr) return 0u;
    return progress_.reserve(device);
}

void VulkanGuard::cancel(uint64_t mark)
{
    AV_SCOPE("VulkanGuard::cancel");
    progress_.cancel(mark);
}

void VulkanGuard::submitted(uint64_t mark, uint64_t device, uint64_t queue, const VkSubmitInfo *submits, uint32_t count,
                            uint64_t value)
{
    AV_SCOPE("VulkanGuard::submitted");
    if (value != 0u) progress_.commit_timeline(mark, queue, value);
    else progress_.commit(mark, queue);
    held(mark, device, submits, count);
}

void VulkanGuard::held(uint64_t mark, uint64_t device, const VkSubmitInfo *submits, uint32_t count)
{
    AV_SCOPE("VulkanGuard::held");
    VulkanLifetimes *const lifetimes = lifetimes_of(device);
    if (lifetimes == nullptr) return;
    for (uint32_t at = 0u; at < count; at++)
        for (uint32_t index = 0u; index < submits[at].commandBufferCount; index++)
            lifetimes->submitted(number(submits[at].pCommandBuffers[index]), mark);
}

void VulkanGuard::submitted(uint64_t mark, uint64_t device, uint64_t queue, const VkSubmitInfo2 *submits, uint32_t count,
                            uint64_t value)
{
    AV_SCOPE("VulkanGuard::submitted");
    if (value != 0u) progress_.commit_timeline(mark, queue, value);
    else progress_.commit(mark, queue);
    held(mark, device, submits, count);
}

void VulkanGuard::sparse_bound(uint64_t mark, uint64_t queue, uint64_t value)
{
    AV_SCOPE("VulkanGuard::sparse_bound");
    if (value != 0u) progress_.commit_timeline(mark, queue, value);
    else progress_.commit(mark, queue);
}

bool VulkanGuard::timeline(uint64_t device, uint64_t queue, VkSemaphore &semaphore, uint64_t &value)
{
    AV_SCOPE("VulkanGuard::timeline");
    return progress_.timeline(device, queue, semaphore, value);
}

bool VulkanGuard::timelined(uint64_t device) const
{
    AV_SCOPE("VulkanGuard::timelined");
    return progress_.timelined(device);
}

void VulkanGuard::held(uint64_t mark, uint64_t device, const VkSubmitInfo2 *submits, uint32_t count)
{
    AV_SCOPE("VulkanGuard::held");
    VulkanLifetimes *const lifetimes = lifetimes_of(device);
    if (lifetimes == nullptr) return;
    for (uint32_t at = 0u; at < count; at++)
        for (uint32_t index = 0u; index < submits[at].commandBufferInfoCount; index++)
            lifetimes->submitted(number(submits[at].pCommandBufferInfos[index].commandBuffer), mark);
}

bool VulkanGuard::await_reset(uint64_t owner, uint64_t device)
{
    AV_SCOPE("VulkanGuard::await_reset");
    VulkanLifetimes *const lifetimes = lifetimes_of(device);
    if (lifetimes == nullptr) return true;
    const uint64_t mark = lifetimes->mark_of(owner);
    if (mark <= progress_.reached(device)) return true;
    return progress_.wait_marks(device, &mark, 1u);
}

bool VulkanGuard::quiesce_marks(const uint64_t *marks, uint32_t count, uint64_t device)
{
    AV_SCOPE("VulkanGuard::quiesce_marks");
    const uint64_t reached = progress_.reached(device);
    for (uint32_t index = 0u; index < count; index++)
        if (marks[index] > reached) return progress_.wait_marks(device, marks, count);
    return true;
}

bool VulkanGuard::reset_pending(uint64_t pool, uint64_t device) const
{
    AV_SCOPE("VulkanGuard::reset_pending");
    const VulkanLifetimes *const lifetimes = lifetimes_of(device);
    return lifetimes != nullptr && lifetimes->reset_pending(pool);
}

uint64_t VulkanGuard::marked() const
{
    AV_SCOPE("VulkanGuard::marked");
    return progress_.marked();
}

uint64_t VulkanGuard::reached(uint64_t device)
{
    AV_SCOPE("VulkanGuard::reached");
    return progress_.reached(device);
}

void VulkanGuard::defer_pool_reset(uint64_t pool, uint64_t device, uint64_t mark)
{
    AV_SCOPE("VulkanGuard::defer_pool_reset");
    VulkanLifetimes *const lifetimes = lifetimes_for(device);
    if (lifetimes == nullptr) return;
    static_cast<void>(lifetimes->destroy(VulkanDestroyKind::descriptor_pool_reset, device, 0u, pool, mark,
                                         progress_.reached(device)));
}

uint64_t VulkanGuard::mark_of(uint64_t owner, uint64_t device) const
{
    AV_SCOPE("VulkanGuard::mark_of");
    const VulkanLifetimes *const lifetimes = lifetimes_of(device);
    return lifetimes != nullptr ? lifetimes->mark_of(owner) : 0u;
}

bool VulkanGuard::quiesce(uint64_t device)
{
    AV_SCOPE("VulkanGuard::quiesce");
    return progress_.wait_idle(device);
}

bool VulkanGuard::holding(uint64_t owner, uint64_t handle) const
{
    AV_SCOPE("VulkanGuard::holding");
    bool named = false;
    devices_.for_each([&](uint64_t, VulkanLifetimes *const &lifetimes) {
        named = named || lifetimes->holding(owner, handle);
    });
    return named;
}

bool VulkanGuard::tracked(uint64_t handle) const
{
    AV_SCOPE("VulkanGuard::tracked");
    bool named = false;
    devices_.for_each([&](uint64_t, VulkanLifetimes *const &lifetimes) { named = named || lifetimes->tracked(handle); });
    return named;
}

bool VulkanGuard::linked(uint64_t handle, uint64_t pool) const
{
    AV_SCOPE("VulkanGuard::linked");
    bool named = false;
    devices_.for_each([&](uint64_t, VulkanLifetimes *const &lifetimes) { named = named || lifetimes->linked(handle, pool); });
    return named;
}

bool VulkanGuard::described(uint64_t set, uint32_t binding, uint32_t element, uint64_t handle) const
{
    AV_SCOPE("VulkanGuard::described");
    bool named = false;
    devices_.for_each([&](uint64_t, VulkanLifetimes *const &lifetimes) {
        named = named || lifetimes->described(set, VulkanLifetimes::Descriptor{binding, element}, handle);
    });
    return named;
}

uint32_t VulkanGuard::devices() const
{
    AV_SCOPE("VulkanGuard::devices");
    return devices_.count();
}

bool VulkanGuard::lost() const
{
    AV_SCOPE("VulkanGuard::lost");
    return progress_.lost();
}

bool VulkanGuard::invariants_hold() const
{
    AV_SCOPE("VulkanGuard::invariants_hold");
    if (!pool_.invariants_hold() || !devices_.invariants_hold() || pool_.live() != devices_.count()) return false;
    bool sound = true;
    devices_.for_each([&sound](uint64_t, VulkanLifetimes *const &lifetimes) {
        sound = sound && lifetimes != nullptr && lifetimes->invariants_hold();
    });
    return sound;
}

}
