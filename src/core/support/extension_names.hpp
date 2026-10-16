/* A set of extension name pointers, each name kept once: what a Vulkan
 * instance or device is made with on the host, the names a Vulkan ICD passes
 * on, the OpenGL extensions a context reports.
 *
 * The names are pointers, never copies: each points into a list the caller
 * keeps alive while this holds it - an application's create info, a policy's
 * constant rows, the host's own constants. Two pointers spelling one name are
 * one name. Room is made ahead where a caller must not fail halfway. */
#ifndef AV_EXTENSION_NAMES_HPP
#define AV_EXTENSION_NAMES_HPP

#include "core/support/growable_array.hpp"
#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class BlockSource;

class ExtensionNames {
public:
    ExtensionNames(BlockSource &blocks, Pool pool);
    ExtensionNames(const ExtensionNames &) = delete;
    ExtensionNames &operator=(const ExtensionNames &) = delete;

    /* Starts again from the `count` names at `names`, with room for `adding`
     * more. invalid_parameter for a count with no names or a name that is
     * null, and no_memory when the room cannot be made; either leaves it
     * empty. */
    [[nodiscard]] Status take(const char *const *names, uint32_t count, uint32_t adding);
    /* Room for `count` names in all, so adds up to it cannot fail. */
    [[nodiscard]] Status reserve(uint32_t count);
    /* `name` kept, after the rest, unless a name it holds spells it - which
     * is ok too. invalid_parameter for a null or empty name, no_memory when
     * there is no room; either changes nothing. */
    [[nodiscard]] Status add(const char *name);
    /* Whether a name it holds spells `name`. */
    bool                 holds(const char *name) const;
    uint32_t             count() const;
    /* The names in the order they were added; null for none. */
    const char *const   *names() const;
    /* Null past count(). */
    const char          *at(uint32_t index) const;
    /* Every name gone. */
    void                 clear();

    /* No name is null or empty, and none is held twice. */
    bool invariants_hold() const;

private:
    friend struct ExtensionNamesProbe;

    using Names = GrowableArray<const char *>;

    bool same(const char *one, const char *other) const;

    Names names_;
};

}  // namespace av

#endif  // AV_EXTENSION_NAMES_HPP
