/* The cl_mems one context made to stand in for GL objects of the guest's
 * (GlMirror), found by the cl_mem: what an acquire and a release copy, what
 * the GL object queries answer, and what goes once the guest's last
 * reference to a cl_mem does. */
#ifndef AV_CL_GL_MIRRORS_HPP
#define AV_CL_GL_MIRRORS_HPP

#include "executor/opencl/gl_mirror.hpp"

#include <stdint.h>

namespace av {

struct ClGlMirrorsProbe;

class ClGlMirrors {
public:
    /* What one context shares of a GL context's objects at once; a guest
     * wanting more is refused at the create, where it can be told. */
    static constexpr uint32_t max_mirrors = 256u;

    ClGlMirrors();
    ClGlMirrors(const ClGlMirrors &) = delete;
    ClGlMirrors &operator=(const ClGlMirrors &) = delete;

    /* False for a mirror of nothing, a mem already held, and when full. */
    [[nodiscard]] bool remember(const GlMirror &mirror);
    /* The mirror `mem` is, or null for a mem not held. */
    GlMirror          *find(const void *mem);
    /* Forgets `mem`, answering what it was. False, `gone` untouched, for a
     * mem not held. */
    [[nodiscard]] bool forget(const void *mem, GlMirror &gone);
    /* Forgets whichever is held first: false once none is. */
    [[nodiscard]] bool forget_any(GlMirror &gone);
    uint32_t           count() const;

    bool invariants_hold() const;

private:
    friend struct ClGlMirrorsProbe;

    /* The slot holding `mem`, or count_ for none. */
    uint32_t slot_of(const void *mem) const;
    void     forget_slot(uint32_t slot, GlMirror &gone);

    /* The first count_ are held. */
    GlMirror mirrors_[max_mirrors];
    uint32_t count_;
};

}  // namespace av

#endif  // AV_CL_GL_MIRRORS_HPP
