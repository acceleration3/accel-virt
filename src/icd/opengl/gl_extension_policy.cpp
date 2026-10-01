#include "icd/opengl/gl_extension_policy.hpp"

#include "core/support/text_buffer.hpp"
#include "core/trace/function_scope.hpp"
#include "icd/opengl/gl_extension_list.hpp"

#include "gl_dispatch_client.h"
#include "gl_tier.h"

namespace av::glicd {

namespace {

/* Tear control means a negative interval, which nothing here honours: the
 * pacing is a wait for whole blanks (feature 051, SwapInterval). */
const char *const never_reported[] = {"WGL_EXT_swap_control_tear"};

}  // namespace

GlExtensionPolicy::GlExtensionPolicy(const Row *gl_rows, uint32_t gl_count, const Row *wgl_rows, uint32_t wgl_count)
    : gl_rows_(gl_rows), gl_count_(gl_rows != nullptr ? (gl_count < max_rows ? gl_count : max_rows) : 0u),
      wgl_rows_(wgl_rows), wgl_count_(wgl_rows != nullptr ? (wgl_count < max_rows ? wgl_count : max_rows) : 0u),
      wgl_{}
{
    AV_SCOPE("GlExtensionPolicy::GlExtensionPolicy");
    TextBuffer joined(wgl_, sizeof wgl_);
    bool       first = true;
    for (uint32_t row = 0u; row < wgl_count_; row++) {
        if (wgl_rows_[row].name_ == nullptr || withheld(wgl_rows_[row].name_)) continue;
        if (!first) joined.append(" ");
        joined.append(wgl_rows_[row].name_);
        first = false;
    }
}

void GlExtensionPolicy::learn(ClientTransport &wire, GlExtensionList &list) const
{
    AV_SCOPE("GlExtensionPolicy::learn");
    if (gl_count_ == 0u) return;
    int32_t listed = 0;
    glproto::av_glGetIntegerv(wire, gl_num_extensions, &listed);
    const uint32_t host_count = listed <= 0 ? 0u
                              : static_cast<uint32_t>(listed) < max_host_names ? static_cast<uint32_t>(listed)
                                                                                : max_host_names;
    bool     found[max_rows] = {};
    uint32_t unfound = gl_count_;
    for (uint32_t index = 0u; index < host_count && unfound != 0u; index++) {
        char   name[name_bytes];
        size_t length = 0u;
        if (!glproto::av_glGetStringi(wire, gl_extensions, index, name, sizeof name, &length)) break;
        /* A name longer than the buffer was cut, and is no name a row has. */
        if (length >= sizeof name) continue;
        const uint32_t row = reportable(name);
        if (row == max_rows || found[row]) continue;
        found[row] = true;
        unfound--;
    }
    for (uint32_t row = 0u; row < gl_count_; row++)
        if (found[row]) static_cast<void>(list.add(gl_rows_[row].name_));
}

const char *GlExtensionPolicy::wgl_extensions() const
{
    AV_SCOPE("GlExtensionPolicy::wgl_extensions");
    return wgl_;
}

/* Every row names an extension of its kind, and the WGL string holds exactly
 * the WGL rows that are not withheld. */
bool GlExtensionPolicy::invariants_hold() const
{
    AV_SCOPE("GlExtensionPolicy::invariants_hold");
    if (gl_count_ > max_rows || wgl_count_ > max_rows) return false;
    for (uint32_t row = 0u; row < gl_count_; row++)
        if (!starts_with(gl_rows_[row].name_, "GL_")) return false;
    size_t expected = 0u;
    for (uint32_t row = 0u; row < wgl_count_; row++) {
        const char *const name = wgl_rows_[row].name_;
        if (!starts_with(name, "WGL_")) return false;
        if (!withheld(name)) expected += __builtin_strlen(name) + (expected != 0u ? 1u : 0u);
    }
    const size_t kept = expected < wgl_bytes ? expected : wgl_bytes - 1u;
    return __builtin_strlen(wgl_) == kept;
}

/* The DLL has no C runtime, and a counted comparison is a strncmp call. */
bool GlExtensionPolicy::starts_with(const char *name, const char *prefix) const
{
    AV_SCOPE("GlExtensionPolicy::starts_with");
    if (name == nullptr) return false;
    for (; *prefix != '\0'; prefix++, name++)
        if (*name != *prefix) return false;
    return true;
}

bool GlExtensionPolicy::carried(const char *name) const
{
    AV_SCOPE("GlExtensionPolicy::carried");
    for (const char *const carried_name : glproto::kCarriedExtensions)
        if (__builtin_strcmp(carried_name, name) == 0) return true;
    return false;
}

bool GlExtensionPolicy::withheld(const char *name) const
{
    AV_SCOPE("GlExtensionPolicy::withheld");
    for (const char *const never : never_reported)
        if (__builtin_strcmp(never, name) == 0) return true;
    return false;
}

uint32_t GlExtensionPolicy::reportable(const char *name) const
{
    AV_SCOPE("GlExtensionPolicy::reportable");
    for (uint32_t row = 0u; row < gl_count_; row++) {
        /* A row that names the host extension it is answered over is found by
         * that name and by no other: GL_EXT_memory_object_win32 is this
         * driver's spelling of the host's GL_EXT_memory_object_fd, because a
         * guest has no file descriptors - and a host that lists the Win32 name
         * itself would be a host that is not this one (feature 053). */
        const char *const looked_for =
            gl_rows_[row].host_ != nullptr ? gl_rows_[row].host_ : gl_rows_[row].name_;
        if (__builtin_strcmp(looked_for, name) != 0) continue;
        return carried(gl_rows_[row].name_) ? row : max_rows;
    }
    return max_rows;
}

}  // namespace av::glicd
