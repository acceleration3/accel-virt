#include "executor/d3d/d3d12_layout_barrier.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>
#include <initializer_list>

using av::D3d12LayoutBarrier;
using av::d3d12proto::LayoutBarrier;

namespace {
LayoutBarrier texture()
{ LayoutBarrier barrier = {};
  barrier.type = 1u;
  barrier.sync_before = 0x40u;
  barrier.sync_after = 0x10u;
  barrier.access_before = 0x8u;
  barrier.access_after = 0x80u;
  barrier.layout_before = 2u;
  barrier.layout_after = 6u;
  barrier.resource = 4u;
  barrier.mip_count = 1u;
  barrier.slice_count = 1u;
  barrier.plane_count = 1u;
  return barrier; }
}

TEST(d3d12_layout_barrier, a_render_target_to_shader_resource_texture_barrier_holds)
{ const LayoutBarrier barrier = texture();
  CHECK(D3d12LayoutBarrier(barrier).holds()); }

TEST(d3d12_layout_barrier, a_type_sync_access_layout_flag_or_plane_count_the_host_does_not_know_is_refused)
{ LayoutBarrier barrier = texture();
  barrier.type = 3u;
  CHECK(!D3d12LayoutBarrier(barrier).holds());
  barrier = texture();
  barrier.sync_before = 0x100000u;
  CHECK(!D3d12LayoutBarrier(barrier).holds());
  barrier = texture();
  barrier.sync_after = 0x20000u;
  CHECK(!D3d12LayoutBarrier(barrier).holds());
  barrier = texture();
  barrier.access_after = 0x20000u;
  CHECK(!D3d12LayoutBarrier(barrier).holds());
  barrier = texture();
  barrier.layout_after = 12u;
  CHECK(!D3d12LayoutBarrier(barrier).holds());
  barrier = texture();
  barrier.layout_before = 30u;
  CHECK(!D3d12LayoutBarrier(barrier).holds());
  barrier = texture();
  barrier.layout_after = 0x80000004u;
  CHECK(!D3d12LayoutBarrier(barrier).holds());
  barrier = texture();
  barrier.flags = 2u;
  CHECK(!D3d12LayoutBarrier(barrier).holds());
  barrier = texture();
  barrier.plane_count = 3u;
  CHECK(!D3d12LayoutBarrier(barrier).holds()); }

TEST(d3d12_layout_barrier, split_syncs_no_access_and_undefined_layouts_hold_and_a_resource_is_needed_by_texture_and_buffer_barriers)
{ LayoutBarrier barrier = texture();
  barrier.sync_before = 0x80000000u;
  barrier.access_after = 0x80000000u;
  barrier.layout_before = 0xFFFFFFFFu;
  CHECK(D3d12LayoutBarrier(barrier).holds());
  barrier.resource = 0u;
  CHECK(!D3d12LayoutBarrier(barrier).holds());
  LayoutBarrier global = {};
  global.type = 0u;
  global.sync_before = 1u;
  global.sync_after = 1u;
  CHECK(D3d12LayoutBarrier(global).holds());
  LayoutBarrier buffer = {};
  buffer.type = 2u;
  CHECK(!D3d12LayoutBarrier(buffer).holds());
  buffer.resource = 2u;
  CHECK(D3d12LayoutBarrier(buffer).holds()); }

TEST(d3d12_layout_barrier, a_subresource_range_must_lie_inside_the_texture)
{ LayoutBarrier barrier = texture();
  barrier.mip_count = 3u;
  barrier.slice_count = 2u;
  CHECK(D3d12LayoutBarrier(barrier).fits(3u, 2u, 3u, 1u));
  CHECK(!D3d12LayoutBarrier(barrier).fits(3u, 2u, 2u, 1u));
  CHECK(!D3d12LayoutBarrier(barrier).fits(3u, 1u, 3u, 1u));
  barrier.first_slice = 1u;
  CHECK(!D3d12LayoutBarrier(barrier).fits(3u, 2u, 3u, 1u));
  barrier = texture();
  barrier.first_plane = 1u;
  CHECK(!D3d12LayoutBarrier(barrier).fits(3u, 1u, 1u, 1u));
  CHECK(D3d12LayoutBarrier(barrier).fits(3u, 1u, 1u, 2u));
  barrier = texture();
  barrier.slice_count = 0u;
  CHECK(!D3d12LayoutBarrier(barrier).fits(3u, 1u, 1u, 1u));
  barrier = texture();
  barrier.first_mip = 0xFFFFFFFFu;
  barrier.mip_count = 2u;
  CHECK(!D3d12LayoutBarrier(barrier).fits(3u, 1u, 4u, 1u)); }

TEST(d3d12_layout_barrier, a_subresource_index_with_no_mip_count_lies_inside_the_texture)
{ LayoutBarrier barrier = texture();
  barrier.mip_count = 0u;
  barrier.first_mip = 5u;
  CHECK(D3d12LayoutBarrier(barrier).fits(3u, 2u, 3u, 1u));
  barrier.first_mip = 6u;
  CHECK(!D3d12LayoutBarrier(barrier).fits(3u, 2u, 3u, 1u));
  barrier.first_mip = 0xFFFFFFFFu;
  CHECK(D3d12LayoutBarrier(barrier).fits(3u, 2u, 3u, 1u)); }

TEST(d3d12_layout_barrier, depth_stencil_and_planar_formats_have_two_planes)
{ CHECK_EQ(D3d12LayoutBarrier::planes_of(45u), 2u);
  CHECK_EQ(D3d12LayoutBarrier::planes_of(20u), 2u);
  CHECK_EQ(D3d12LayoutBarrier::planes_of(103u), 2u);
  CHECK_EQ(D3d12LayoutBarrier::planes_of(28u), 1u);
  CHECK_EQ(D3d12LayoutBarrier::planes_of(40u), 1u); }

TEST(d3d12_layout_barrier, the_layouts_the_runtime_makes_of_legacy_transitions_become_the_api_layouts_they_stand_for)
{ LayoutBarrier barrier = texture();
  for (const std::uint32_t legacy : {0x80000000u, 0x80000001u, 0x80000002u, 0x80000003u, 31u}) {
      barrier.layout_before = legacy;
      CHECK(D3d12LayoutBarrier(barrier).holds());
  }
  CHECK_EQ(D3d12LayoutBarrier::api_layout(0x80000000u), 7u);
  CHECK_EQ(D3d12LayoutBarrier::api_layout(0x80000001u), 8u);
  CHECK_EQ(D3d12LayoutBarrier::api_layout(0x80000002u), 6u);
  CHECK_EQ(D3d12LayoutBarrier::api_layout(0x80000003u), 6u);
  CHECK_EQ(D3d12LayoutBarrier::api_layout(31u), 1u);
  CHECK_EQ(D3d12LayoutBarrier::api_layout(2u), 2u);
  CHECK_EQ(D3d12LayoutBarrier::api_layout(0xFFFFFFFFu), 0xFFFFFFFFu); }
