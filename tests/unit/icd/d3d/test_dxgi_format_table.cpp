#include "harness/framework/test_registry.hpp"
#include "icd/d3d/dxgi_format_table.hpp"
#include "proto/d3d/dxgi_format.hpp"

#include <cstdint>
#include <initializer_list>

using av::d3d::DxgiFormatTable;

namespace {
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

constexpr std::uint32_t ddi_sample = 0x1u;
constexpr std::uint32_t ddi_render_target = 0x2u;
constexpr std::uint32_t ddi_blendable = 0x4u;
constexpr std::uint32_t ddi_multisample_render_target = 0x8u;
constexpr std::uint32_t ddi_multisample_load = 0x10u;
constexpr std::uint32_t ddi_buffer = 0x400u;
constexpr std::uint32_t ddi_logic_op = 0x2000u;
constexpr std::uint32_t ddi_uav_reads = 0x20000u;
constexpr std::uint32_t ddi_display = 0x200000u;
constexpr std::uint32_t ddi_multiplane_overlay = 0x8000u;

constexpr std::uint32_t rgba32_float = 2u;
constexpr std::uint32_t rgb32_typeless = 5u, rgb32_float = 6u, rgb32_uint = 7u;
constexpr std::uint32_t rgba8_typeless = 27u, rgba8_unorm = 28u, rgba8_srgb = 29u, rgba8_uint = 30u,
                        rgba8_snorm = 31u;
constexpr std::uint32_t d24_s8 = 45u;
constexpr std::uint32_t bgra_unorm = 87u, bgra_typeless = 90u;
constexpr std::uint32_t r16_typeless = 53u;

struct Family {
    std::uint32_t head, last;
};
constexpr Family families[] = {{1u, 4u},   {5u, 8u},   {9u, 14u},  {15u, 18u}, {19u, 22u}, {23u, 25u},
                               {27u, 32u}, {33u, 38u}, {39u, 43u}, {44u, 47u}, {48u, 52u}, {53u, 59u},
                               {60u, 64u}, {70u, 72u}, {73u, 75u}, {76u, 78u}, {79u, 81u}, {82u, 84u},
                               {94u, 96u}, {97u, 99u}};

std::uint32_t typeless_parent(std::uint32_t format)
{
    if (format == 87u || format == 91u) return 90u;
    if (format == 88u || format == 93u) return 92u;
    for (const Family &family : families)
        if (format >= family.head && format <= family.last) return family.head;
    return format;
}

bool is_typeless(std::uint32_t format)
{
    if (format == 90u || format == 92u) return true;
    for (const Family &family : families)
        if (format == family.head) return true;
    return false;
}

constexpr std::uint32_t renders = api_render_target | api_blendable | api_multisample_render_target |
                                  api_multisample_load | api_sample;

void record_rgba8(DxgiFormatTable &table)
{
    table.record(rgba8_unorm, renders | api_display, 0u);
    table.record_samples(rgba8_unorm, 2u, 1u);
    table.record_samples(rgba8_unorm, 4u, 3u);
    table.record(rgba8_srgb, renders, 0u);
    table.record_samples(rgba8_srgb, 2u, 2u);
    table.record_samples(rgba8_srgb, 4u, 5u);
    table.record(rgba8_uint, api_render_target | api_sample | api_multisample_load, 0u);
}

}

TEST(dxgi_format_table, the_hosts_bits_become_the_ddis_bits)
{ DxgiFormatTable table;
  record_rgba8(table);
  table.record(rgba32_float, api_buffer | api_sample, api2_uav_reads | api2_logic_op);
  table.settle();
  CHECK_EQ(table.format_support(rgba8_unorm),
           ddi_sample | ddi_render_target | ddi_blendable | ddi_multisample_render_target | ddi_multisample_load |
               ddi_display | ddi_multiplane_overlay);
  CHECK_EQ(table.format_support(rgba32_float), ddi_buffer | ddi_sample | ddi_uav_reads | ddi_logic_op);
  CHECK_EQ(table.format_support(1u), 0u);
  CHECK_EQ(table.format_support(0u), 0u);
  CHECK_EQ(table.format_support(av::DxgiFormat::count), 0u); }

