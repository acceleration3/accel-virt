/* The device extensions this ICD tells an application of (research.md §9;
 * constitution VI): one is reported only when the host's device has it, the
 * generator implemented its commands (vk_supported_extensions.h), and a policy
 * row names it together with the test that proves it works through this ICD -
 * which tools/check_extension_policy.py holds every row to. The row's spec
 * version caps the host's, since a newer revision was not what was tested.
 *
 * A row may name the host extension it is answered over, for one this ICD
 * implements on top of a different host mechanism; its version is then the
 * row's own, since the host's version is a different extension's.
 *
 * The surface extensions are never reported whatever says otherwise: the
 * ICD presents itself (046). Host-memory import, where a row reports it,
 * is the ICD's own: it lends the application's pages to the device and
 * imports them as the type asked (docs/host-memory.md, "Application
 * memory"), never the host's own import of a pointer. The host's list is read as bytes the host wrote: a name
 * is compared whole, terminator included, within its fixed field. */
#ifndef AV_VKICD_EXTENSION_POLICY_HPP
#define AV_VKICD_EXTENSION_POLICY_HPP

#include "icd/common/extension_row.hpp"

#include <stdint.h>

#include <vulkan/vulkan_core.h>

namespace av::icd {

class ExtensionPolicy {
public:
    using Row = ExtensionRow;

    /* `rows` is a constant table the owner keeps for as long as this. */
    ExtensionPolicy(const Row *rows, uint32_t count);
    ExtensionPolicy(const ExtensionPolicy &) = delete;
    ExtensionPolicy &operator=(const ExtensionPolicy &) = delete;

    /* The host's `host_count` extensions as the application sees them, with
     * Vulkan's count and array semantics: in the policy's order, each at most
     * once. */
    VkResult report(const VkExtensionProperties *host, uint32_t host_count, uint32_t *count,
                    VkExtensionProperties *properties) const;
    /* The index of the first of a device's `count` extension names that
     * report would not answer for a host that has it - a null name included -
     * or `count` when a device may be created with all of them. */
    uint32_t first_refused(const char *const *names, uint32_t count) const;

    /* Every row names an extension in fewer than 256 bytes, a test and a
     * version, and no name twice. */
    bool invariants_hold() const;

private:
    const Row *reportable(const char *name) const;
    bool       reported_by_host(const Row &row, const VkExtensionProperties *host, uint32_t host_count,
                                VkExtensionProperties &entry) const;

    const Row *rows_;
    uint32_t   count_;
};

}  // namespace av::icd

#endif  // AV_VKICD_EXTENSION_POLICY_HPP
