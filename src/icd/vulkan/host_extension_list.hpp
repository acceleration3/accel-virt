/* The extensions the host is asked to enable, out of the ones an application
 * enabled (feature 046).
 *
 * The window-system extensions are this ICD's own: a surface is a window on the
 * guest's desktop, and a swapchain is host images copied back and blitted onto
 * it. The host is a Vulkan driver on the host's platform, which has no
 * VK_KHR_win32_surface at all, so passing those names on is how a real
 * vulkaninfo failed with VK_ERROR_EXTENSION_NOT_PRESENT. Everything else is the
 * host's to answer, and is passed on - in the host's own spelling of it, which
 * for the two Win32 external-object extensions is the fd extension the ICD
 * answers them over (feature 047, research.md §10). */
#ifndef AV_ICD_HOST_EXTENSION_LIST_HPP
#define AV_ICD_HOST_EXTENSION_LIST_HPP

#include "core/support/extension_names.hpp"
#include "core/support/status.hpp"

#include <stdint.h>

namespace av {
class BlockSource;
}  // namespace av

namespace av::icd {

class HostExtensionList {
public:
    /* The names are kept in `memory`: as many as the application enables. */
    explicit HostExtensionList(BlockSource &memory);
    HostExtensionList(const HostExtensionList &) = delete;
    HostExtensionList &operator=(const HostExtensionList &) = delete;

    /* The `count` names an application enabled, less the ones this ICD answers
     * itself, with room for one more. invalid_parameter for a count with no
     * names or a null name; no_memory when there is no room for them. */
    [[nodiscard]] Status take(const char *const *names, uint32_t count);
    /* One more name the host is asked for, which no application named: the
     * dma-buf extension a shared allocation is exported through, where the
     * host has it (feature 053). Ignored when the list already holds it; the
     * one take made room for cannot be refused. */
    [[nodiscard]] Status add(const char *name);
    /* Whether the application named `name` itself. */
    bool                 named(const char *name) const;
    const char *const   *names() const;
    uint32_t             count() const;
    /* How many of the application's names this ICD answers itself. */
    uint32_t             kept() const;

    /* It never holds more names than it was given and added. */
    bool invariants_hold() const;

private:
    /* Whether this ICD, rather than the host, implements `name`. */
    bool is_the_icd_s(const char *name) const;
    /* The host's own spelling of `name`, which is `name` itself unless this
     * ICD answers it over a different host extension. */
    const char *host_spelling_of(const char *name) const;

    ExtensionNames     names_;
    /* The application's own list, which outlives the create it came in. */
    const char *const *given_names_;
    uint32_t           given_;
    uint32_t           kept_;
    /* Names this ICD added on top of the list it was given: `add` is how the
     * driver asks the host for something the application never named, so they
     * are not part of `given_` and the invariant has to know they exist
     * (feature 053). */
    uint32_t           added_;
};

}  // namespace av::icd

#endif  // AV_ICD_HOST_EXTENSION_LIST_HPP
