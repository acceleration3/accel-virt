#include "icd/vulkan/physical_device_view.hpp"

#include "icd/vulkan/log_sink.hpp"
#include "proto/generated/vulkan/vk_dispatch_client.h"

namespace av::icd {

PhysicalDeviceView::PhysicalDeviceView(ClientTransport &transport, uint64_t luid, uint32_t latest_version,
                                       LogSink &log)
    : transport_(transport), luid_(luid), latest_version_(latest_version), log_(log), nodes_{}, types_{}, nexts_{}, remembered_(0u) {}

void PhysicalDeviceView::properties(VkPhysicalDevice physical_device, VkPhysicalDeviceProperties *answer)
{
    *answer = VkPhysicalDeviceProperties{};
    vkproto::rvk_vkGetPhysicalDeviceProperties(transport_, physical_device, answer);
    if (answer->apiVersion > latest_version_) answer->apiVersion = latest_version_;
}

void PhysicalDeviceView::properties2(VkPhysicalDevice physical_device, VkPhysicalDeviceProperties2 *answer)
{
    if (!remember(answer)) {
        log_.write("rvkvk: vkGetPhysicalDeviceProperties2 chain longer than 32 structures, not sent");
        return;
    }
    answer->properties = VkPhysicalDeviceProperties{};
    vkproto::rvk_vkGetPhysicalDeviceProperties2(transport_, physical_device, answer);
    restore();
    if (answer->properties.apiVersion > latest_version_) answer->properties.apiVersion = latest_version_;
    write_luid();
}

bool PhysicalDeviceView::remember(VkPhysicalDeviceProperties2 *answer)
{
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
    for (uint32_t index = 0u; index < remembered_; index++) {
        nodes_[index]->sType = types_[index];
        nodes_[index]->pNext = nexts_[index];
    }
}

void PhysicalDeviceView::write_luid()
{
    for (uint32_t index = 1u; index < remembered_; index++) {
        if (types_[index] != VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES) continue;
        auto *identity = reinterpret_cast<VkPhysicalDeviceIDProperties *>(nodes_[index]);
        /* A LUID's bytes are its 64-bit value's, little-endian (adapter_luids.hpp). */
        __builtin_memcpy(identity->deviceLUID, &luid_, sizeof identity->deviceLUID);
        identity->deviceLUIDValid = VK_TRUE;
    }
}

bool PhysicalDeviceView::invariants_hold() const { return remembered_ <= max_chain; }

}  // namespace av::icd
