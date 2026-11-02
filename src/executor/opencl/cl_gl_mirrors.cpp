#include "executor/opencl/cl_gl_mirrors.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

namespace {

uint64_t key_of(const void *mem)
{
    AV_SCOPE("key_of");
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(mem));
}

}

ClGlMirrors::ClGlMirrors(BlockSource &blocks) : mirrors_(blocks, Pool::paged)
{
    AV_SCOPE("ClGlMirrors::ClGlMirrors");
}

Status ClGlMirrors::remember(const GlMirror &mirror)
{
    AV_SCOPE("ClGlMirrors::remember");
    if (!mirror.described()) return Status::invalid_parameter;
    uint32_t slot = 0u;
    return mirrors_.insert(key_of(mirror.mem()), mirror, slot);
}

GlMirror *ClGlMirrors::find(const void *mem)
{
    AV_SCOPE("ClGlMirrors::find");
    return mirrors_.find(key_of(mem));
}

bool ClGlMirrors::forget(const void *mem, GlMirror &gone)
{
    AV_SCOPE("ClGlMirrors::forget");
    return mirrors_.take(key_of(mem), gone);
}

bool ClGlMirrors::forget_any(GlMirror &gone)
{
    AV_SCOPE("ClGlMirrors::forget_any");
    const uint32_t slot = mirrors_.next_live(0u);
    if (slot == KeyedSlots<uint64_t, GlMirror>::no_slot) return false;
    gone = *mirrors_.at(slot);
    mirrors_.erase_slot(slot);
    return true;
}

void ClGlMirrors::forget_all()
{
    AV_SCOPE("ClGlMirrors::forget_all");
    mirrors_.clear();
}

uint32_t ClGlMirrors::count() const
{
    AV_SCOPE("ClGlMirrors::count");
    return mirrors_.count();
}

bool ClGlMirrors::invariants_hold() const
{
    AV_SCOPE("ClGlMirrors::invariants_hold");
    bool holds = true;
    mirrors_.for_each([&holds](uint64_t mem, const GlMirror &mirror) {
        holds = holds && mirror.described() && key_of(mirror.mem()) == mem;
    });
    return holds;
}

}
