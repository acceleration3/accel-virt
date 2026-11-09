#include "executor/vulkan/vulkan_backend.hpp"

#include "core/log/driver_log.hpp"
#include "core/trace/function_scope.hpp"
#include "executor/vulkan/vulkan_submit_set.hpp"
#include "proto/generated/vulkan/vk_dispatch_server.h"
#include "proto/wire/wire_arena.hpp"
#include "proto/wire/wire_writer.hpp"

namespace av {

namespace {

uint64_t number(const void *handle)
{
    AV_SCOPE("number");
    return reinterpret_cast<uint64_t>(handle);
}

}

bool VulkanBackend::event_unset(uint64_t device, uint64_t event)
{
    AV_SCOPE("VulkanBackend::event_unset");
    return RealVulkanDispatch::on_vkGetEventStatus(reinterpret_cast<VkDevice>(device),
                                                   reinterpret_cast<VkEvent>(event)) == VK_EVENT_RESET;
}

bool VulkanBackend::replay(uint32_t command, const uint8_t *frame, size_t bytes)
{
    AV_SCOPE("VulkanBackend::replay");
    WireReader reader(frame, bytes);
    WireWriter reply(nullptr, 0u);
    WireArena  arena(nullptr, 0u);
    arena.overflow_into(blocks_);
    const uint64_t kept = parent_;
    replaying_ = true;
    last_submit_ = VK_SUCCESS;
    guard_.begin_frame(false);
    const bool decoded = vkproto::dispatch_command(*this, command, reader, reply, arena);
    replaying_ = false;
    parent_ = kept;
    return decoded && last_submit_ == VK_SUCCESS;
}

void VulkanBackend::release_mark(uint64_t mark)
{
    AV_SCOPE("VulkanBackend::release_mark");
    guard_.cancel(mark);
}

void VulkanBackend::lose()
{
    AV_SCOPE("VulkanBackend::lose");
    lines_.write("a parked submission waited too long for its event; the device is lost");
    lost_ = true;
}

Status VulkanBackend::gather(const VkSubmitInfo *submits, uint32_t count, VulkanSubmitSet &set) const
{
    AV_SCOPE("VulkanBackend::gather");
    Status gathered = Status::ok;
    for (uint32_t at = 0u; at < count && gathered == Status::ok; at++) {
        const VkSubmitInfo &submit = submits[at];
        for (uint32_t index = 0u; index < submit.waitSemaphoreCount && gathered == Status::ok; index++)
            gathered = set.add_wait(number(submit.pWaitSemaphores[index]));
        for (uint32_t index = 0u; index < submit.signalSemaphoreCount && gathered == Status::ok; index++)
            gathered = set.add_signal(number(submit.pSignalSemaphores[index]));
        for (uint32_t index = 0u; index < submit.commandBufferCount && gathered == Status::ok; index++)
            host_waits_.each(number(submit.pCommandBuffers[index]), [&](uint64_t event) {
                if (gathered == Status::ok) gathered = set.add_event(event);
            });
    }
    return gathered;
}

Status VulkanBackend::gather(const VkSubmitInfo2 *submits, uint32_t count, VulkanSubmitSet &set) const
{
    AV_SCOPE("VulkanBackend::gather");
    Status gathered = Status::ok;
    for (uint32_t at = 0u; at < count && gathered == Status::ok; at++) {
        const VkSubmitInfo2 &submit = submits[at];
        for (uint32_t index = 0u; index < submit.waitSemaphoreInfoCount && gathered == Status::ok; index++)
            gathered = set.add_wait(number(submit.pWaitSemaphoreInfos[index].semaphore));
        for (uint32_t index = 0u; index < submit.signalSemaphoreInfoCount && gathered == Status::ok; index++)
            gathered = set.add_signal(number(submit.pSignalSemaphoreInfos[index].semaphore));
        for (uint32_t index = 0u; index < submit.commandBufferInfoCount && gathered == Status::ok; index++)
            host_waits_.each(number(submit.pCommandBufferInfos[index].commandBuffer), [&](uint64_t event) {
                if (gathered == Status::ok) gathered = set.add_event(event);
            });
    }
    return gathered;
}

Status VulkanBackend::gather(const VkBindSparseInfo *binds, uint32_t count, VulkanSubmitSet &set) const
{
    AV_SCOPE("VulkanBackend::gather");
    Status gathered = Status::ok;
    for (uint32_t at = 0u; at < count && gathered == Status::ok; at++) {
        const VkBindSparseInfo &bind = binds[at];
        for (uint32_t index = 0u; index < bind.waitSemaphoreCount && gathered == Status::ok; index++)
            gathered = set.add_wait(number(bind.pWaitSemaphores[index]));
        for (uint32_t index = 0u; index < bind.signalSemaphoreCount && gathered == Status::ok; index++)
            gathered = set.add_signal(number(bind.pSignalSemaphores[index]));
    }
    return gathered;
}

bool VulkanBackend::events_unset(uint64_t device, const VulkanSubmitSet &set)
{
    AV_SCOPE("VulkanBackend::events_unset");
    for (uint32_t index = 0u; index < set.event_count(); index++)
        if (event_unset(device, set.events()[index])) return true;
    return false;
}

template <typename Submit>
VulkanBackend::Hold VulkanBackend::hold_back(VkQueue queue, uint64_t device, uint64_t mark, const Submit *submits,
                                             uint32_t count)
{
    AV_SCOPE("VulkanBackend::hold_back");
    if (replaying_ || !framed_ || (!parked_.pending() && !host_waits_.any())) return Hold::proceed;
    VulkanSubmitSet set(blocks_);
    if (gather(submits, count, set) != Status::ok) return Hold::proceed;
    if (!parked_.behind(number(queue), set.waits(), set.wait_count()) && !events_unset(device, set))
        return Hold::proceed;
    const VulkanParkedSubmits::Request request{number(queue), device, mark, frame_command_, &frame_, &set};
    if (parked_.park(request) != Status::ok) {
        lost_ = true;
        return Hold::refused;
    }
    if constexpr (!__is_same(Submit, VkBindSparseInfo)) guard_.held(mark, device, submits, count);
    return Hold::parked;
}

template <typename Submit>
bool VulkanBackend::stamp(VkQueue queue, uint64_t device, const Submit *submits, uint32_t count, Stamp<Submit> &out)
{
    AV_SCOPE("VulkanBackend::stamp");
    out = Stamp<Submit>{submits, count, VK_NULL_HANDLE, 0u};
    if (!guard_.timelined(device)) return true;
    if (!guard_.timeline(device, number(queue), out.semaphore_, out.value_)) return false;
    out.sent_ = timeline_submit_.append(submits, count, out.semaphore_, out.value_, out.count_);
    return out.sent_ != nullptr;
}

void VulkanBackend::note_fence(VkFence fence, VkSemaphore semaphore, uint64_t value)
{
    AV_SCOPE("VulkanBackend::note_fence");
    if (fence != VK_NULL_HANDLE && value != 0u) fence_targets_.note(fence, semaphore, value);
}

VkResult VulkanBackend::on_vkQueueSubmit(VkQueue queue, uint32_t submitCount, const VkSubmitInfo *pSubmits,
                                         VkFence fence)
{
    AV_SCOPE("VulkanBackend::on_vkQueueSubmit");
    const uint64_t device = handles_.parent_of(number(queue));
    const uint64_t mark = guard_.reserve(device);
    if (mark == 0u) return VK_ERROR_OUT_OF_HOST_MEMORY;
    const Hold hold = hold_back(queue, device, mark, pSubmits, submitCount);
    if (hold != Hold::proceed) {
        if (hold == Hold::parked) return VK_SUCCESS;
        guard_.cancel(mark);
        return VK_ERROR_DEVICE_LOST;
    }
    Stamp<VkSubmitInfo> sent;
    if (!stamp(queue, device, pSubmits, submitCount, sent)) {
        guard_.cancel(mark);
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }
    const VkResult result = timing_.enabled() ? timing_.submit(queue, sent.count_, sent.sent_, fence)
                                              : RealVulkanDispatch::on_vkQueueSubmit(queue, sent.count_, sent.sent_, fence);
    last_submit_ = result;
    if (result != VK_SUCCESS) {
        guard_.cancel(mark);
        return result;
    }
    guard_.submitted(mark, device, number(queue), pSubmits, submitCount, sent.value_);
    note_fence(fence, sent.semaphore_, sent.value_);
    return result;
}

VkResult VulkanBackend::on_vkQueueSubmit2(VkQueue queue, uint32_t submitCount, const VkSubmitInfo2 *pSubmits,
                                          VkFence fence)
{
    AV_SCOPE("VulkanBackend::on_vkQueueSubmit2");
    const uint64_t device = handles_.parent_of(number(queue));
    const uint64_t mark = guard_.reserve(device);
    if (mark == 0u) return VK_ERROR_OUT_OF_HOST_MEMORY;
    const Hold hold = hold_back(queue, device, mark, pSubmits, submitCount);
    if (hold != Hold::proceed) {
        if (hold == Hold::parked) return VK_SUCCESS;
        guard_.cancel(mark);
        return VK_ERROR_DEVICE_LOST;
    }
    Stamp<VkSubmitInfo2> sent;
    if (!stamp(queue, device, pSubmits, submitCount, sent)) {
        guard_.cancel(mark);
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }
    const VkResult result = RealVulkanDispatch::on_vkQueueSubmit2(queue, sent.count_, sent.sent_, fence);
    last_submit_ = result;
    if (result != VK_SUCCESS) {
        guard_.cancel(mark);
        return result;
    }
    guard_.submitted(mark, device, number(queue), pSubmits, submitCount, sent.value_);
    note_fence(fence, sent.semaphore_, sent.value_);
    return result;
}

VkResult VulkanBackend::on_vkQueueSubmit2KHR(VkQueue queue, uint32_t submitCount, const VkSubmitInfo2 *pSubmits,
                                             VkFence fence)
{
    AV_SCOPE("VulkanBackend::on_vkQueueSubmit2KHR");
    const uint64_t device = handles_.parent_of(number(queue));
    const uint64_t mark = guard_.reserve(device);
    if (mark == 0u) return VK_ERROR_OUT_OF_HOST_MEMORY;
    const Hold hold = hold_back(queue, device, mark, pSubmits, submitCount);
    if (hold != Hold::proceed) {
        if (hold == Hold::parked) return VK_SUCCESS;
        guard_.cancel(mark);
        return VK_ERROR_DEVICE_LOST;
    }
    Stamp<VkSubmitInfo2> sent;
    if (!stamp(queue, device, pSubmits, submitCount, sent)) {
        guard_.cancel(mark);
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }
    const VkResult result = RealVulkanDispatch::on_vkQueueSubmit2KHR(queue, sent.count_, sent.sent_, fence);
    last_submit_ = result;
    if (result != VK_SUCCESS) {
        guard_.cancel(mark);
        return result;
    }
    guard_.submitted(mark, device, number(queue), pSubmits, submitCount, sent.value_);
    note_fence(fence, sent.semaphore_, sent.value_);
    return result;
}

VkResult VulkanBackend::on_vkQueueBindSparse(VkQueue queue, uint32_t bindInfoCount, const VkBindSparseInfo *pBindInfo,
                                             VkFence fence)
{
    AV_SCOPE("VulkanBackend::on_vkQueueBindSparse");
    const uint64_t device = handles_.parent_of(number(queue));
    const uint64_t mark = guard_.reserve(device);
    if (mark == 0u) return VK_ERROR_OUT_OF_HOST_MEMORY;
    const Hold hold = hold_back(queue, device, mark, pBindInfo, bindInfoCount);
    if (hold != Hold::proceed) {
        if (hold == Hold::parked) return VK_SUCCESS;
        guard_.cancel(mark);
        return VK_ERROR_DEVICE_LOST;
    }
    Stamp<VkBindSparseInfo> sent;
    if (!stamp(queue, device, pBindInfo, bindInfoCount, sent)) {
        guard_.cancel(mark);
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }
    const VkResult result = RealVulkanDispatch::on_vkQueueBindSparse(queue, sent.count_, sent.sent_, fence);
    last_submit_ = result;
    if (result != VK_SUCCESS) {
        guard_.cancel(mark);
        return result;
    }
    guard_.sparse_bound(mark, number(queue), sent.value_);
    note_fence(fence, sent.semaphore_, sent.value_);
    return result;
}

VkResult VulkanBackend::on_vkSetEvent(VkDevice device, VkEvent event)
{
    AV_SCOPE("VulkanBackend::on_vkSetEvent");
    const VkResult set = RealVulkanDispatch::on_vkSetEvent(device, event);
    if (set == VK_SUCCESS) parked_.service();
    return set;
}

void VulkanBackend::note_waits(VkCommandBuffer commandBuffer, uint32_t count, const VkEvent *events)
{
    AV_SCOPE("VulkanBackend::note_waits");
    for (uint32_t index = 0u; index < count; index++)
        if (host_waits_.record(number(commandBuffer), number(events[index])) != Status::ok) lost_ = true;
}

void VulkanBackend::on_vkCmdWaitEvents(VkCommandBuffer commandBuffer, uint32_t eventCount, const VkEvent *pEvents,
                                       VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask,
                                       uint32_t memoryBarrierCount, const VkMemoryBarrier *pMemoryBarriers,
                                       uint32_t bufferMemoryBarrierCount,
                                       const VkBufferMemoryBarrier *pBufferMemoryBarriers,
                                       uint32_t imageMemoryBarrierCount, const VkImageMemoryBarrier *pImageMemoryBarriers)
{
    AV_SCOPE("VulkanBackend::on_vkCmdWaitEvents");
    if ((srcStageMask & VK_PIPELINE_STAGE_HOST_BIT) != 0u) note_waits(commandBuffer, eventCount, pEvents);
    RealVulkanDispatch::on_vkCmdWaitEvents(commandBuffer, eventCount, pEvents, srcStageMask, dstStageMask,
                                           memoryBarrierCount, pMemoryBarriers, bufferMemoryBarrierCount,
                                           pBufferMemoryBarriers, imageMemoryBarrierCount, pImageMemoryBarriers);
}

bool VulkanBackend::host_settable(const VkDependencyInfo &dependency) const
{
    AV_SCOPE("VulkanBackend::host_settable");
    if (dependency.memoryBarrierCount + dependency.bufferMemoryBarrierCount + dependency.imageMemoryBarrierCount == 0u)
        return true;
    for (uint32_t index = 0u; index < dependency.memoryBarrierCount; index++)
        if ((dependency.pMemoryBarriers[index].srcStageMask & VK_PIPELINE_STAGE_2_HOST_BIT) != 0u) return true;
    for (uint32_t index = 0u; index < dependency.bufferMemoryBarrierCount; index++)
        if ((dependency.pBufferMemoryBarriers[index].srcStageMask & VK_PIPELINE_STAGE_2_HOST_BIT) != 0u) return true;
    for (uint32_t index = 0u; index < dependency.imageMemoryBarrierCount; index++)
        if ((dependency.pImageMemoryBarriers[index].srcStageMask & VK_PIPELINE_STAGE_2_HOST_BIT) != 0u) return true;
    return false;
}

void VulkanBackend::wait_events2(VkCommandBuffer commandBuffer, uint32_t count, const VkEvent *events,
                                 const VkDependencyInfo *dependencies)
{
    AV_SCOPE("VulkanBackend::wait_events2");
    for (uint32_t index = 0u; index < count; index++)
        if (host_settable(dependencies[index])) note_waits(commandBuffer, 1u, &events[index]);
}

void VulkanBackend::on_vkCmdWaitEvents2(VkCommandBuffer commandBuffer, uint32_t eventCount, const VkEvent *pEvents,
                                        const VkDependencyInfo *pDependencyInfos)
{
    AV_SCOPE("VulkanBackend::on_vkCmdWaitEvents2");
    wait_events2(commandBuffer, eventCount, pEvents, pDependencyInfos);
    RealVulkanDispatch::on_vkCmdWaitEvents2(commandBuffer, eventCount, pEvents, pDependencyInfos);
}

void VulkanBackend::on_vkCmdWaitEvents2KHR(VkCommandBuffer commandBuffer, uint32_t eventCount, const VkEvent *pEvents,
                                           const VkDependencyInfo *pDependencyInfos)
{
    AV_SCOPE("VulkanBackend::on_vkCmdWaitEvents2KHR");
    wait_events2(commandBuffer, eventCount, pEvents, pDependencyInfos);
    RealVulkanDispatch::on_vkCmdWaitEvents2KHR(commandBuffer, eventCount, pEvents, pDependencyInfos);
}

void VulkanBackend::on_vkCmdExecuteCommands(VkCommandBuffer commandBuffer, uint32_t commandBufferCount,
                                            const VkCommandBuffer *pCommandBuffers)
{
    AV_SCOPE("VulkanBackend::on_vkCmdExecuteCommands");
    if (host_waits_.any())
        for (uint32_t index = 0u; index < commandBufferCount; index++)
            if (host_waits_.inherit(number(commandBuffer), number(pCommandBuffers[index])) != Status::ok) lost_ = true;
    RealVulkanDispatch::on_vkCmdExecuteCommands(commandBuffer, commandBufferCount, pCommandBuffers);
}

void VulkanBackend::forget_waits_under(uint64_t pool)
{
    AV_SCOPE("VulkanBackend::forget_waits_under");
    if (!host_waits_.any()) return;
    handles_.for_each_child(pool, [this](uint64_t child) { host_waits_.forget_buffer(child); });
}

}
