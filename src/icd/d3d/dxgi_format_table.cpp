#include "icd/d3d/dxgi_format_table.hpp"

#include "core/trace/function_scope.hpp"

namespace av::d3d {

namespace {

/* D3D11_FORMAT_SUPPORT / D3D12_FORMAT_SUPPORT1 (and 2) bits the host answers
 * in, and the DDI bit each becomes: the two are numbered differently. */
struct Carry {
    uint32_t api;
    bool     second;
    uint32_t ddi;
};
constexpr uint32_t ddi_shader_sample = 0x1u;
constexpr uint32_t ddi_render_target = 0x2u;
constexpr uint32_t ddi_blendable = 0x4u;
constexpr uint32_t ddi_multisample_render_target = 0x8u;
constexpr uint32_t ddi_multisample_load = 0x10u;
constexpr uint32_t ddi_vertex_buffer = 0x100u;
constexpr uint32_t ddi_uav_writes = 0x200u;
constexpr uint32_t ddi_buffer = 0x400u;
constexpr uint32_t ddi_logic_op = 0x2000u;
constexpr uint32_t ddi_shader_gather = 0x4000u;
constexpr uint32_t ddi_tiled = 0x10000u;
constexpr uint32_t ddi_uav_reads = 0x20000u;
constexpr uint32_t ddi_display = 0x200000u;
/* D3D11_1DDI_FORMAT_SUPPORT_MULTIPLANE_OVERLAY: DXGI's CheckOverlaySupport asks
 * it of a format before it says an overlay plane takes it. */
constexpr uint32_t ddi_multiplane_overlay = 0x8000u;
/* What the host composes an overlay plane from: the 32-bit RGB layouts avgpu
 * scans out (OverlaySupport), B8G8R8A8, B8G8R8X8 and R8G8B8A8, unsigned. */
constexpr uint32_t overlay_formats[] = {87u, 88u, 28u};
/* And video the host converts as it composes: NV12 and YUY2, wherever the
 * host has them at all. */
constexpr uint32_t overlay_video_formats[] = {103u, 107u};
constexpr Carry    carries[] = {
    {0x200u, false, ddi_shader_sample},
    {0x4000u, false, ddi_render_target},
    {0x8000u, false, ddi_blendable},
    {0x200000u, false, ddi_multisample_render_target},
    {0x400000u, false, ddi_multisample_load},
    {0x2u, false, ddi_vertex_buffer},
    {0x2000000u, false, ddi_uav_writes},
    {0x1u, false, ddi_buffer},
    {0x800000u, false, ddi_shader_gather},
    {0x80000u, false, ddi_display},
    {0x100u, true, ddi_logic_op},
    {0x40u, true, ddi_uav_reads},
};
constexpr uint32_t api_depth_stencil = 0x10000u;
/* D3D12_FORMAT_SUPPORT2_TILED. */
constexpr uint32_t api_tiled = 0x200u;

}  // namespace

DxgiFormatTable::DxgiFormatTable() : support_{}, support2_{}, levels_{}, family_levels_{}, keeps_{}
{
    AV_SCOPE("DxgiFormatTable::DxgiFormatTable");
}

void DxgiFormatTable::record(uint32_t format, uint32_t support, uint32_t support2)
{
    AV_SCOPE("DxgiFormatTable::record");
    if (format >= DxgiFormat::count) return;
    support_[format] = support;
    support2_[format] = support2;
}

void DxgiFormatTable::record_samples(uint32_t format, uint32_t samples, uint32_t levels)
{
    AV_SCOPE("DxgiFormatTable::record_samples");
    const uint32_t step = step_of(samples);
    if (format >= DxgiFormat::count || step == sample_steps) return;
    levels_[format][step] = levels;
}

void DxgiFormatTable::settle()
{
    AV_SCOPE("DxgiFormatTable::settle");
    for (auto &family : family_levels_)
        for (uint32_t &levels : family) levels = 0u;
    for (bool &keeps : keeps_) keeps = false;
    /* A head is its own family's: a typeless head, or a format in no family. */
    for (uint32_t format = 1u; format < DxgiFormat::count; format++)
        if (DxgiFormat(format).family() == format) settle_family(format);
}

/* The runtime holds a family's members that claim multisampled rendering to
 * one level count at each sample count, so the family's counts are those every
 * keeping member renders at - the fewest levels among some set of its members
 * - and a member keeps multisampled rendering only if it renders at each of
 * them; one that does not must still load from a multisampled view. Every
 * set is tried (a family has at most seven members, so at most 127 sets), and
 * the counts kept are the ones offering the most member-and-count pairs: then
 * the most members, then the most counts. Nothing is offered past what the
 * host answered for that format. */
void DxgiFormatTable::settle_family(uint32_t head)
{
    AV_SCOPE("DxgiFormatTable::settle_family");
    constexpr uint32_t most_members = 8u;
    uint32_t members[most_members] = {};
    uint32_t count = 0u, loads = 0u;
    for (uint32_t format = 1u; format < DxgiFormat::count && count < most_members; format++) {
        /* A member the host supports not at all is never viewed, so it has
         * no part to take. */
        if (DxgiFormat(format).family() != head || DxgiFormat(format).family_head() || carried(format) == 0u) continue;
        const bool load = (carried(format) & ddi_multisample_load) != 0u;
        bool       counted = false;
        for (uint32_t step = 1u; step < sample_steps; step++) counted = counted || levels_[format][step] != 0u;
        if ((carried(format) & ddi_multisample_render_target) != 0u && counted) {
            if (load) loads |= 1u << count;
            members[count++] = format;
            continue;
        }
        /* A member that neither renders multisampled nor loads from a
         * multisampled view keeps the whole family from multisampling: the
         * runtime refuses one beside a parent that does. */
        if (!load) return;
    }
    uint32_t best[sample_steps] = {};
    uint32_t best_keeping = 0u, best_score = 0u, best_members = 0u, best_counts = 0u;
    for (uint32_t set = 1u; set < 1u << count; set++) {
        uint32_t counts[sample_steps] = {};
        for (uint32_t step = 1u; step < sample_steps; step++) {
            counts[step] = ~0u;
            for (uint32_t index = 0u; index < count; index++)
                if ((set & (1u << index)) != 0u && levels_[members[index]][step] < counts[step])
                    counts[step] = levels_[members[index]][step];
        }
        uint32_t offered = 0u;
        for (uint32_t step = 1u; step < sample_steps; step++) offered += counts[step] != 0u ? 1u : 0u;
        if (offered == 0u) continue;
        /* Every member that renders at the set's counts keeps them, in the
         * set or not. */
        uint32_t keeping = 0u, kept = 0u;
        for (uint32_t index = 0u; index < count; index++) {
            bool renders = true;
            for (uint32_t step = 1u; step < sample_steps; step++)
                renders = renders && levels_[members[index]][step] >= counts[step];
            if (!renders) continue;
            keeping |= 1u << index;
            kept++;
        }
        /* A member left out keeps taking part only by loading. */
        const uint32_t everyone = (1u << count) - 1u;
        if ((everyone & ~keeping & ~loads) != 0u) continue;
        const uint32_t score = kept * offered;
        const bool     better = score != best_score     ? score > best_score
                                : kept != best_members ? kept > best_members
                                                       : offered > best_counts;
        if (!better) continue;
        best_keeping = keeping;
        best_score = score;
        best_members = kept;
        best_counts = offered;
        for (uint32_t step = 1u; step < sample_steps; step++) best[step] = counts[step];
    }
    for (uint32_t step = 1u; step < sample_steps; step++) family_levels_[head][step] = best[step];
    for (uint32_t index = 0u; index < count; index++) keeps_[members[index]] = (best_keeping & (1u << index)) != 0u;
}

bool DxgiFormatTable::family_multisamples(uint32_t format) const
{
    AV_SCOPE("DxgiFormatTable::family_multisamples");
    const uint32_t *const shared = family_levels_[DxgiFormat(format).family()];
    for (uint32_t step = 1u; step < sample_steps; step++)
        if (shared[step] != 0u) return true;
    return false;
}

uint32_t DxgiFormatTable::carried(uint32_t format) const
{
    AV_SCOPE("DxgiFormatTable::carried");
    uint32_t support = 0u;
    for (const Carry &carry : carries)
        if (((carry.second ? support2_[format] : support_[format]) & carry.api) != 0u) support |= carry.ddi;
    /* The DDI has no depth-stencil bit: a depth format's "render target" is
     * its depth-stencil view - but only a depth format's, never the typeless
     * and view formats of its family, which the runtime refuses to see as
     * renderable (it stops at R32_FLOAT_X8X24_TYPELESS). A depth format is
     * sampled through its view format, never itself, and Vulkan's answer -
     * which can sample depth - is not Direct3D's. */
    if (DxgiFormat(format).depth()) {
        if ((support_[format] & api_depth_stencil) != 0u) support |= ddi_render_target;
        support &= ~(ddi_shader_sample | ddi_multisample_load | ddi_shader_gather | ddi_buffer | ddi_vertex_buffer |
                     ddi_uav_writes | ddi_uav_reads);
    }
    /* Blending and multisampled rendering are rendering. */
    if ((support & ddi_render_target) == 0u) support &= ~(ddi_blendable | ddi_multisample_render_target);
    return support;
}

bool DxgiFormatTable::multisamples(uint32_t format) const
{
    AV_SCOPE("DxgiFormatTable::multisamples");
    if ((carried(format) & ddi_multisample_render_target) == 0u) return false;
    if (!DxgiFormat(format).family_head()) return keeps_[format];
    const uint32_t *const shared = family_levels_[format];
    for (uint32_t step = 1u; step < sample_steps; step++)
        if (shared[step] != 0u) return true;
    return false;
}

uint32_t DxgiFormatTable::tiled_support(uint32_t format) const
{
    AV_SCOPE("DxgiFormatTable::tiled_support");
    if (format >= DxgiFormat::count || (support2_[format] & api_tiled) == 0u) return 0u;
    return ddi_tiled;
}

uint32_t DxgiFormatTable::format_support(uint32_t format) const
{
    AV_SCOPE("DxgiFormatTable::format_support");
    if (format == 0u || format >= DxgiFormat::count) return 0u;
    uint32_t support = carried(format);
    /* Multisampled use at no count past one is a table the runtime refuses
     * the driver over ("MSAA quality reported to be 0"). A member that only
     * loads keeps loading while its family multisamples. */
    if (!multisamples(format)) support &= ~ddi_multisample_render_target;
    if (!multisamples(format) && !family_multisamples(format)) support &= ~ddi_multisample_load;
    /* A plane is a displayed surface, so a format the host cannot display is
     * none, whatever the list says. */
    for (const uint32_t overlay : overlay_formats)
        if (format == overlay && (support & ddi_display) != 0u) support |= ddi_multiplane_overlay;
    for (const uint32_t video : overlay_video_formats)
        if (format == video && support != 0u) support |= ddi_multiplane_overlay;
    return support;
}

/* One sample is every format's one quality level. Past one, the runtime holds
 * each family to one story: the typeless parent answers what its
 * multisampled children do, a child that claims multisampled rendering
 * answers the same, and a child that does not claim it answers none (and
 * loads, settle_family has seen to that). */
uint32_t DxgiFormatTable::quality_levels(uint32_t format, uint32_t samples) const
{
    AV_SCOPE("DxgiFormatTable::quality_levels");
    const uint32_t step = step_of(samples);
    if (format == 0u || format >= DxgiFormat::count || step == sample_steps) return 0u;
    if (step == 0u) return 1u;
    if (!DxgiFormat(format).family_head() && !multisamples(format)) return 0u;
    return family_levels_[DxgiFormat(format).family()][step];
}

bool DxgiFormatTable::multisample_candidate(uint32_t format) const
{
    AV_SCOPE("DxgiFormatTable::multisample_candidate");
    return format < DxgiFormat::count && (carried(format) & ddi_multisample_render_target) != 0u;
}

uint32_t DxgiFormatTable::step_of(uint32_t samples) const
{
    AV_SCOPE("DxgiFormatTable::step_of");
    for (uint32_t step = 0u; step < sample_steps; step++)
        if (samples == 1u << step) return step;
    return sample_steps;
}

}  // namespace av::d3d
