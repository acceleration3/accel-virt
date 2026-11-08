#include "icd/d3d/dxgi_format_table.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/generated/d3d/d3d_wire_types.h"

namespace av::d3d {

namespace {
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
constexpr uint32_t ddi_displayable = 0x100000u;
constexpr uint32_t ddi_video_processor_output = 0x40u;
constexpr uint32_t ddi_video_processor_input = 0x80u;
constexpr uint32_t ddi_multiplane_overlay = 0x8000u;
constexpr uint32_t overlay_formats[] = {87u, 88u, 28u};
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
constexpr uint32_t api_tiled = 0x200u;

}

DxgiFormatTable::DxgiFormatTable() : support_{}, support2_{}, levels_{}, family_levels_{}, keeps_{}
{
    AV_SCOPE("DxgiFormatTable::DxgiFormatTable");
}

void DxgiFormatTable::learn(const d3dproto::FormatCaps *rows, uint32_t count)
{
    AV_SCOPE("DxgiFormatTable::learn");
    static_assert(sizeof(d3dproto::FormatCaps::quality_levels) / sizeof(uint32_t) == sample_steps - 1u,
                  "a row's level counts are the counts past one sample");
    for (uint32_t format = 0u; format < DxgiFormat::count; format++) {
        const d3dproto::FormatCaps row = format < count ? rows[format] : d3dproto::FormatCaps{};
        support_[format] = row.support;
        support2_[format] = row.support2;
        for (uint32_t step = 1u; step < sample_steps; step++) levels_[format][step] = row.quality_levels[step - 1u];
    }
    settle();
}

void DxgiFormatTable::settle()
{
    AV_SCOPE("DxgiFormatTable::settle");
    for (auto &family : family_levels_)
        for (uint32_t &levels : family) levels = 0u;
    for (bool &keeps : keeps_) keeps = false;
    for (uint32_t format = 1u; format < DxgiFormat::count; format++)
        if (DxgiFormat(format).family() == format) settle_family(format);
}

void DxgiFormatTable::settle_family(uint32_t head)
{
    AV_SCOPE("DxgiFormatTable::settle_family");
    constexpr uint32_t most_members = 8u;
    uint32_t members[most_members] = {};
    uint32_t count = 0u, loads = 0u;
    for (uint32_t format = 1u; format < DxgiFormat::count; format++) {
        if (DxgiFormat(format).family() != head || DxgiFormat(format).family_head() || carried(format) == 0u) continue;
        const bool load = (carried(format) & ddi_multisample_load) != 0u;
        bool       counted = false;
        for (uint32_t step = 1u; step < sample_steps; step++) counted = counted || levels_[format][step] != 0u;
        if ((carried(format) & ddi_multisample_render_target) != 0u && counted) {
            if (load) loads |= 1u << count;
            members[count++] = format;
            continue;
        }
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
        uint32_t keeping = 0u, kept = 0u;
        for (uint32_t index = 0u; index < count; index++) {
            bool renders = true;
            for (uint32_t step = 1u; step < sample_steps; step++)
                renders = renders && levels_[members[index]][step] >= counts[step];
            if (!renders) continue;
            keeping |= 1u << index;
            kept++;
        }
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
    if (DxgiFormat(format).depth()) {
        if ((support_[format] & api_depth_stencil) != 0u) support |= ddi_render_target;
        support &= ~(ddi_shader_sample | ddi_multisample_load | ddi_shader_gather | ddi_buffer | ddi_vertex_buffer |
                     ddi_uav_writes | ddi_uav_reads);
    }
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
    if (!multisamples(format)) support &= ~ddi_multisample_render_target;
    if (!multisamples(format) && !family_multisamples(format)) support &= ~ddi_multisample_load;
    for (const uint32_t overlay : overlay_formats)
        if (format == overlay && (support & ddi_display) != 0u) support |= ddi_multiplane_overlay;
    for (const uint32_t video : overlay_video_formats)
        if (format == video && support != 0u)
            support |= ddi_multiplane_overlay | ddi_display | ddi_displayable | ddi_video_processor_input |
                       ddi_video_processor_output;
    return support;
}

uint32_t DxgiFormatTable::quality_levels(uint32_t format, uint32_t samples) const
{
    AV_SCOPE("DxgiFormatTable::quality_levels");
    const uint32_t step = step_of(samples);
    if (format == 0u || format >= DxgiFormat::count || step == sample_steps) return 0u;
    if (step == 0u) return 1u;
    if (!DxgiFormat(format).family_head() && !multisamples(format)) return 0u;
    return family_levels_[DxgiFormat(format).family()][step];
}

uint32_t DxgiFormatTable::step_of(uint32_t samples) const
{
    AV_SCOPE("DxgiFormatTable::step_of");
    for (uint32_t step = 0u; step < sample_steps; step++)
        if (samples == 1u << step) return step;
    return sample_steps;
}

}
