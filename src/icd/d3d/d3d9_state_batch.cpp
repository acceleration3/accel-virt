#include "icd/d3d/d3d9_state_batch.hpp"

#include "core/trace/function_scope.hpp"

namespace av::d3d {

namespace {

/* What the API calls each D3DDDITSS_* that is a sampler state (D3DSAMP_*), 0
 * where it is a stage state of the same number, ~0u where it is neither. */
constexpr uint32_t neither = ~0u;
constexpr uint32_t stage = 0u;
constexpr uint32_t ddi_states[] = {
    neither,                                       /* 0 TEXTUREMAP: a handle, bound through SetTexture */
    stage, stage, stage, stage, stage, stage,      /* 1-6 COLOROP ... ALPHAARG2 */
    stage, stage, stage, stage, stage,             /* 7-11 BUMPENVMAT00 ... TEXCOORDINDEX */
    neither,                                       /* 12 ADDRESS: both axes, answered apart */
    1u, 2u, 4u, 5u, 6u, 7u, 8u, 9u, 10u,           /* 13-21 ADDRESSU ... MAXANISOTROPY */
    stage, stage, stage,                           /* 22-24 BUMPENVLSCALE ... TEXTURETRANSFORMFLAGS */
    3u,                                            /* 25 ADDRESSW */
    stage, stage, stage,                           /* 26-28 COLORARG0, ALPHAARG0, RESULTARG */
    11u, 12u, 13u,                                 /* 29-31 SRGBTEXTURE, ELEMENTINDEX, DMAPOFFSET */
    stage,                                         /* 32 CONSTANT */
    neither, neither};                             /* 33-34 colour keys: no API state */
constexpr uint32_t ddi_state_count = sizeof(ddi_states) / sizeof(ddi_states[0]);
constexpr uint32_t legacy_address = 12u;
constexpr uint32_t sampler_address_u = 1u;
constexpr uint32_t sampler_address_v = 2u;
constexpr uint32_t pixel_samplers = 16u;
constexpr uint32_t first_vertex_sampler = 256u;       /* D3DDMAPSAMPLER */
constexpr uint32_t last_vertex_sampler = 260u;        /* D3DVERTEXTEXTURESAMPLER3 */
/* Render states below this are not states (D3DRENDERSTATETYPE starts at ZENABLE). */
constexpr uint32_t first_render_state = 7u;
/* D3DDDIRS_SCENECAPTURE: the DDI's own, which no device keeps. */
constexpr uint32_t scene_capture = 62u;
constexpr uint32_t pair = 2u;
constexpr uint32_t triple = 3u;

}  // namespace

D3d9StateBatch::D3d9StateBatch()
    : render_slots_{}, sampler_slots_{}, stage_slots_{}, render_{}, render_count_(0u), sampler_{},
      sampler_count_(0u), stage_{}, stage_count_(0u)
{
    AV_SCOPE("D3d9StateBatch::D3d9StateBatch");
}

void D3d9StateBatch::queue(Slot &slot, uint32_t *list, uint32_t &count, uint32_t width, uint32_t first,
                           uint32_t second, uint32_t value)
{
    AV_SCOPE("D3d9StateBatch::queue");
    if (slot.pending_at_ != 0u) {
        list[(slot.pending_at_ - 1u) * width + width - 1u] = value;
        return;
    }
    if (slot.known_ && slot.sent_ == value) return;
    uint32_t *const entry = list + count * width;
    entry[0] = first;
    if (width == triple) entry[1] = second;
    entry[width - 1u] = value;
    count++;
    slot.pending_at_ = count;
}

void D3d9StateBatch::render_state(uint32_t state, uint32_t value)
{
    AV_SCOPE("D3d9StateBatch::render_state");
    if (state < first_render_state || state >= render_states || state == scene_capture) return;
    queue(render_slots_[state], render_, render_count_, pair, state, 0u, value);
}

uint32_t D3d9StateBatch::sampler_slot(uint32_t sampler) const
{
    AV_SCOPE("D3d9StateBatch::sampler_slot");
    if (sampler < pixel_samplers) return sampler;
    if (sampler >= first_vertex_sampler && sampler <= last_vertex_sampler)
        return pixel_samplers + sampler - first_vertex_sampler;
    return samplers;
}

void D3d9StateBatch::sampler_state(uint32_t sampler, uint32_t state, uint32_t value)
{
    AV_SCOPE("D3d9StateBatch::sampler_state");
    const uint32_t slot = sampler_slot(sampler);
    if (slot == samplers) return;
    queue(sampler_slots_[slot * sampler_states + state], sampler_, sampler_count_, triple, sampler, state, value);
}

void D3d9StateBatch::stage_state(uint32_t stage_number, uint32_t state, uint32_t value)
{
    AV_SCOPE("D3d9StateBatch::stage_state");
    if (state == legacy_address) {
        sampler_state(stage_number, sampler_address_u, value);
        sampler_state(stage_number, sampler_address_v, value);
        return;
    }
    if (state >= ddi_state_count || ddi_states[state] == neither) return;
    if (ddi_states[state] != stage) {
        sampler_state(stage_number, ddi_states[state], value);
        return;
    }
    if (stage_number >= stages) return;
    queue(stage_slots_[stage_number * stage_states + state], stage_, stage_count_, triple, stage_number, state,
          value);
}

bool D3d9StateBatch::pending() const
{
    AV_SCOPE("D3d9StateBatch::pending");
    return render_count_ != 0u || sampler_count_ != 0u || stage_count_ != 0u;
}

uint32_t D3d9StateBatch::render_count() const
{
    AV_SCOPE("D3d9StateBatch::render_count");
    return render_count_ * pair;
}

const uint32_t *D3d9StateBatch::render_values() const
{
    AV_SCOPE("D3d9StateBatch::render_values");
    return render_;
}

uint32_t D3d9StateBatch::sampler_count() const
{
    AV_SCOPE("D3d9StateBatch::sampler_count");
    return sampler_count_ * triple;
}

const uint32_t *D3d9StateBatch::sampler_values() const
{
    AV_SCOPE("D3d9StateBatch::sampler_values");
    return sampler_;
}

uint32_t D3d9StateBatch::stage_count() const
{
    AV_SCOPE("D3d9StateBatch::stage_count");
    return stage_count_ * triple;
}

const uint32_t *D3d9StateBatch::stage_values() const
{
    AV_SCOPE("D3d9StateBatch::stage_values");
    return stage_;
}

void D3d9StateBatch::taken()
{
    AV_SCOPE("D3d9StateBatch::taken");
    for (uint32_t entry = 0u; entry < render_count_; entry++) {
        Slot &slot = render_slots_[render_[entry * pair]];
        slot = Slot{render_[entry * pair + 1u], 0u, true};
    }
    for (uint32_t entry = 0u; entry < sampler_count_; entry++) {
        const uint32_t *const triple_at = sampler_ + entry * triple;
        Slot &slot = sampler_slots_[sampler_slot(triple_at[0]) * sampler_states + triple_at[1]];
        slot = Slot{triple_at[2], 0u, true};
    }
    for (uint32_t entry = 0u; entry < stage_count_; entry++) {
        const uint32_t *const triple_at = stage_ + entry * triple;
        Slot &slot = stage_slots_[triple_at[0] * stage_states + triple_at[1]];
        slot = Slot{triple_at[2], 0u, true};
    }
    render_count_ = 0u;
    sampler_count_ = 0u;
    stage_count_ = 0u;
}

void D3d9StateBatch::forget()
{
    AV_SCOPE("D3d9StateBatch::forget");
    for (Slot &slot : render_slots_) slot.known_ = false;
    for (Slot &slot : sampler_slots_) slot.known_ = false;
    for (Slot &slot : stage_slots_) slot.known_ = false;
}

/* Every pending entry is found by its slot, and no slot claims an entry that
 * is not its own. */
bool D3d9StateBatch::invariants_hold() const
{
    AV_SCOPE("D3d9StateBatch::invariants_hold");
    for (uint32_t entry = 0u; entry < render_count_; entry++)
        if (render_slots_[render_[entry * pair]].pending_at_ != entry + 1u) return false;
    for (uint32_t entry = 0u; entry < sampler_count_; entry++) {
        const uint32_t *const triple_at = sampler_ + entry * triple;
        if (sampler_slots_[sampler_slot(triple_at[0]) * sampler_states + triple_at[1]].pending_at_ != entry + 1u)
            return false;
    }
    for (uint32_t entry = 0u; entry < stage_count_; entry++) {
        const uint32_t *const triple_at = stage_ + entry * triple;
        if (stage_slots_[triple_at[0] * stage_states + triple_at[1]].pending_at_ != entry + 1u) return false;
    }
    uint32_t claimed = 0u;
    for (const Slot &slot : render_slots_) claimed += slot.pending_at_ != 0u ? 1u : 0u;
    for (const Slot &slot : sampler_slots_) claimed += slot.pending_at_ != 0u ? 1u : 0u;
    for (const Slot &slot : stage_slots_) claimed += slot.pending_at_ != 0u ? 1u : 0u;
    return claimed == render_count_ + sampler_count_ + stage_count_;
}

}  // namespace av::d3d
