#include "executor/opengl/gl_texture_targets.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

GlTextureTargets::GlTextureTargets(BlockSource &blocks) : targets_(blocks, Pool::paged)
{
    AV_SCOPE("GlTextureTargets::GlTextureTargets");
}

Status GlTextureTargets::note(uint32_t texture, uint32_t target)
{
    AV_SCOPE("GlTextureTargets::note");
    if (texture == 0u || target == 0u) return Status::invalid_parameter;
    if (targets_.find(texture) != nullptr) return Status::ok;
    uint32_t slot = 0u;
    return targets_.insert(texture, target, slot);
}

uint32_t GlTextureTargets::target_of(uint32_t texture) const
{
    AV_SCOPE("GlTextureTargets::target_of");
    const uint32_t *const target = targets_.find(texture);
    return target != nullptr ? *target : 0u;
}

void GlTextureTargets::forget(uint32_t texture)
{
    AV_SCOPE("GlTextureTargets::forget");
    static_cast<void>(targets_.erase(texture));
}

uint32_t GlTextureTargets::count() const
{
    AV_SCOPE("GlTextureTargets::count");
    return targets_.count();
}

bool GlTextureTargets::invariants_hold() const
{
    AV_SCOPE("GlTextureTargets::invariants_hold");
    return targets_.invariants_hold();
}

}
