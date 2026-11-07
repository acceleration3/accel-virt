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
    case vkproto::HandleKind::sampler: return true;
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
    : progress_(progress), lifetimes_(blocks, destroyer), owner_(0u), recording_(false)
{
    AV_SCOPE("VulkanGuard::VulkanGuard");
}

void VulkanGuard::begin_frame(bool recording)
{
    AV_SCOPE("VulkanGuard::begin_frame");
    recording_ = recording;
    owner_ = 0u;
}

bool VulkanGuard::hold(uint64_t owner, uint64_t handle, uint64_t device, uint64_t pool)
{
    AV_SCOPE("VulkanGuard::hold");
    if (handle == 0u) return true;
    return lifetimes_.hold(owner, handle, device, pool) == Status::ok;
}

bool VulkanGuard::reference(vkproto::HandleKind kind, uint64_t wire, uint64_t parent)
{
    AV_SCOPE("VulkanGuard::reference");
    if (!recording_ || wire == 0u) return true;
    if (owner_ == 0u) {
        if (kind == vkproto::HandleKind::command_buffer) owner_ = wire;
        return true;
    }
    if (!holdable(kind)) return true;
    const bool pooled = kind == vkproto::HandleKind::descriptor_set || kind == vkproto::HandleKind::command_buffer;
    return hold(owner_, wire, 0u, pooled ? parent : 0u);
}

void VulkanGuard::reap()
{
    AV_SCOPE("VulkanGuard::reap");
    if (lifetimes_.retired() != 0u) lifetimes_.reap(progress_.reached());
}

void VulkanGuard::release_all()
{
    AV_SCOPE("VulkanGuard::release_all");
    progress_.wait_idle();
    lifetimes_.release_all();
}

bool VulkanGuard::release(uint64_t owner)
{
    AV_SCOPE("VulkanGuard::release");
    if (lifetimes_.release(owner, progress_.reached()) == Status::ok) return true;
    progress_.wait_idle();
    return lifetimes_.release(owner, progress_.reached()) == Status::ok;
}

void VulkanGuard::destroy(VulkanDestroyKind kind, uint64_t device, uint64_t parent, uint64_t handle)
{
    AV_SCOPE("VulkanGuard::destroy");
    const uint64_t marked = progress_.marked();
    if (lifetimes_.destroy(kind, device, parent, handle, marked, marked == 0u ? 0u : progress_.reached()) ==
        Status::ok)
        return;
    progress_.wait_idle();
    static_cast<void>(lifetimes_.destroy(kind, device, parent, handle, 0u, 0u));
}

bool VulkanGuard::depends(uint64_t owner, uint64_t handle, uint64_t device)
{
    AV_SCOPE("VulkanGuard::depends");
    if (handle == 0u) return true;
    return lifetimes_.track(owner, device) == Status::ok && hold(owner, handle, device, 0u);
}

bool VulkanGuard::written(const VkWriteDescriptorSet *writes, uint32_t count, uint64_t device)
{
    AV_SCOPE("VulkanGuard::written");
    for (uint32_t at = 0u; at < count; at++) {
        const VkWriteDescriptorSet &write = writes[at];
        const uint64_t              set = number(write.dstSet);
        if (set == 0u) continue;
        for (uint32_t index = 0u; index < write.descriptorCount; index++) {
            if (write.pImageInfo != nullptr &&
                (!hold(set, number(write.pImageInfo[index].imageView), device, 0u) ||
                 !hold(set, number(write.pImageInfo[index].sampler), device, 0u)))
                return false;
            if (write.pBufferInfo != nullptr && !hold(set, number(write.pBufferInfo[index].buffer), device, 0u))
                return false;
            if (write.pTexelBufferView != nullptr && !hold(set, number(write.pTexelBufferView[index]), device, 0u))
                return false;
        }
    }
    return true;
}

bool VulkanGuard::copied(const VkCopyDescriptorSet *copies, uint32_t count)
{
    AV_SCOPE("VulkanGuard::copied");
    for (uint32_t at = 0u; at < count; at++)
        if (lifetimes_.copy(number(copies[at].dstSet), number(copies[at].srcSet)) != Status::ok) return false;
    return true;
}

bool VulkanGuard::submitted(uint64_t device, uint64_t queue, const VkSubmitInfo *submits, uint32_t count)
{
    AV_SCOPE("VulkanGuard::submitted");
    const uint64_t mark = progress_.mark(device, queue);
    if (mark == 0u) return false;
    for (uint32_t at = 0u; at < count; at++)
        for (uint32_t index = 0u; index < submits[at].commandBufferCount; index++)
            lifetimes_.submitted(number(submits[at].pCommandBuffers[index]), mark);
    return true;
}

bool VulkanGuard::submitted(uint64_t device, uint64_t queue, const VkSubmitInfo2 *submits, uint32_t count)
{
    AV_SCOPE("VulkanGuard::submitted");
    const uint64_t mark = progress_.mark(device, queue);
    if (mark == 0u) return false;
    for (uint32_t at = 0u; at < count; at++)
        for (uint32_t index = 0u; index < submits[at].commandBufferInfoCount; index++)
            lifetimes_.submitted(number(submits[at].pCommandBufferInfos[index].commandBuffer), mark);
    return true;
}

void VulkanGuard::await_reset(uint64_t owner)
{
    AV_SCOPE("VulkanGuard::await_reset");
    if (lifetimes_.mark_of(owner) > progress_.reached()) progress_.wait_idle();
}

bool VulkanGuard::holding(uint64_t owner, uint64_t handle) const
{
    AV_SCOPE("VulkanGuard::holding");
    return lifetimes_.holding(owner, handle);
}

bool VulkanGuard::tracked(uint64_t handle) const
{
    AV_SCOPE("VulkanGuard::tracked");
    return lifetimes_.tracked(handle);
}

bool VulkanGuard::invariants_hold() const
{
    AV_SCOPE("VulkanGuard::invariants_hold");
    return lifetimes_.invariants_hold();
}

}
