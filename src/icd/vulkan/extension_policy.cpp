#include "icd/vulkan/extension_policy.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/generated/vulkan/vk_supported_extensions.h"

namespace av::icd {

namespace {

const char *const withheld[] = {"VK_KHR_surface", "VK_KHR_win32_surface"};

}  // namespace

ExtensionPolicy::ExtensionPolicy(const Row *rows, uint32_t count) : rows_(rows), count_(count)
{
    AV_SCOPE("ExtensionPolicy::ExtensionPolicy");
}

const ExtensionPolicy::Row *ExtensionPolicy::reportable(const char *name) const
{
    AV_SCOPE("ExtensionPolicy::reportable");
    if (name == nullptr) return nullptr;
    for (const char *const never : withheld)
        if (__builtin_strcmp(never, name) == 0) return nullptr;
    bool implemented = false;
    for (const char *const generated : vkproto::kImplementedDeviceExtensions)
        implemented = implemented || __builtin_strcmp(generated, name) == 0;
    if (!implemented) return nullptr;
    for (uint32_t index = 0u; index < count_; index++)
        if (__builtin_strcmp(rows_[index].name_, name) == 0) return &rows_[index];
    return nullptr;
}

/* The first host entry of the extension the row is answered over - its own
 * name, or the one it names: that name and its terminator against as many of
 * the host's bytes, which the invariant keeps inside the host's field. What
 * the application is told is always the row's own name, and its version is the
 * host's only where they are the same extension. */
bool ExtensionPolicy::reported_by_host(const Row &row, const VkExtensionProperties *host, uint32_t host_count,
                                       VkExtensionProperties &entry) const
{
    AV_SCOPE("ExtensionPolicy::reported_by_host");
    const char *const wanted = row.host_ != nullptr ? row.host_ : row.name_;
    const size_t      compared = __builtin_strlen(wanted) + 1u;
    const size_t      named = __builtin_strlen(row.name_) + 1u;
    for (uint32_t index = 0u; index < host_count; index++) {
        if (__builtin_memcmp(host[index].extensionName, wanted, compared) != 0) continue;
        entry = VkExtensionProperties{};
        __builtin_memcpy(entry.extensionName, row.name_, named);
        const uint32_t host_version = host[index].specVersion < row.spec_version_ ? host[index].specVersion
                                                                                  : row.spec_version_;
        entry.specVersion = row.host_ != nullptr ? row.spec_version_ : host_version;
        return true;
    }
    return false;
}

VkResult ExtensionPolicy::report(const VkExtensionProperties *host, uint32_t host_count, uint32_t *count,
                                 VkExtensionProperties *properties) const
{
    AV_SCOPE("ExtensionPolicy::report");
    uint32_t reported = 0u;
    for (uint32_t index = 0u; index < count_; index++) {
        VkExtensionProperties entry = {};
        if (reportable(rows_[index].name_) == nullptr || !reported_by_host(rows_[index], host, host_count, entry))
            continue;
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
        if (reportable(names[index]) == nullptr) return index;
    return count;
}

bool ExtensionPolicy::invariants_hold() const
{
    AV_SCOPE("ExtensionPolicy::invariants_hold");
    for (uint32_t index = 0u; index < count_; index++) {
        const Row &row = rows_[index];
        if (row.name_ == nullptr || row.name_[0] == '\0' || row.test_ == nullptr || row.test_[0] == '\0') return false;
        uint32_t length = 0u;
        while (length < VK_MAX_EXTENSION_NAME_SIZE && row.name_[length] != '\0') length++;
        if (row.spec_version_ == 0u || length == VK_MAX_EXTENSION_NAME_SIZE) return false;
        if (row.host_ != nullptr && row.host_[0] == '\0') return false;
        for (uint32_t earlier = 0u; earlier < index; earlier++)
            if (__builtin_strcmp(rows_[earlier].name_, row.name_) == 0) return false;
    }
    return true;
}

}  // namespace av::icd
