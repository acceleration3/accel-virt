#include "icd/vulkan/physical_device_view.hpp"

#include "core/trace/function_scope.hpp"
#include "core/log/driver_log.hpp"
#include "proto/generated/vulkan/vk_dispatch_client.h"

namespace av::icd {

PhysicalDeviceView::PhysicalDeviceView(ClientTransport &transport, vkproto::HandleTranslation &handles, uint64_t luid, const char *name,
                                       uint32_t latest_version, DriverLog &log)
    : transport_(transport), handles_(handles), luid_(luid), name_(name), latest_version_(latest_version), log_(log), nodes_{}, types_{}, nexts_{}, remembered_(0u)
    {
        AV_SCOPE("PhysicalDeviceView::PhysicalDeviceView");
    }

void PhysicalDeviceView::properties(VkPhysicalDevice physical_device, VkPhysicalDeviceProperties *answer)
{
    AV_SCOPE("PhysicalDeviceView::properties");
    *answer = VkPhysicalDeviceProperties{};
    vkproto::av_vkGetPhysicalDeviceProperties(transport_, handles_, physical_device, answer);
    if (answer->apiVersion > latest_version_) answer->apiVersion = latest_version_;
    write_name(*answer);
}

void PhysicalDeviceView::properties2(VkPhysicalDevice physical_device, VkPhysicalDeviceProperties2 *answer)
{
    AV_SCOPE("PhysicalDeviceView::properties2");
    if (!remember(answer)) {
        log_.write("vkGetPhysicalDeviceProperties2 chain longer than 32 structures, not sent");
        return;
    }
    answer->properties = VkPhysicalDeviceProperties{};
    vkproto::av_vkGetPhysicalDeviceProperties2(transport_, handles_, physical_device, answer);
    restore();
    if (answer->properties.apiVersion > latest_version_) answer->properties.apiVersion = latest_version_;
    write_name(answer->properties);
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

/* The adapter's name is at most AdapterString::bytes, well inside the 256 a
 * device name has room for; written whole with its terminator. */
void PhysicalDeviceView::write_name(VkPhysicalDeviceProperties &properties) const
{
    AV_SCOPE("PhysicalDeviceView::write_name");
    if (name_ == nullptr || name_[0] == '\0') return;
    size_t length = 0u;
    while (name_[length] != '\0' && length + 1u < sizeof properties.deviceName) {
        properties.deviceName[length] = name_[length];
        ++length;
    }
    properties.deviceName[length] = '\0';
}

bool PhysicalDeviceView::invariants_hold() const
{
    AV_SCOPE("PhysicalDeviceView::invariants_hold");
    return remembered_ <= max_chain;
}

}  // namespace av::icd
