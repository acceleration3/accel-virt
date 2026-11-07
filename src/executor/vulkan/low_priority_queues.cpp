#include "executor/vulkan/low_priority_queues.hpp"

#include "core/trace/function_scope.hpp"

#include <string.h>

namespace av {

namespace {

constexpr uint32_t most_extensions = 512u;

}

LowPriorityQueues::LowPriorityQueues() : infos_{}, priorities_{}, extension_(nullptr)
{
    AV_SCOPE("LowPriorityQueues::LowPriorityQueues");
}

bool LowPriorityQueues::offered_by(VkPhysicalDevice physical)
{
    AV_SCOPE("LowPriorityQueues::offered_by");
    extension_ = nullptr;
    uint32_t count = 0u;
    if (::vkEnumerateDeviceExtensionProperties(physical, nullptr, &count, nullptr) != VK_SUCCESS || count == 0u ||
        count > most_extensions)
        return false;
    VkExtensionProperties listed[most_extensions];
    if (::vkEnumerateDeviceExtensionProperties(physical, nullptr, &count, listed) != VK_SUCCESS) return false;
    for (uint32_t at = 0u; at < count; at++) {
        if (strcmp(listed[at].extensionName, VK_KHR_GLOBAL_PRIORITY_EXTENSION_NAME) == 0) {
            extension_ = VK_KHR_GLOBAL_PRIORITY_EXTENSION_NAME;
            return true;
        }
        if (strcmp(listed[at].extensionName, VK_EXT_GLOBAL_PRIORITY_EXTENSION_NAME) == 0)
            extension_ = VK_EXT_GLOBAL_PRIORITY_EXTENSION_NAME;
    }
    return extension_ != nullptr;
}

const char *LowPriorityQueues::extension() const
{
    AV_SCOPE("LowPriorityQueues::extension");
    return extension_;
}

bool LowPriorityQueues::apply(VkDeviceCreateInfo &create)
{
    AV_SCOPE("LowPriorityQueues::apply");
    if (extension_ == nullptr || create.queueCreateInfoCount == 0u || create.queueCreateInfoCount > most_queue_infos)
        return false;
    for (uint32_t at = 0u; at < create.queueCreateInfoCount; at++) {
        infos_[at] = create.pQueueCreateInfos[at];
        for (auto *link = static_cast<const VkBaseInStructure *>(infos_[at].pNext); link != nullptr; link = link->pNext)
            if (link->sType == VK_STRUCTURE_TYPE_DEVICE_QUEUE_GLOBAL_PRIORITY_CREATE_INFO) return false;
    }
    for (uint32_t at = 0u; at < create.queueCreateInfoCount; at++) {
        priorities_[at] = {};
        priorities_[at].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_GLOBAL_PRIORITY_CREATE_INFO;
        priorities_[at].pNext = infos_[at].pNext;
        priorities_[at].globalPriority = VK_QUEUE_GLOBAL_PRIORITY_LOW;
        infos_[at].pNext = &priorities_[at];
    }
    create.pQueueCreateInfos = infos_;
    return true;
}

bool LowPriorityQueues::invariants_hold() const
{
    AV_SCOPE("LowPriorityQueues::invariants_hold");
    return extension_ == nullptr || strncmp(extension_, "VK_", 3u) == 0;
}

}
