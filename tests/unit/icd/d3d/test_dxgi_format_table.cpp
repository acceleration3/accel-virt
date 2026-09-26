/* What a Direct3D runtime is told of each DXGI format, from what the host's
 * device answered: the host's D3D11 support words become DDI bits, a family's
 * multisampling is pooled for Direct3D 12 and each format's own for
 * Direct3D 11, and no answer is one the runtime refuses the driver over. */
#include "harness/framework/test_registry.hpp"
#include "icd/d3d/dxgi_format_table.hpp"

#include <cstdint>

using av::d3d::DxgiFormatTable;

namespace {

/* D3D11_FORMAT_SUPPORT and _SUPPORT2 bits, as the host answers them. */
constexpr std::uint32_t api_buffer = 0x1u;
constexpr std::uint32_t api_sample = 0x200u;
constexpr std::uint32_t api_render_target = 0x4000u;
constexpr std::uint32_t api_blendable = 0x8000u;
constexpr std::uint32_t api_depth_stencil = 0x10000u;
constexpr std::uint32_t api_display = 0x80000u;
constexpr std::uint32_t api_multisample_render_target = 0x200000u;
constexpr std::uint32_t api_multisample_load = 0x400000u;
constexpr std::uint32_t api2_uav_reads = 0x40u;
constexpr std::uint32_t api2_logic_op = 0x100u;

/* D3D10_DDI_FORMAT_SUPPORT bits, as the runtime is told them. */
constexpr std::uint32_t ddi_sample = 0x1u;
constexpr std::uint32_t ddi_render_target = 0x2u;
constexpr std::uint32_t ddi_blendable = 0x4u;
constexpr std::uint32_t ddi_multisample_render_target = 0x8u;
constexpr std::uint32_t ddi_multisample_load = 0x10u;
constexpr std::uint32_t ddi_buffer = 0x400u;
constexpr std::uint32_t ddi_logic_op = 0x2000u;
constexpr std::uint32_t ddi_uav_reads = 0x20000u;
constexpr std::uint32_t ddi_display = 0x200000u;

/* DXGI_FORMAT numbers. */
constexpr std::uint32_t rgba32_float = 2u;
constexpr std::uint32_t rgba8_typeless = 27u, rgba8_unorm = 28u, rgba8_srgb = 29u, rgba8_uint = 30u,
                        rgba8_snorm = 31u;
constexpr std::uint32_t d24_s8 = 45u;
constexpr std::uint32_t bgra_unorm = 87u, bgra_typeless = 90u;
constexpr std::uint32_t r16_typeless = 53u;

constexpr std::uint32_t renders = api_render_target | api_blendable | api_multisample_render_target |
                                  api_multisample_load | api_sample;

/* An RGBA8 family whose two multisampled members disagree on their levels,
 * and a member that renders but does not multisample. */
void record_rgba8(DxgiFormatTable &table)
{
    table.record(rgba8_unorm, renders | api_display, 0u);
    table.record_samples(rgba8_unorm, 2u, 1u);
    table.record_samples(rgba8_unorm, 4u, 3u);
    table.record(rgba8_srgb, renders, 0u);
    table.record_samples(rgba8_srgb, 2u, 2u);
    table.record_samples(rgba8_srgb, 4u, 5u);
    table.record(rgba8_uint, api_render_target | api_sample, 0u);
}

}  // namespace

TEST(dxgi_format_table, the_hosts_bits_become_the_ddis_bits)
{ DxgiFormatTable table;
  record_rgba8(table);
  table.record(rgba32_float, api_buffer | api_sample, api2_uav_reads | api2_logic_op);
  table.settle();
  CHECK_EQ(table.format_support(rgba8_unorm),
           ddi_sample | ddi_render_target | ddi_blendable | ddi_multisample_render_target | ddi_multisample_load |
               ddi_display);
  CHECK_EQ(table.format_support(rgba32_float), ddi_buffer | ddi_sample | ddi_uav_reads | ddi_logic_op);
  /* Nothing recorded, nothing claimed; and no format is 0 or past the table. */
  CHECK_EQ(table.format_support(1u), 0u);
  CHECK_EQ(table.format_support(0u), 0u);
  CHECK_EQ(table.format_support(DxgiFormatTable::format_count), 0u);
  CHECK_EQ(table.own_format_support(0u), 0u);
  CHECK_EQ(table.own_format_support(DxgiFormatTable::format_count), 0u); }

