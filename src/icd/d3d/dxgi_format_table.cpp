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
constexpr uint32_t ddi_uav_reads = 0x20000u;
constexpr uint32_t ddi_display = 0x200000u;
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

struct Family {
    uint32_t head;
    uint32_t last;
};
constexpr Family families[] = {{1u, 4u},   {5u, 8u},   {9u, 14u},  {15u, 18u}, {19u, 22u}, {23u, 25u},
                               {27u, 32u}, {33u, 38u}, {39u, 43u}, {44u, 47u}, {48u, 52u}, {53u, 59u},
                               {60u, 64u}, {70u, 72u}, {73u, 75u}, {76u, 78u}, {79u, 81u}, {82u, 84u},
                               {94u, 96u}, {97u, 99u}};
/* B8G8R8A8: 87 and 91 under 90; B8G8R8X8: 88 and 93 under 92. */
constexpr uint32_t bgra_head = 90u, bgrx_head = 92u;
constexpr uint32_t bgra_unorm = 87u, bgra_srgb = 91u, bgrx_unorm = 88u, bgrx_srgb = 93u;
/* D32_FLOAT_S8X24_UINT, D32_FLOAT, D24_UNORM_S8_UINT, D16_UNORM. */
constexpr uint32_t depth_formats[] = {20u, 40u, 45u, 55u};

}  // namespace

DxgiFormatTable::DxgiFormatTable() : support_{}, support2_{}, levels_{}, family_levels_{}
{
    AV_SCOPE("DxgiFormatTable::DxgiFormatTable");
}

void DxgiFormatTable::record(uint32_t format, uint32_t support, uint32_t support2)
{
    AV_SCOPE("DxgiFormatTable::record");
    if (format >= format_count) return;
    support_[format] = support;
    support2_[format] = support2;
}

void DxgiFormatTable::record_samples(uint32_t format, uint32_t samples, uint32_t levels)
{
    AV_SCOPE("DxgiFormatTable::record_samples");
    const uint32_t step = step_of(samples);
    if (format >= format_count || step == sample_steps) return;
    levels_[format][step] = levels;
}

void DxgiFormatTable::settle()
{
    AV_SCOPE("DxgiFormatTable::settle");
    constexpr uint32_t unset = ~0u;
    for (auto &family : family_levels_)
        for (uint32_t &levels : family) levels = unset;
    for (uint32_t format = 1u; format < format_count; format++) {
        const uint32_t claimed = carried(format);
        if ((claimed & ddi_multisample_render_target) == 0u) continue;
        uint32_t *const shared = family_levels_[family_of(format)];
        for (uint32_t step = 1u; step < sample_steps; step++)
            if (levels_[format][step] < shared[step]) shared[step] = levels_[format][step];
    }
    for (auto &family : family_levels_)
        for (uint32_t &levels : family)
            if (levels == unset) levels = 0u;
}

uint32_t DxgiFormatTable::family_of(uint32_t format) const
{
    AV_SCOPE("DxgiFormatTable::family_of");
    if (format == bgra_unorm || format == bgra_srgb) return bgra_head;
    if (format == bgrx_unorm || format == bgrx_srgb) return bgrx_head;
    for (const Family &family : families)
        if (format >= family.head && format <= family.last) return family.head;
    return format;
}

bool DxgiFormatTable::typeless(uint32_t format) const
{
    AV_SCOPE("DxgiFormatTable::typeless");
    if (format == bgra_head || format == bgrx_head) return true;
    for (const Family &family : families)
        if (format == family.head) return true;
    return false;
}

bool DxgiFormatTable::depth(uint32_t format) const
{
    AV_SCOPE("DxgiFormatTable::depth");
    for (const uint32_t candidate : depth_formats)
        if (format == candidate) return true;
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
    if (depth(format)) {
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
    const uint32_t *const shared = family_levels_[family_of(format)];
    for (uint32_t step = 1u; step < sample_steps; step++)
        if (shared[step] != 0u) return true;
    return false;
}

uint32_t DxgiFormatTable::format_support(uint32_t format) const
{
    AV_SCOPE("DxgiFormatTable::format_support");
    if (format == 0u || format >= format_count) return 0u;
    uint32_t support = carried(format);
    /* Multisampled use at no count past one is a table the runtime refuses
     * the driver over ("MSAA quality reported to be 0"). */
    if (!multisamples(format)) support &= ~(ddi_multisample_render_target | ddi_multisample_load);
    return support;
}

/* One sample is every format's one quality level. Past one, the runtime holds
 * each family to one story: the typeless parent answers what its
 * multisampled children do, a child that claims multisampled rendering
 * answers the same, and a child that does not claim it answers none. */
uint32_t DxgiFormatTable::quality_levels(uint32_t format, uint32_t samples) const
{
    AV_SCOPE("DxgiFormatTable::quality_levels");
    const uint32_t step = step_of(samples);
    if (format == 0u || format >= format_count || step == sample_steps) return 0u;
    if (step == 0u) return 1u;
    if (!typeless(format) && !multisamples(format)) return 0u;
    return family_levels_[family_of(format)][step];
}

uint32_t DxgiFormatTable::own_format_support(uint32_t format) const
{
    AV_SCOPE("DxgiFormatTable::own_format_support");
    if (format == 0u || format >= format_count) return 0u;
    uint32_t support = carried(format);
    bool     counted = false;
    for (uint32_t step = 1u; step < sample_steps; step++) counted = counted || levels_[format][step] != 0u;
    if (!counted) support &= ~(ddi_multisample_render_target | ddi_multisample_load);
    return support;
}

uint32_t DxgiFormatTable::own_quality_levels(uint32_t format, uint32_t samples) const
{
    AV_SCOPE("DxgiFormatTable::own_quality_levels");
    const uint32_t step = step_of(samples);
    if (format >= format_count || step == sample_steps) return 0u;
    if (step == 0u) return 1u;
    if (!typeless(format) && (own_format_support(format) & ddi_multisample_render_target) == 0u) return 0u;
    return levels_[format][step];
}

bool DxgiFormatTable::multisample_candidate(uint32_t format) const
{
    AV_SCOPE("DxgiFormatTable::multisample_candidate");
    return format < format_count && (carried(format) & ddi_multisample_render_target) != 0u;
}

uint32_t DxgiFormatTable::step_of(uint32_t samples) const
{
    AV_SCOPE("DxgiFormatTable::step_of");
    for (uint32_t step = 0u; step < sample_steps; step++)
        if (samples == 1u << step) return step;
    return sample_steps;
}

}  // namespace av::d3d
