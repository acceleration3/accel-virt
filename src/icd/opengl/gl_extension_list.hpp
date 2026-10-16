/* The OpenGL extensions one context reports (data-model.md): what
 * glGetStringi(GL_EXTENSIONS, i), GL_NUM_EXTENSIONS and a compatibility
 * glGetString(GL_EXTENSIONS) answer. Its names are a policy's rows, which
 * outlive every context, so they are kept as pointers; the joined string is
 * built as they are added. */
#ifndef AV_GLICD_GL_EXTENSION_LIST_HPP
#define AV_GLICD_GL_EXTENSION_LIST_HPP

#include "core/support/extension_names.hpp"
#include "core/support/extension_words.hpp"

#include <stdint.h>

namespace av {
class BlockSource;
}  // namespace av

namespace av::glicd {

struct GlExtensionListProbe;

class GlExtensionList {
public:
    /* Both lists are kept in `memory`. */
    explicit GlExtensionList(BlockSource &memory);
    GlExtensionList(const GlExtensionList &) = delete;
    GlExtensionList &operator=(const GlExtensionList &) = delete;

    /* False, keeping the list as it was, for a null or empty name, one held
     * already, a name with a space in it, or no room for it. */
    bool        add(const char *name);
    uint32_t    count() const;
    /* Null past count(). */
    const char *name(uint32_t index) const;
    /* The names separated by single spaces; empty for none. */
    const char *joined() const;

    /* The joined string is exactly the names, in order. */
    bool invariants_hold() const;

private:
    ExtensionNames names_;
    ExtensionWords joined_;

    friend struct GlExtensionListProbe;
};

}  // namespace av::glicd

#endif  // AV_GLICD_GL_EXTENSION_LIST_HPP