TEST(dxgi_format_table, an_overlay_plane_takes_the_displayed_32_bit_rgb_formats)
{ DxgiFormatTable table;
  constexpr std::uint32_t bgra8_unorm = 87u, bgrx8_unorm = 88u, bgra8_srgb = 91u;
  table.record(bgra8_unorm, api_sample | api_render_target | api_display, 0u);
  table.record(bgrx8_unorm, api_sample | api_render_target, 0u);
  table.record(bgra8_srgb, api_sample | api_render_target | api_display, 0u);
  table.record(rgba32_float, api_sample | api_display, 0u);
  table.settle();
  CHECK((table.format_support(bgra8_unorm) & ddi_multiplane_overlay) != 0u);
  CHECK_EQ(table.format_support(bgrx8_unorm) & ddi_multiplane_overlay, 0u);
  CHECK_EQ(table.format_support(bgra8_srgb) & ddi_multiplane_overlay, 0u);
  CHECK_EQ(table.format_support(rgba32_float) & ddi_multiplane_overlay, 0u); }

TEST(dxgi_format_table, a_format_the_host_tiles_is_told_apart_from_its_other_support)
{ DxgiFormatTable table;
  constexpr std::uint32_t api2_tiled = 0x200u, ddi_tiled = 0x10000u;
  table.record(rgba8_unorm, api_sample, api2_tiled);
  table.record(rgba32_float, api_sample, api2_uav_reads);
  table.settle();
  CHECK_EQ(table.tiled_support(rgba8_unorm), ddi_tiled);
  CHECK_EQ(table.tiled_support(rgba32_float), 0u);
  CHECK_EQ(table.tiled_support(av::DxgiFormat::count), 0u);
  CHECK_EQ(table.format_support(rgba8_unorm) & ddi_tiled, 0u); }

TEST(dxgi_format_table, blending_without_rendering_is_not_claimed)
{ DxgiFormatTable table;
  table.record(rgba32_float, api_blendable | api_multisample_render_target, 0u);
  table.record_samples(rgba32_float, 4u, 1u);
  table.settle();
  CHECK_EQ(table.format_support(rgba32_float), 0u);
  CHECK(!table.multisample_candidate(rgba32_float));
  CHECK_EQ(table.quality_levels(rgba32_float, 4u), 0u); }

TEST(dxgi_format_table, a_depth_format_renders_by_its_depth_stencil_and_is_never_sampled)
{ DxgiFormatTable table;
  table.record(d24_s8, api_depth_stencil | api_sample | api_buffer | api_multisample_render_target, 0u);
  table.settle();
  CHECK_EQ(table.format_support(d24_s8), ddi_render_target);
  table.record_samples(d24_s8, 4u, 1u);
  table.settle();
  CHECK_EQ(table.format_support(d24_s8), ddi_render_target | ddi_multisample_render_target);
  CHECK_EQ(table.quality_levels(d24_s8, 4u), 1u);
  DxgiFormatTable bare;
  bare.record(d24_s8, api_sample, 0u);
  bare.settle();
  CHECK_EQ(bare.format_support(d24_s8), 0u); }

TEST(dxgi_format_table, a_family_answers_one_set_of_levels)
{ DxgiFormatTable table;
  record_rgba8(table);
  table.settle();
  CHECK_EQ(table.quality_levels(rgba8_unorm, 4u), 3u);
  CHECK_EQ(table.quality_levels(rgba8_srgb, 4u), 3u);
  CHECK_EQ(table.quality_levels(rgba8_typeless, 4u), 3u);
  CHECK_EQ(table.quality_levels(rgba8_unorm, 2u), 1u);
  CHECK_EQ(table.quality_levels(rgba8_srgb, 2u), 1u);
  CHECK_EQ(table.quality_levels(rgba8_uint, 4u), 0u);
  CHECK_EQ(table.quality_levels(rgba8_unorm, 8u), 0u);
  CHECK_EQ(table.quality_levels(rgba8_unorm, 16u), 0u);
  CHECK_EQ(table.quality_levels(rgba8_uint, 1u), 1u);
  CHECK_EQ(table.quality_levels(rgba8_unorm, 3u), 0u);
  CHECK_EQ(table.quality_levels(rgba8_unorm, 32u), 0u);
  CHECK_EQ(table.quality_levels(0u, 1u), 0u);
  CHECK_EQ(table.quality_levels(av::DxgiFormat::count, 1u), 0u); }

