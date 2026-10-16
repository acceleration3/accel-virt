/* The extension names a host instance or device is made with: the guest's own,
 * and what the host needs beside them.
 *
 * The guest's list is decoded memory the backend does not own and cannot grow,
 * so a name is added to a copy, which holds however many names the guest
 * enabled: room for them and for the names the host adds is made at once,
 * so adding cannot fail. */
#ifndef AV_EXTENSION_LIST_HPP
#define AV_EXTENSION_LIST_HPP

#include "core/support/growable_array.hpp"
#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class ExtensionList {
public:
    explicit ExtensionList(BlockSource &blocks);
    ExtensionList(const ExtensionList &) = delete;
    ExtensionList &operator=(const ExtensionList &) = delete;

    /* Starts again from the guest's `count` names, with room for `adding`
     * more. invalid_parameter for a count with no names or a name that is
     * null - a list the loader refuses, or crashes on, by itself - and
     * no_memory when the room cannot be made; either leaves the list empty. */
    [[nodiscard]] Status take(uint32_t count, const char *const *names, uint32_t adding);
    /* Adds `name` unless a name already there spells it, into the room take
     * made: nothing past it. */
    void                 add(const char *name);

    uint32_t           count() const;
    const char *const *names() const;

    bool invariants_hold() const;

private:
    friend struct ExtensionListProbe;

    using Names = GrowableArray<const char *>;

    Names names_;
};

}  // namespace av

#endif  // AV_EXTENSION_LIST_HPP
