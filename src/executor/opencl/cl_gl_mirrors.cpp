#include "executor/opencl/cl_gl_mirrors.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

ClGlMirrors::ClGlMirrors() : mirrors_(), count_(0u)
{
    AV_SCOPE("ClGlMirrors::ClGlMirrors");
}

bool ClGlMirrors::remember(const GlMirror &mirror)
{
    AV_SCOPE("ClGlMirrors::remember");
    if (!mirror.described() || count_ == max_mirrors || slot_of(mirror.mem()) != count_) return false;
    mirrors_[count_++] = mirror;
    return true;
}

GlMirror *ClGlMirrors::find(const void *mem)
{
    AV_SCOPE("ClGlMirrors::find");
    const uint32_t slot = slot_of(mem);
    return slot != count_ ? &mirrors_[slot] : nullptr;
}

bool ClGlMirrors::forget(const void *mem, GlMirror &gone)
{
    AV_SCOPE("ClGlMirrors::forget");
    const uint32_t slot = slot_of(mem);
    if (slot == count_) return false;
    forget_slot(slot, gone);
    return true;
}

bool ClGlMirrors::forget_any(GlMirror &gone)
{
    AV_SCOPE("ClGlMirrors::forget_any");
    if (count_ == 0u) return false;
    forget_slot(0u, gone);
    return true;
}

uint32_t ClGlMirrors::count() const
{
    AV_SCOPE("ClGlMirrors::count");
    return count_;
}

uint32_t ClGlMirrors::slot_of(const void *mem) const
{
    AV_SCOPE("ClGlMirrors::slot_of");
    for (uint32_t slot = 0u; slot < count_; slot++)
        if (mirrors_[slot].mem() == mem) return slot;
    return count_;
}

/* The last held moves into the hole, so the held stay the first count_. */
void ClGlMirrors::forget_slot(uint32_t slot, GlMirror &gone)
{
    AV_SCOPE("ClGlMirrors::forget_slot");
    gone = mirrors_[slot];
    count_--;
    mirrors_[slot] = mirrors_[count_];
    mirrors_[count_] = GlMirror();
}

/* Every held slot is a mirror of something, no mem twice; every slot past
 * them is empty. */
bool ClGlMirrors::invariants_hold() const
{
    AV_SCOPE("ClGlMirrors::invariants_hold");
    if (count_ > max_mirrors) return false;
    for (uint32_t slot = 0u; slot < count_; slot++) {
        if (!mirrors_[slot].described()) return false;
        for (uint32_t other = slot + 1u; other < count_; other++)
            if (mirrors_[other].mem() == mirrors_[slot].mem()) return false;
    }
    for (uint32_t slot = count_; slot < max_mirrors; slot++)
        if (mirrors_[slot].mem() != nullptr) return false;
    return true;
}

}  // namespace av
