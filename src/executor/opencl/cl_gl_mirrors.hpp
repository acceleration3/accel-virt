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
    explicit ClGlMirrors(BlockSource &blocks);
    ClGlMirrors(const ClGlMirrors &) = delete;
    ClGlMirrors &operator=(const ClGlMirrors &) = delete;
    ClGlMirrors(ClGlMirrors &&) = delete;
    ClGlMirrors &operator=(ClGlMirrors &&) = delete;

    [[nodiscard]] Status remember(const GlMirror &mirror);
    GlMirror            *find(const void *mem);
    [[nodiscard]] bool   forget(const void *mem, GlMirror &gone);
    [[nodiscard]] bool   forget_any(GlMirror &gone);
    void                 forget_all();
    uint32_t             count() const;

    bool invariants_hold() const;

private:
    friend struct ClGlMirrorsProbe;

    KeyedSlots<uint64_t, GlMirror> mirrors_;
};

}

#endif
