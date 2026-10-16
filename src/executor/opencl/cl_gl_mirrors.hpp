/* The cl_mems one context made to stand in for GL objects of the guest's
 * (GlMirror), found by the cl_mem: what an acquire and a release copy, what
 * the GL object queries answer, and what goes once the guest's last
 * reference to a cl_mem does. */
#ifndef AV_CL_GL_MIRRORS_HPP
#define AV_CL_GL_MIRRORS_HPP

#include "core/support/keyed_slots.hpp"
#include "core/support/status.hpp"
#include "executor/opencl/gl_mirror.hpp"

#include <stdint.h>

namespace av {

struct ClGlMirrorsProbe;

class ClGlMirrors {
public:
    /* As many as the guest holds cl_mems for: DaVinci Resolve keeps every
     * texture it hands OpenCL for its whole session. */
    explicit ClGlMirrors(BlockSource &blocks);
    ClGlMirrors(const ClGlMirrors &) = delete;
    ClGlMirrors &operator=(const ClGlMirrors &) = delete;
    ClGlMirrors(ClGlMirrors &&) = delete;
    ClGlMirrors &operator=(ClGlMirrors &&) = delete;

    /* invalid_parameter for a mirror of nothing, conflict for a mem already
     * held, no_memory when the table cannot grow. */
    [[nodiscard]] Status remember(const GlMirror &mirror);
    /* The mirror `mem` is, or null for a mem not held. */
    GlMirror            *find(const void *mem);
    /* Forgets `mem`, answering what it was. False, `gone` untouched, for a
     * mem not held. */
    [[nodiscard]] bool   forget(const void *mem, GlMirror &gone);
    /* Forgets whichever is held first: false once none is. */
    [[nodiscard]] bool   forget_any(GlMirror &gone);
    /* Forgets everything without answering it, and gives the table's memory
     * back. */
    void                 forget_all();
    uint32_t             count() const;

    /* Every mirror held is of something, under its own mem. */
    bool invariants_hold() const;

private:
    friend struct ClGlMirrorsProbe;

    KeyedSlots<uint64_t, GlMirror> mirrors_;
};

}  // namespace av

#endif  // AV_CL_GL_MIRRORS_HPP
