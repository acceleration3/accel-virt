#include "executor/vulkan/vulkan_timeline_submit.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

VulkanTimelineSubmit::VulkanTimelineSubmit(BlockSource &blocks)
    : submits_(blocks, Pool::non_paged), submits2_(blocks, Pool::non_paged), semaphores_(blocks, Pool::non_paged),
      values_(blocks, Pool::non_paged), waits_(blocks, Pool::non_paged), infos_(blocks, Pool::non_paged), timeline_{}
{
    AV_SCOPE("VulkanTimelineSubmit::VulkanTimelineSubmit");
}

bool VulkanTimelineSubmit::signalled(const VkSubmitInfo &last, VkSemaphore semaphore, uint64_t value)
{
    AV_SCOPE("VulkanTimelineSubmit::signalled");
    semaphores_.truncate(0u);
    values_.truncate(0u);
    waits_.truncate(0u);
    const VkTimelineSemaphoreSubmitInfo *guest = nullptr;
    for (auto *link = static_cast<const VkBaseInStructure *>(last.pNext); link != nullptr; link = link->pNext)
        if (link->sType == VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO)
            guest = reinterpret_cast<const VkTimelineSemaphoreSubmitInfo *>(link);
    for (uint32_t at = 0u; at < last.signalSemaphoreCount; at++) {
        const uint64_t given = guest != nullptr && at < guest->signalSemaphoreValueCount ? guest->pSignalSemaphoreValues[at] : 0u;
        if (semaphores_.append(last.pSignalSemaphores[at]) != Status::ok || values_.append(given) != Status::ok)
            return false;
    }
    if (semaphores_.append(semaphore) != Status::ok || values_.append(value) != Status::ok) return false;
    if (guest != nullptr) return true;
    for (uint32_t at = 0u; at < last.waitSemaphoreCount; at++)
        if (waits_.append(0u) != Status::ok) return false;
    return true;
}

const VkSubmitInfo *VulkanTimelineSubmit::append(const VkSubmitInfo *submits, uint32_t count, VkSemaphore semaphore,
                                                 uint64_t value, uint32_t &appended)
{
    AV_SCOPE("VulkanTimelineSubmit::append");
    submits_.truncate(0u);
    VkSubmitInfo last = {};
    last.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    for (uint32_t at = 0u; at + 1u < count; at++)
        if (submits_.append(submits[at]) != Status::ok) return nullptr;
    if (count != 0u) last = submits[count - 1u];
    if (!signalled(last, semaphore, value)) return nullptr;
    const VkTimelineSemaphoreSubmitInfo *guest = nullptr;
    for (auto *link = static_cast<const VkBaseInStructure *>(last.pNext); link != nullptr; link = link->pNext)
        if (link->sType == VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO)
            guest = reinterpret_cast<const VkTimelineSemaphoreSubmitInfo *>(link);
    if (guest != nullptr) {
        auto *const own = const_cast<VkTimelineSemaphoreSubmitInfo *>(guest);
        own->signalSemaphoreValueCount = values_.size();
        own->pSignalSemaphoreValues = values_.begin();
    } else {
        timeline_ = {};
        timeline_.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
        timeline_.pNext = last.pNext;
        timeline_.waitSemaphoreValueCount = waits_.size();
        timeline_.pWaitSemaphoreValues = waits_.begin();
        timeline_.signalSemaphoreValueCount = values_.size();
        timeline_.pSignalSemaphoreValues = values_.begin();
        last.pNext = &timeline_;
    }
    last.signalSemaphoreCount = semaphores_.size();
    last.pSignalSemaphores = semaphores_.begin();
    if (submits_.append(last) != Status::ok) return nullptr;
    appended = submits_.size();
    return submits_.begin();
}

const VkSubmitInfo2 *VulkanTimelineSubmit::append(const VkSubmitInfo2 *submits, uint32_t count, VkSemaphore semaphore,
                                                  uint64_t value, uint32_t &appended)
{
    AV_SCOPE("VulkanTimelineSubmit::append");
    submits2_.truncate(0u);
    infos_.truncate(0u);
    VkSubmitInfo2 last = {};
    last.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
    for (uint32_t at = 0u; at + 1u < count; at++)
        if (submits2_.append(submits[at]) != Status::ok) return nullptr;
    if (count != 0u) last = submits[count - 1u];
    for (uint32_t at = 0u; at < last.signalSemaphoreInfoCount; at++)
        if (infos_.append(last.pSignalSemaphoreInfos[at]) != Status::ok) return nullptr;
    VkSemaphoreSubmitInfo own = {};
    own.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
    own.semaphore = semaphore;
    own.value = value;
    own.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    if (infos_.append(own) != Status::ok) return nullptr;
    last.signalSemaphoreInfoCount = infos_.size();
    last.pSignalSemaphoreInfos = infos_.begin();
    if (submits2_.append(last) != Status::ok) return nullptr;
    appended = submits2_.size();
    return submits2_.begin();
}

}
