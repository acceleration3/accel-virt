#ifndef AV_VKICD_EXTENSION_POLICY_HPP
#define AV_VKICD_EXTENSION_POLICY_HPP

#include "icd/common/extension_rows.hpp"

#include <stdint.h>

#include <vulkan/vulkan_core.h>

namespace av::icd {

class ExtensionPolicy {
public:
    using Row = ExtensionRow;

    ExtensionPolicy(const Row *rows, uint32_t count);
    ExtensionPolicy(const ExtensionPolicy &) = delete;
    ExtensionPolicy &operator=(const ExtensionPolicy &) = delete;

    VkResult report(const VkExtensionProperties *host, uint32_t host_count, uint32_t *count,
                    VkExtensionProperties *properties) const;
    uint32_t first_refused(const char *const *names, uint32_t count) const;

    bool invariants_hold() const;

private:
    bool reported_by_host(uint32_t index, const VkExtensionProperties *host, uint32_t host_count,
                          VkExtensionProperties &entry) const;

    ExtensionRows rows_;
};

}

#endif
