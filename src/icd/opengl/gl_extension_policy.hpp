/* Which extensions the OpenGL ICD reports (research.md §7; constitution VI):
 * an OpenGL extension only when a policy row names it with its test, the
 * generator carries its commands (gl_tier.h) and the context's host lists it;
 * a WGL extension - the ICD's own functions - only through a row naming its
 * test. WGL_EXT_swap_control is never reported: presentation has no interval
 * to honour (051). The host's list is read once per context, and only while a
 * row is still unfound: each name is a round trip. */
#ifndef AV_GLICD_GL_EXTENSION_POLICY_HPP
#define AV_GLICD_GL_EXTENSION_POLICY_HPP

#include "icd/common/extension_policy.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {
class ClientTransport;
}

namespace av::glicd {

class GlExtensionList;
struct GlExtensionPolicyProbe;

class GlExtensionPolicy {
public:
    using Row = icd::ExtensionPolicy::Row;

    static constexpr uint32_t max_host_names = 1024u;
    static constexpr uint32_t name_bytes = 256u;
    static constexpr uint32_t max_rows = 64u;
    static constexpr size_t   wgl_bytes = 1024u;
    enum : uint32_t { gl_extensions = 0x1F03u, gl_num_extensions = 0x821Du };

    /* Both tables are constants the owner keeps for as long as this; rows
     * past max_rows are not read. */
    GlExtensionPolicy(const Row *gl_rows, uint32_t gl_count, const Row *wgl_rows, uint32_t wgl_count);
    GlExtensionPolicy(const GlExtensionPolicy &) = delete;
    GlExtensionPolicy &operator=(const GlExtensionPolicy &) = delete;

    /* The OpenGL extensions of the context `wire` reaches into `list`, in the
     * policy's order. A host whose answer fails stops the reading: what was
     * found by then is kept. */
    void        learn(ClientTransport &wire, GlExtensionList &list) const;
    /* The WGL extensions, space-separated. */
    const char *wgl_extensions() const;

    bool invariants_hold() const;

private:
    bool starts_with(const char *name, const char *prefix) const;
    bool carried(const char *name) const;
    bool withheld(const char *name) const;
    /* The row of `name` that may be reported, or max_rows. */
    uint32_t reportable(const char *name) const;

    const Row *gl_rows_;
    uint32_t   gl_count_;
    const Row *wgl_rows_;
    uint32_t   wgl_count_;
    char       wgl_[wgl_bytes];

    friend struct GlExtensionPolicyProbe;
};

}  // namespace av::glicd

#endif  // AV_GLICD_GL_EXTENSION_POLICY_HPP
