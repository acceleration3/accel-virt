/* The extension names a host instance or device is made with: the guest's own,
 * and what the host needs beside them.
 *
 * The guest's list is decoded memory the backend does not own and cannot grow,
 * so a name is added to a copy. Fixed room rather than an allocation, because
 * a create runs on a context's worker between two commands. */
#ifndef AV_EXTENSION_LIST_HPP
#define AV_EXTENSION_LIST_HPP

#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class ExtensionList {
public:
    /* Every extension the Vulkan registry defines for an instance, or for a
     * device, fits with room to spare. */
    static constexpr uint32_t capacity = 256u;

    ExtensionList();
    ExtensionList(const ExtensionList &) = delete;
    ExtensionList &operator=(const ExtensionList &) = delete;

    /* Starts again from the guest's `count` names. invalid_parameter for a
     * count with no names or a name that is null - a list the loader refuses,
     * or crashes on, by itself - and no_space for more than the room; either
     * leaves the list empty. */
    [[nodiscard]] Status take(uint32_t count, const char *const *names);
    /* Adds `name` unless a name already there spells it or there is no room:
     * a guest that filled the list is given what it asked for, and the driver
     * answers it. */
    void                 add(const char *name);

    uint32_t           count() const;
    const char *const *names() const;

    bool invariants_hold() const;

private:
    friend struct ExtensionListProbe;

    const char *names_[capacity];
    uint32_t    count_;
};

}  // namespace av

#endif  // AV_EXTENSION_LIST_HPP