/* Blending and multisampled rendering are rendering: claimed without a render
 * target, they are not told. */
TEST(dxgi_format_table, blending_without_rendering_is_not_claimed)
{ DxgiFormatTable table;
  table.record(rgba32_float, api_blendable | api_multisample_render_target, 0u);
  table.record_samples(rgba32_float, 4u, 1u);
  table.settle();
  CHECK_EQ(table.format_support(rgba32_float), 0u);
  CHECK(!table.multisample_candidate(rgba32_float));
  CHECK_EQ(table.quality_levels(rgba32_float, 4u), 0u); }

/* A depth format's render target is its depth-stencil view, and it is never
 * sampled itself. */
TEST(dxgi_format_table, a_depth_format_renders_by_its_depth_stencil_and_is_never_sampled)
{ DxgiFormatTable table;
  table.record(d24_s8, api_depth_stencil | api_sample | api_buffer | api_multisample_render_target, 0u);
  table.settle();
  /* No level at any count: its multisampling is not told either. */
  CHECK_EQ(table.format_support(d24_s8), ddi_render_target);
  table.record_samples(d24_s8, 4u, 1u);
  table.settle();
  CHECK_EQ(table.format_support(d24_s8), ddi_render_target | ddi_multisample_render_target);
  CHECK_EQ(table.quality_levels(d24_s8, 4u), 1u);
  /* Without the depth-stencil bit it does not render at all. */
  DxgiFormatTable bare;
  bare.record(d24_s8, api_sample, 0u);
  bare.settle();
  CHECK_EQ(bare.format_support(d24_s8), 0u); }

/* Direct3D 12 holds a family to one story: every multisampled member answers
 * the fewest levels any of them has, the typeless parent the same, and a
 * member that does not multisample answers none. */
TEST(dxgi_format_table, direct3d_12_is_told_each_familys_pooled_levels)
{ DxgiFormatTable table;
  record_rgba8(table);
  table.settle();
  CHECK_EQ(table.quality_levels(rgba8_unorm, 4u), 3u);
  CHECK_EQ(table.quality_levels(rgba8_srgb, 4u), 3u);
  CHECK_EQ(table.quality_levels(rgba8_typeless, 4u), 3u);
  CHECK_EQ(table.quality_levels(rgba8_unorm, 2u), 1u);
  CHECK_EQ(table.quality_levels(rgba8_srgb, 2u), 1u);
  CHECK_EQ(table.quality_levels(rgba8_uint, 4u), 0u);
  /* A member that claims multisampling with no count of its own takes the
   * whole family down to none: the fewest levels any claimant has. */
  table.record(rgba8_snorm, renders, 0u);
  table.settle();
  CHECK_EQ(table.quality_levels(rgba8_unorm, 4u), 0u);
  CHECK_EQ(table.quality_levels(rgba8_typeless, 4u), 0u);
  CHECK_EQ(table.format_support(rgba8_unorm) & (ddi_multisample_render_target | ddi_multisample_load), 0u);
  table.record(rgba8_snorm, 0u, 0u);
  table.settle();
  /* No count past the family's recorded ones: 8 and 16 samples are none. */
  CHECK_EQ(table.quality_levels(rgba8_unorm, 8u), 0u);
  CHECK_EQ(table.quality_levels(rgba8_unorm, 16u), 0u);
  /* One sample is every format's one level; a count that is no power of two,
   * or past 16, is no count. */
  CHECK_EQ(table.quality_levels(rgba8_uint, 1u), 1u);
  CHECK_EQ(table.quality_levels(rgba8_unorm, 3u), 0u);
  CHECK_EQ(table.quality_levels(rgba8_unorm, 32u), 0u);
  CHECK_EQ(table.quality_levels(0u, 1u), 0u);
  CHECK_EQ(table.quality_levels(DxgiFormatTable::format_count, 1u), 0u); }