TEST(dxgi_format_table, a_member_with_no_count_takes_nothing_from_its_siblings)
{ DxgiFormatTable table;
  record_rgba8(table);
  table.record(rgba8_snorm, renders, 0u);
  table.settle();
  CHECK_EQ(table.quality_levels(rgba8_snorm, 4u), 0u);
  CHECK_EQ(table.format_support(rgba8_snorm) & (ddi_multisample_render_target | ddi_multisample_load),
           ddi_multisample_load);
  CHECK_EQ(table.quality_levels(rgba8_unorm, 4u), 3u);
  CHECK_EQ(table.quality_levels(rgba8_typeless, 4u), 3u);
  CHECK_EQ(table.format_support(rgba8_unorm),
           ddi_sample | ddi_render_target | ddi_blendable | ddi_multisample_render_target | ddi_multisample_load |
               ddi_display | ddi_multiplane_overlay); }

TEST(dxgi_format_table, a_member_that_takes_no_part_keeps_its_family_from_multisampling)
{ DxgiFormatTable table;
  table.record(rgb32_float, renders, 0u);
  for (std::uint32_t samples = 2u; samples <= 16u; samples *= 2u) table.record_samples(rgb32_float, samples, 1u);
  table.record(rgb32_uint, api_sample, 0u);
  table.settle();
  for (std::uint32_t samples = 2u; samples <= 16u; samples *= 2u) {
      CHECK_EQ(table.quality_levels(rgb32_typeless, samples), 0u);
      CHECK_EQ(table.quality_levels(rgb32_float, samples), 0u);
      CHECK_EQ(table.quality_levels(rgb32_uint, samples), 0u);
  }
  CHECK_EQ(table.format_support(rgb32_float) & (ddi_multisample_render_target | ddi_multisample_load), 0u); }

TEST(dxgi_format_table, a_typeless_parent_answers_what_its_members_do)
{ DxgiFormatTable table;
  table.record(rgb32_float, renders, 0u);
  for (std::uint32_t samples = 2u; samples <= 16u; samples *= 2u) table.record_samples(rgb32_float, samples, 1u);
  table.record(rgb32_uint, api_sample | api_multisample_load, 0u);
  table.settle();
  for (std::uint32_t samples = 2u; samples <= 16u; samples *= 2u) {
      CHECK_EQ(table.quality_levels(rgb32_typeless, samples), 1u);
      CHECK_EQ(table.quality_levels(rgb32_float, samples), 1u);
      CHECK_EQ(table.quality_levels(rgb32_uint, samples), 0u);
  }
  CHECK_EQ(table.format_support(rgb32_uint), ddi_sample | ddi_multisample_load); }

TEST(dxgi_format_table, a_weak_member_gives_way_to_its_siblings)
{ DxgiFormatTable table;
  for (const std::uint32_t member : {rgba8_unorm, rgba8_srgb}) {
      table.record(member, renders, 0u);
      for (std::uint32_t samples = 2u; samples <= 8u; samples *= 2u) table.record_samples(member, samples, 1u);
  }
  table.record(rgba8_snorm, renders, 0u);
  table.record_samples(rgba8_snorm, 2u, 1u);
  table.settle();
  CHECK_EQ(table.quality_levels(rgba8_unorm, 8u), 1u);
  CHECK_EQ(table.quality_levels(rgba8_srgb, 8u), 1u);
  CHECK_EQ(table.quality_levels(rgba8_typeless, 8u), 1u);
  CHECK_EQ(table.quality_levels(rgba8_snorm, 2u), 0u);
  CHECK_EQ(table.format_support(rgba8_snorm) & ddi_multisample_render_target, 0u); }

TEST(dxgi_format_table, a_strong_member_gives_its_extra_counts_to_the_family)
{ DxgiFormatTable table;
  table.record(rgba8_unorm, renders, 0u);
  for (std::uint32_t samples = 2u; samples <= 16u; samples *= 2u) table.record_samples(rgba8_unorm, samples, 1u);
  for (const std::uint32_t member : {rgba8_srgb, rgba8_snorm}) {
      table.record(member, renders, 0u);
      table.record_samples(member, 2u, 1u);
      table.record_samples(member, 4u, 1u);
  }
  table.settle();
  for (const std::uint32_t member : {rgba8_typeless, rgba8_unorm, rgba8_srgb, rgba8_snorm}) {
      CHECK_EQ(table.quality_levels(member, 4u), 1u);
      CHECK_EQ(table.quality_levels(member, 8u), 0u);
      CHECK_EQ(table.quality_levels(member, 16u), 0u);
  } }

