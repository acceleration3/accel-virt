#include "icd/vulkan/extension_policy.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/generated/vulkan/vk_supported_extensions.h"

namespace av::icd {

namespace {
const char *const withheld[] = {"VK_KHR_surface", "VK_KHR_win32_surface", "VK_EXT_descriptor_buffer",
                                "VK_EXT_device_generated_commands"};

static_assert(ExtensionRows::name_bytes == VK_MAX_EXTENSION_NAME_SIZE);

}

ExtensionPolicy::ExtensionPolicy(const Row *rows, uint32_t count)
    : rows_(rows, count, "VK_",
            ExtensionRows::Names(vkproto::kImplementedDeviceExtensions,
                                 sizeof vkproto::kImplementedDeviceExtensions /
                                     sizeof vkproto::kImplementedDeviceExtensions[0]),
            ExtensionRows::Names(withheld, sizeof withheld / sizeof withheld[0]))
{
    AV_SCOPE("ExtensionPolicy::ExtensionPolicy");
}

bool ExtensionPolicy::reported_by_host(uint32_t index, const VkExtensionProperties *host, uint32_t host_count,
                                       VkExtensionProperties &entry) const
{
    AV_SCOPE("ExtensionPolicy::reported_by_host");
    const char *const wanted = rows_.host_name(index);
    const char *const name = rows_.row(index).name_;
    const size_t      compared = __builtin_strlen(wanted) + 1u;
    const size_t      named = __builtin_strlen(name) + 1u;
    for (uint32_t at = 0u; at < host_count; at++) {
        if (__builtin_memcmp(host[at].extensionName, wanted, compared) != 0) continue;
        entry = VkExtensionProperties{};
        __builtin_memcpy(entry.extensionName, name, named);
        entry.specVersion = rows_.version(index, host[at].specVersion);
        return true;
    }
    return false;
}

VkResult ExtensionPolicy::report(const VkExtensionProperties *host, uint32_t host_count, uint32_t *count,
                                 VkExtensionProperties *properties) const
{
    AV_SCOPE("ExtensionPolicy::report");
    uint32_t reported = 0u;
    for (uint32_t index = 0u; index < rows_.count(); index++) {
        VkExtensionProperties entry = {};
        if (!rows_.reportable(index) || !reported_by_host(index, host, host_count, entry)) continue;
        if (properties != nullptr && reported == *count) return VK_INCOMPLETE;
        if (properties != nullptr) properties[reported] = entry;
        reported++;
    }
    *count = reported;
    return VK_SUCCESS;
}

uint32_t ExtensionPolicy::first_refused(const char *const *names, uint32_t count) const
{
    AV_SCOPE("ExtensionPolicy::first_refused");
    for (uint32_t index = 0u; index < count; index++)
        if (rows_.named(names[index]) == rows_.count()) return index;
    return count;
}

bool ExtensionPolicy::invariants_hold() const
{
    AV_SCOPE("ExtensionPolicy::invariants_hold");
    return rows_.invariants_hold();
}

}
