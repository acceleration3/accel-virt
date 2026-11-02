#ifndef AV_GLICD_GL_EXTENSION_POLICY_HPP
#define AV_GLICD_GL_EXTENSION_POLICY_HPP

#include "icd/common/extension_rows.hpp"

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
    using Row = icd::ExtensionRow;

    static constexpr uint32_t max_host_names = 1024u;
    static constexpr uint32_t name_bytes = 256u;
    static constexpr uint32_t max_rows = 64u;
    static constexpr size_t   wgl_bytes = 1024u;
    enum : uint32_t { gl_extensions = 0x1F03u, gl_num_extensions = 0x821Du };

    GlExtensionPolicy(const Row *gl_rows, uint32_t gl_count, const Row *wgl_rows, uint32_t wgl_count);
    GlExtensionPolicy(const GlExtensionPolicy &) = delete;
    GlExtensionPolicy &operator=(const GlExtensionPolicy &) = delete;

    void        learn(ClientTransport &wire, GlExtensionList &list) const;
    const char *wgl_extensions() const;

    bool invariants_hold() const;

private:
    uint32_t mark(const char *name, bool (&found)[max_rows]) const;

    icd::ExtensionRows gl_rows_;
    icd::ExtensionRows wgl_rows_;
    char               wgl_[wgl_bytes];

    friend struct GlExtensionPolicyProbe;
};

}

#endif