TEST(dxgi_format_table, any_hosts_answers_keep_every_family_consistent)
{ std::uint32_t seed = 0x2545f491u;
  const auto next = [&seed]() {
      seed = seed * 1664525u + 1013904223u;
      return seed >> 8u;
  };
  for (int round = 0; round < 2000; round++) {
      DxgiFormatTable table;
      std::uint32_t   recorded[av::DxgiFormat::count][DxgiFormatTable::sample_steps] = {};
      for (std::uint32_t format = 1u; format < av::DxgiFormat::count; format++) {
          if (next() % 3u == 0u) continue;
          const std::uint32_t kind = next() % 3u;
          table.record(format,
                       kind == 0u ? renders
                                  : kind == 1u ? api_render_target | api_sample | api_multisample_load
                                               : api_render_target | api_sample,
                       0u);
          for (std::uint32_t step = 1u; step < DxgiFormatTable::sample_steps; step++) {
              if (next() % 3u == 0u) continue;
              recorded[format][step] = 1u + next() % 3u;
              table.record_samples(format, 1u << step, recorded[format][step]);
          }
      }
      table.settle();
      for (std::uint32_t format = 1u; format < av::DxgiFormat::count; format++) {
          const bool claims = (table.format_support(format) & ddi_multisample_render_target) != 0u;
          const std::uint32_t parent = typeless_parent(format);
          bool counted = false;
          for (std::uint32_t step = 1u; step < DxgiFormatTable::sample_steps; step++) {
              const std::uint32_t levels = table.quality_levels(format, 1u << step);
              counted = counted || levels != 0u;
              if (parent == format) {
                  CHECK(levels == 0u || claims || is_typeless(format));
                  continue;
              }
              CHECK(claims ? levels == table.quality_levels(parent, 1u << step) : levels == 0u);
              CHECK(levels <= recorded[format][step]);
          }
          if (claims) CHECK(counted);
          const bool takes_part = (table.format_support(format) & (ddi_multisample_render_target | ddi_multisample_load)) != 0u;
          if (parent != format && !takes_part && table.format_support(format) != 0u)
              for (std::uint32_t step = 1u; step < DxgiFormatTable::sample_steps; step++)
                  CHECK_EQ(table.quality_levels(parent, 1u << step), 0u);
      }
  } }

TEST(dxgi_format_table, a_bgra_member_pools_under_its_typeless_head)
{ DxgiFormatTable table;
  table.record(bgra_unorm, renders, 0u);
  table.record_samples(bgra_unorm, 4u, 2u);
  table.settle();
  CHECK_EQ(table.quality_levels(bgra_typeless, 4u), 2u);
  CHECK_EQ(table.quality_levels(bgra_unorm, 4u), 2u);
  CHECK_EQ(table.quality_levels(r16_typeless, 4u), 0u); }

TEST(dxgi_format_table, a_multisample_candidate_claims_multisampled_rendering)
{ DxgiFormatTable table;
  record_rgba8(table);
  table.record(rgba8_snorm, renders, 0u);
  CHECK(table.multisample_candidate(rgba8_unorm));
  CHECK(table.multisample_candidate(rgba8_snorm));
  CHECK(!table.multisample_candidate(rgba8_uint));
  CHECK(!table.multisample_candidate(av::DxgiFormat::count)); }

TEST(dxgi_format_table, an_answer_for_no_format_or_no_count_is_dropped)
{ DxgiFormatTable table;
  table.record(av::DxgiFormat::count, renders, 0u);
  table.record_samples(av::DxgiFormat::count, 4u, 9u);
  table.record(rgba8_unorm, renders, 0u);
  table.record_samples(rgba8_unorm, 3u, 9u);
  table.record_samples(rgba8_unorm, 32u, 9u);
  table.settle();
  CHECK_EQ(table.quality_levels(rgba8_unorm, 4u), 0u);
  CHECK_EQ(table.quality_levels(rgba8_unorm, 2u), 0u);
  CHECK_EQ(table.format_support(rgba8_unorm) & ddi_multisample_render_target, 0u); }

TEST(dxgi_format_table, an_overlay_plane_takes_the_video_formats_the_host_has)
{ DxgiFormatTable table;
  constexpr std::uint32_t nv12 = 103u, yuy2 = 107u;
  table.record(nv12, api_sample, 0u);
  table.settle();
  CHECK((table.format_support(nv12) & ddi_multiplane_overlay) != 0u);
  CHECK_EQ(table.format_support(yuy2) & ddi_multiplane_overlay, 0u); }
