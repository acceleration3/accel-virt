#include "icd/vulkan/physical_device_view.hpp"

#include "core/trace/function_scope.hpp"
#include "icd/common/log_sink.hpp"
#include "proto/generated/vulkan/vk_dispatch_client.h"

namespace av::icd {

PhysicalDeviceView::PhysicalDeviceView(ClientTransport &transport, uint64_t luid, uint32_t latest_version,
                                       LogSink &log)
    : transport_(transport), luid_(luid), latest_version_(latest_version), log_(log), nodes_{}, types_{}, nexts_{}, remembered_(0u)
    {
        AV_SCOPE("PhysicalDeviceView::PhysicalDeviceView");
    }

void PhysicalDeviceView::properties(VkPhysicalDevice physical_device, VkPhysicalDeviceProperties *answer)
{
    AV_SCOPE("PhysicalDeviceView::properties");
    *answer = VkPhysicalDeviceProperties{};
    vkproto::av_vkGetPhysicalDeviceProperties(transport_, physical_device, answer);
    if (answer->apiVersion > latest_version_) answer->apiVersion = latest_version_;
}

void PhysicalDeviceView::properties2(VkPhysicalDevice physical_device, VkPhysicalDeviceProperties2 *answer)
{
    AV_SCOPE("PhysicalDeviceView::properties2");
    if (!remember(answer)) {
        log_.write("avvk: vkGetPhysicalDeviceProperties2 chain longer than 32 structures, not sent");
        return;
    }
    answer->properties = VkPhysicalDeviceProperties{};
    vkproto::av_vkGetPhysicalDeviceProperties2(transport_, physical_device, answer);
    restore();
    if (answer->properties.apiVersion > latest_version_) answer->properties.apiVersion = latest_version_;
    write_luid();
}

bool PhysicalDeviceView::remember(VkPhysicalDeviceProperties2 *answer)
{
    AV_SCOPE("PhysicalDeviceView::remember");
    remembered_ = 0u;
    auto *node = reinterpret_cast<VkBaseOutStructure *>(answer);
    for (; node != nullptr && remembered_ < max_chain; node = node->pNext) {
        nodes_[remembered_] = node;
        types_[remembered_] = node->sType;
        nexts_[remembered_] = node->pNext;
        remembered_++;
    }
    return node == nullptr;
}

void PhysicalDeviceView::restore()
{
    AV_SCOPE("PhysicalDeviceView::restore");
    for (uint32_t index = 0u; index < remembered_; index++) {
        nodes_[index]->sType = types_[index];
        nodes_[index]->pNext = nexts_[index];
    }
}

/* A LUID's bytes are its 64-bit value's, little-endian: LowPart, then
 * HighPart (Kmt::adapter_luid). The ID node and the core 1.1 node both carry
 * it; a client of 1.2 or later reads the latter. */
void PhysicalDeviceView::write_luid()
{
    AV_SCOPE("PhysicalDeviceView::write_luid");
    for (uint32_t index = 1u; index < remembered_; index++) {
        if (types_[index] == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES) {
            auto *identity = reinterpret_cast<VkPhysicalDeviceIDProperties *>(nodes_[index]);
            __builtin_memcpy(identity->deviceLUID, &luid_, sizeof identity->deviceLUID);
            identity->deviceNodeMask = only_node;
            identity->deviceLUIDValid = VK_TRUE;
        } else if (types_[index] == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_PROPERTIES) {
            auto *core11 = reinterpret_cast<VkPhysicalDeviceVulkan11Properties *>(nodes_[index]);
            __builtin_memcpy(core11->deviceLUID, &luid_, sizeof core11->deviceLUID);
            core11->deviceNodeMask = only_node;
            core11->deviceLUIDValid = VK_TRUE;
        }
    }
}

bool PhysicalDeviceView::invariants_hold() const
{
    AV_SCOPE("PhysicalDeviceView::invariants_hold");
    return remembered_ <= max_chain;
}

}  // namespace av::icd
