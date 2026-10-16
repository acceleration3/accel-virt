#include "icd/opengl/gl_extension_policy.hpp"

#include "core/support/text_buffer.hpp"
#include "core/trace/function_scope.hpp"
#include "icd/opengl/gl_extension_list.hpp"

#include "gl_dispatch_client.h"
#include "gl_tier.h"

namespace av::glicd {

namespace {

/* Tear control means a negative interval, which nothing here honours: the
 * pacing is a wait for whole blanks (feature 051, PresentPacing). */
const char *const never_reported[] = {"WGL_EXT_swap_control_tear"};

}  // namespace

GlExtensionPolicy::GlExtensionPolicy(const Row *gl_rows, uint32_t gl_count, const Row *wgl_rows, uint32_t wgl_count)
    : gl_rows_(gl_rows, gl_count < max_rows ? gl_count : max_rows, "GL_",
               icd::ExtensionRows::Names(glproto::kCarriedExtensions,
                                         static_cast<uint32_t>(glproto::kCarriedExtensionCount)),
               icd::ExtensionRows::Names(nullptr, 0u)),
      wgl_rows_(wgl_rows, wgl_count < max_rows ? wgl_count : max_rows, "WGL_",
                icd::ExtensionRows::Names(never_reported, sizeof never_reported / sizeof never_reported[0])),
      wgl_{}
{
    AV_SCOPE("GlExtensionPolicy::GlExtensionPolicy");
    TextBuffer joined(wgl_, sizeof wgl_);
    bool       first = true;
    for (uint32_t row = 0u; row < wgl_rows_.count(); row++) {
        if (!wgl_rows_.reportable(row)) continue;
        if (!first) joined.append(" ");
        joined.append(wgl_rows_.row(row).name_);
        first = false;
    }
}

uint32_t GlExtensionPolicy::mark(const char *name, bool (&found)[max_rows]) const
{
    AV_SCOPE("GlExtensionPolicy::mark");
    /* A row that names the host extension it is answered over is found by
     * that name and by no other: GL_EXT_memory_object_win32 is this driver's
     * spelling of the host's GL_EXT_memory_object_fd, because a guest has no
     * file descriptors - and a host that lists the Win32 name itself would be
     * a host that is not this one (feature 053). */
    uint32_t marked = 0u;
    for (uint32_t row = gl_rows_.answering(name, 0u); row < gl_rows_.count();
         row = gl_rows_.answering(name, row + 1u)) {
        if (found[row]) continue;
        found[row] = true;
        marked++;
    }
    return marked;
}

void GlExtensionPolicy::learn(ClientTransport &wire, GlExtensionList &list) const
{
    AV_SCOPE("GlExtensionPolicy::learn");
    if (gl_rows_.count() == 0u) return;
    int32_t listed = 0;
    glproto::av_glGetIntegerv(wire, gl_num_extensions, &listed);
    const uint32_t host_count = listed <= 0 ? 0u
                              : static_cast<uint32_t>(listed) < max_host_names ? static_cast<uint32_t>(listed)
                                                                                : max_host_names;
    bool     found[max_rows] = {};
    uint32_t unfound = gl_rows_.count();
    for (uint32_t index = 0u; index < host_count && unfound != 0u; index++) {
        char   name[name_bytes];
        size_t length = 0u;
        if (!glproto::av_glGetStringi(wire, gl_extensions, index, name, sizeof name, &length)) break;
        /* A name longer than the buffer was cut, and is no name a row has. */
        if (length >= sizeof name) continue;
        unfound -= mark(name, found);
    }
    for (uint32_t row = 0u; row < gl_rows_.count(); row++)
        if (found[row]) static_cast<void>(list.add(gl_rows_.row(row).name_));
}

const char *GlExtensionPolicy::wgl_extensions() const
{
    AV_SCOPE("GlExtensionPolicy::wgl_extensions");
    return wgl_;
}

/* Both tables hold together, and the WGL string holds exactly the WGL rows
 * that are reportable. */
bool GlExtensionPolicy::invariants_hold() const
{
    AV_SCOPE("GlExtensionPolicy::invariants_hold");
    if (gl_rows_.count() > max_rows || wgl_rows_.count() > max_rows) return false;
    if (!gl_rows_.invariants_hold() || !wgl_rows_.invariants_hold()) return false;
    size_t expected = 0u;
    for (uint32_t row = 0u; row < wgl_rows_.count(); row++)
        if (wgl_rows_.reportable(row))
            expected += __builtin_strlen(wgl_rows_.row(row).name_) + (expected != 0u ? 1u : 0u);
    const size_t kept = expected < wgl_bytes ? expected : wgl_bytes - 1u;
    return __builtin_strlen(wgl_) == kept;
}

}  // namespace av::glicd
