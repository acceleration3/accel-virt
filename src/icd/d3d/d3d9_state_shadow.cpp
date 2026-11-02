#include "icd/d3d/d3d9_state_shadow.hpp"

#include "core/trace/function_scope.hpp"

namespace av::d3d {

namespace {

constexpr uint32_t pixel_samplers = 16u;
constexpr uint32_t first_vertex_sampler = 256u;
constexpr uint32_t last_vertex_sampler = 260u;
constexpr uint32_t stage_pixel = 1u;

}

D3d9StateShadow::D3d9StateShadow()
    : render_{}, stage_{}, texture_{}, vertex_shader_{}, pixel_shader_{}, declaration_{}, constant_f_{},
      constant_i_{}, constant_b_{}, stream_{}, indices_{}, viewport_{}, z_range_{}, scissor_{}, clip_plane_{}
{
    AV_SCOPE("D3d9StateShadow::D3d9StateShadow");
}

uint32_t D3d9StateShadow::stage_index(uint32_t stage) const
{
    AV_SCOPE("D3d9StateShadow::stage_index");
    if (stage < pixel_samplers) return stage;
    if (stage >= first_vertex_sampler && stage <= last_vertex_sampler) return pixel_samplers + stage - first_vertex_sampler;
    return stages;
}

const D3d9StateShadow::Slot *D3d9StateShadow::slot(const D3d9StateEntry &entry) const
{
    AV_SCOPE("D3d9StateShadow::slot");
    const uint32_t a = entry.a();
    const uint32_t b = entry.b();
    const bool     shader_stage = a <= stage_pixel;
    switch (entry.kind()) {
    case D3d9StateEntry::Kind::render: return a < render_states ? &render_[a] : nullptr;
    case D3d9StateEntry::Kind::stage:
        return stage_index(a) < stages && b < stage_states ? &stage_[stage_index(a) * stage_states + b] : nullptr;
    case D3d9StateEntry::Kind::texture: return stage_index(a) < stages ? &texture_[stage_index(a)] : nullptr;
    case D3d9StateEntry::Kind::vertex_shader: return &vertex_shader_;
    case D3d9StateEntry::Kind::pixel_shader: return &pixel_shader_;
    case D3d9StateEntry::Kind::declaration: return &declaration_;
    case D3d9StateEntry::Kind::constant_f:
        return shader_stage && b < float_registers ? &constant_f_[a * float_registers + b] : nullptr;
    case D3d9StateEntry::Kind::constant_i:
        return shader_stage && b < integer_registers ? &constant_i_[a * integer_registers + b] : nullptr;
    case D3d9StateEntry::Kind::constant_b:
        return shader_stage && b < boolean_registers ? &constant_b_[a * boolean_registers + b] : nullptr;
    case D3d9StateEntry::Kind::stream: return a < streams ? &stream_[a] : nullptr;
    case D3d9StateEntry::Kind::indices: return &indices_;
    case D3d9StateEntry::Kind::viewport: return &viewport_;
    case D3d9StateEntry::Kind::z_range: return &z_range_;
    case D3d9StateEntry::Kind::scissor: return &scissor_;
    case D3d9StateEntry::Kind::clip_plane: return a < clip_planes ? &clip_plane_[a] : nullptr;
    case D3d9StateEntry::Kind::none: break;
    }
    return nullptr;
}

D3d9StateShadow::Slot *D3d9StateShadow::slot(const D3d9StateEntry &entry)
{
    AV_SCOPE("D3d9StateShadow::slot");
    return const_cast<Slot *>(static_cast<const D3d9StateShadow *>(this)->slot(entry));
}

bool D3d9StateShadow::set(const D3d9StateEntry &entry)
{
    AV_SCOPE("D3d9StateShadow::set");
    Slot *const kept = slot(entry);
    if (kept == nullptr) return false;
    for (uint32_t index = 0u; index < D3d9StateEntry::value_words; index++) kept->value_[index] = entry.value()[index];
    kept->known_ = true;
    return true;
}

bool D3d9StateShadow::current(const D3d9StateEntry &entry, D3d9StateEntry &value) const
{
    AV_SCOPE("D3d9StateShadow::current");
    const Slot *const kept = slot(entry);
    if (kept == nullptr || !kept->known_) return false;
    value = D3d9StateEntry(entry.kind(), entry.a(), entry.b(), kept->value_, D3d9StateEntry::value_words);
    return true;
}

}
