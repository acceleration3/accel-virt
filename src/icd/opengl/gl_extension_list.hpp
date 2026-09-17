/* The OpenGL extensions one context reports (data-model.md): what
 * glGetStringi(GL_EXTENSIONS, i), GL_NUM_EXTENSIONS and a compatibility
 * glGetString(GL_EXTENSIONS) answer. Its names are a policy's rows, which
 * outlive every context, so they are kept as pointers; the joined string is
 * built as they are added. */
#ifndef AV_GLICD_GL_EXTENSION_LIST_HPP
#define AV_GLICD_GL_EXTENSION_LIST_HPP

#include <stddef.h>
#include <stdint.h>

namespace av::glicd {

struct GlExtensionListProbe;

class GlExtensionList {
public:
    static constexpr uint32_t max_names = 64u;
    static constexpr size_t   joined_bytes = 4096u;

    GlExtensionList();
    GlExtensionList(const GlExtensionList &) = delete;
    GlExtensionList &operator=(const GlExtensionList &) = delete;

    /* False, keeping the list as it was, for a null or empty name, one held
     * already, max_names held, or a name the joined string has no room for. */
    bool        add(const char *name);
    uint32_t    count() const;
    /* Null past count(). */
    const char *name(uint32_t index) const;
    /* The names separated by single spaces; empty for none. */
    const char *joined() const;

    bool invariants_hold() const;

private:
    size_t length_of(const char *name) const;

    const char *names_[max_names];
    uint32_t    count_;
    char        joined_[joined_bytes];
    size_t      length_;

    friend struct GlExtensionListProbe;
};

}  // namespace av::glicd

#endif  // AV_GLICD_GL_EXTENSION_LIST_HPP