/* Direct3D 11 is told each format's own answer. Pooled, formats DWM draws its
 * acrylic backdrop in lost their multisampling. */
TEST(dxgi_format_table, direct3d_11_is_told_each_formats_own_levels)
{ DxgiFormatTable table;
  record_rgba8(table);
  table.settle();
  CHECK_EQ(table.own_quality_levels(rgba8_srgb, 4u), 5u);
  CHECK_EQ(table.own_quality_levels(rgba8_unorm, 4u), 3u);
  CHECK_EQ(table.own_quality_levels(rgba8_srgb, 2u), 2u);
  /* A member that claims multisampling with no level of its own is told none,
   * and its multisampled bits go. */
  table.record(rgba8_snorm, renders, 0u);
  CHECK_EQ(table.own_quality_levels(rgba8_snorm, 4u), 0u);
  CHECK_EQ(table.own_format_support(rgba8_snorm) & (ddi_multisample_render_target | ddi_multisample_load), 0u);
  CHECK_EQ(table.own_format_support(rgba8_unorm),
           ddi_sample | ddi_render_target | ddi_blendable | ddi_multisample_render_target | ddi_multisample_load |
               ddi_display);
  CHECK_EQ(table.own_quality_levels(rgba8_uint, 4u), 0u);
  /* The typeless parent answers its own, which nothing recorded. */
  CHECK_EQ(table.own_quality_levels(rgba8_typeless, 4u), 0u);
  table.record_samples(rgba8_typeless, 4u, 7u);
  CHECK_EQ(table.own_quality_levels(rgba8_typeless, 4u), 7u);
  CHECK_EQ(table.own_quality_levels(rgba8_uint, 1u), 1u);
  CHECK_EQ(table.own_quality_levels(rgba8_unorm, 3u), 0u);
  CHECK_EQ(table.own_quality_levels(DxgiFormatTable::format_count, 1u), 0u); }

/* The two BGRA families are named member by member, not numbered after their
 * head. */
TEST(dxgi_format_table, a_bgra_member_pools_under_its_typeless_head)
{ DxgiFormatTable table;
  table.record(bgra_unorm, renders, 0u);
  table.record_samples(bgra_unorm, 4u, 2u);
  table.settle();
  CHECK_EQ(table.quality_levels(bgra_typeless, 4u), 2u);
  CHECK_EQ(table.quality_levels(bgra_unorm, 4u), 2u);
  /* A family nobody multisamples settles at none. */
  CHECK_EQ(table.quality_levels(r16_typeless, 4u), 0u); }

TEST(dxgi_format_table, a_multisample_candidate_claims_multisampled_rendering)
{ DxgiFormatTable table;
  record_rgba8(table);
  table.record(rgba8_snorm, renders, 0u);
  CHECK(table.multisample_candidate(rgba8_unorm));
  CHECK(table.multisample_candidate(rgba8_snorm));
  CHECK(!table.multisample_candidate(rgba8_uint));
  CHECK(!table.multisample_candidate(DxgiFormatTable::format_count)); }

/* What is out of the table's range is not recorded, and changes nothing. */
TEST(dxgi_format_table, an_answer_for_no_format_or_no_count_is_dropped)
{ DxgiFormatTable table;
  table.record(DxgiFormatTable::format_count, renders, 0u);
  table.record_samples(DxgiFormatTable::format_count, 4u, 9u);
  table.record(rgba8_unorm, renders, 0u);
  table.record_samples(rgba8_unorm, 3u, 9u);
  table.record_samples(rgba8_unorm, 32u, 9u);
  table.settle();
  CHECK_EQ(table.own_quality_levels(rgba8_unorm, 4u), 0u);
  CHECK_EQ(table.own_quality_levels(rgba8_unorm, 2u), 0u);
  CHECK_EQ(table.format_support(rgba8_unorm) & ddi_multisample_render_target, 0u); }
