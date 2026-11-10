#include "executor/d3d/d3d12_address_map.hpp"
#include "executor/d3d/d3d12_raytracing_inputs.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"

#include <cstdint>

using av::D3d12AddressMap;
using av::D3d12RaytracingInputs;
using av::MockPlatform;
using av::Status;
using av::d3d12proto::RaytracingGeometry;

namespace {

constexpr std::uint32_t format_r32g32b32 = 6u, format_r32_uint = 42u, format_r16_uint = 57u;

struct Rig {
    MockPlatform platform;
    D3d12AddressMap map{platform};
    D3d12RaytracingInputs inputs{platform, map};

    Rig()
    {
        static_cast<void>(map.insert(0x100000u, 0x10000u, 0xA00000u, 1u));
        static_cast<void>(map.insert(0x200000u, 0x1000u, 0xB00000u, 2u));
    }
};

RaytracingGeometry triangles()
{
    RaytracingGeometry made = {};
    made.vertex_format = format_r32g32b32;
    made.index_format = format_r16_uint;
    made.index_count = 6u;
    made.vertex_count = 4u;
    made.index_buffer = 0x100100u;
    made.start = 0x100200u;
    made.stride = 12u;
    return made;
}

D3d12RaytracingInputs::Shape bottom(const RaytracingGeometry *geometries, std::uint32_t count)
{
    return D3d12RaytracingInputs::Shape{1u, 0u, count, 0u, 0u, geometries, count};
}

}

TEST(d3d12_raytracing_inputs_bottom, translates_every_address_a_triangle_geometry_names)
{ Rig rig;
  RaytracingGeometry geometry = triangles();
  geometry.transform = 0x100300u;
  CHECK(rig.inputs.read(bottom(&geometry, 1u), true));
  const auto &made = rig.inputs.inputs().geometries[0].triangles;
  CHECK(made.vertices.start == 0xA00200u);
  CHECK(made.index_buffer == 0xA00100u);
  CHECK(made.transform == 0xA00300u);
  CHECK_EQ(made.index_count, 6u);
  CHECK(!rig.inputs.top_level()); }

TEST(d3d12_raytracing_inputs_bottom, refuses_an_address_no_buffer_holds_or_a_range_that_runs_past_its_buffer)
{ Rig rig;
  RaytracingGeometry geometry = triangles();
  geometry.start = 0x900000u;
  CHECK(!rig.inputs.read(bottom(&geometry, 1u), true));
  geometry = triangles();
  geometry.start = 0x100000u + 0x10000u - 44u;
  CHECK(!rig.inputs.read(bottom(&geometry, 1u), true));
  geometry = triangles();
  geometry.start = 0x100000u + 0x10000u - 48u;
  CHECK(rig.inputs.read(bottom(&geometry, 1u), true));
  geometry = triangles();
  geometry.index_buffer = 0x200000u + 0x1000u - 8u;
  CHECK(!rig.inputs.read(bottom(&geometry, 1u), true)); }

TEST(d3d12_raytracing_inputs_bottom, refuses_misaligned_addresses_formats_and_strides_the_host_cannot_read)
{ Rig rig;
  RaytracingGeometry geometry = triangles();
  geometry.start += 2u;
  CHECK(!rig.inputs.read(bottom(&geometry, 1u), true));
  geometry = triangles();
  geometry.stride = 8u;
  CHECK(!rig.inputs.read(bottom(&geometry, 1u), true));
  geometry = triangles();
  geometry.vertex_format = 28u;
  CHECK(!rig.inputs.read(bottom(&geometry, 1u), true));
  geometry = triangles();
  geometry.index_format = format_r32_uint;
  geometry.index_buffer += 2u;
  CHECK(!rig.inputs.read(bottom(&geometry, 1u), true));
  geometry = triangles();
  geometry.transform = 0x100308u;
  CHECK(!rig.inputs.read(bottom(&geometry, 1u), true)); }

TEST(d3d12_raytracing_inputs_bottom, an_unindexed_geometry_names_no_index_format_count_or_buffer)
{ Rig rig;
  RaytracingGeometry geometry = triangles();
  geometry.index_format = 0u;
  geometry.index_count = 0u;
  geometry.index_buffer = 0u;
  CHECK(rig.inputs.read(bottom(&geometry, 1u), true));
  geometry.index_count = 3u;
  CHECK(!rig.inputs.read(bottom(&geometry, 1u), true));
  geometry.index_count = 0u;
  geometry.index_buffer = 0x100100u;
  CHECK(!rig.inputs.read(bottom(&geometry, 1u), true)); }

TEST(d3d12_raytracing_inputs_bottom, box_geometries_are_read_by_stride_and_count)
{ Rig rig;
  RaytracingGeometry geometry = {};
  geometry.type = 1u;
  geometry.start = 0x100000u;
  geometry.stride = 24u;
  geometry.count = 10u;
  CHECK(rig.inputs.read(bottom(&geometry, 1u), true));
  CHECK(rig.inputs.inputs().geometries[0].aabbs.boxes.start == 0xA00000u);
  geometry.stride = 20u;
  CHECK(!rig.inputs.read(bottom(&geometry, 1u), true));
  geometry.stride = 0x2000u;
  geometry.count = 10u;
  CHECK(!rig.inputs.read(bottom(&geometry, 1u), true)); }

TEST(d3d12_raytracing_inputs_bottom, refuses_a_geometry_count_that_is_not_the_description_count_or_is_too_large)
{ Rig rig;
  RaytracingGeometry geometry = triangles();
  D3d12RaytracingInputs::Shape shape = bottom(&geometry, 1u);
  shape.count = 2u;
  CHECK(!rig.inputs.read(shape, true));
  shape = bottom(&geometry, 1u);
  shape.geometry_count = 0u;
  CHECK(!rig.inputs.read(shape, true));
  shape = bottom(&geometry, D3d12RaytracingInputs::most_geometries + 1u);
  CHECK(!rig.inputs.read(shape, true)); }

TEST(d3d12_raytracing_inputs_top, translates_the_instance_buffer_and_refuses_one_too_small_for_its_count)
{ Rig rig;
  D3d12RaytracingInputs::Shape shape = {0u, 0u, 4u, 0u, 0x100000u, nullptr, 0u};
  CHECK(rig.inputs.read(shape, true));
  CHECK(rig.inputs.top_level());
  CHECK(rig.inputs.inputs().instances == 0xA00000u);
  shape.instances = 0x200000u + 0x1000u - 192u;
  CHECK(!rig.inputs.read(shape, true));
  shape.instances = 0x200000u + 0x1000u - 256u;
  CHECK(rig.inputs.read(shape, true));
  shape.instances = 0x100008u;
  CHECK(!rig.inputs.read(shape, true));
  shape.instances = 0x100000u;
  shape.layout = 1u;
  CHECK(!rig.inputs.read(shape, true));
  shape.layout = 0u;
  shape.geometry_count = 1u;
  CHECK(!rig.inputs.read(shape, true)); }

TEST(d3d12_raytracing_inputs_flags, only_the_build_flags_of_the_api_are_carried)
{ Rig rig;
  RaytracingGeometry geometry = triangles();
  D3d12RaytracingInputs::Shape shape = bottom(&geometry, 1u);
  shape.flags = 0x22u;
  CHECK(rig.inputs.read(shape, true));
  CHECK(rig.inputs.compacts());
  CHECK(rig.inputs.updates());
  shape.flags = 0x40u;
  CHECK(!rig.inputs.read(shape, true));
  shape.flags = 0u;
  shape.type = 2u;
  CHECK(!rig.inputs.read(shape, true)); }

TEST(d3d12_raytracing_inputs_shape, a_prebuild_reads_no_address_but_notes_which_are_set)
{ Rig rig;
  RaytracingGeometry geometry = triangles();
  geometry.start = 0x7777u;
  geometry.index_buffer = 0u;
  geometry.transform = 0x1u;
  CHECK(rig.inputs.read(bottom(&geometry, 1u), false));
  const auto &made = rig.inputs.inputs().geometries[0].triangles;
  CHECK(made.vertices.start != 0u);
  CHECK(made.index_buffer == 0u);
  CHECK(made.transform != 0u);
  geometry.vertex_format = 0u;
  CHECK(!rig.inputs.read(bottom(&geometry, 1u), false)); }

TEST(d3d12_raytracing_inputs_vertex_bytes, is_the_size_of_each_format_the_api_allows)
{ CHECK_EQ(D3d12RaytracingInputs::vertex_bytes(format_r32g32b32), 12u);
  CHECK_EQ(D3d12RaytracingInputs::vertex_bytes(16u), 8u);
  CHECK_EQ(D3d12RaytracingInputs::vertex_bytes(10u), 8u);
  CHECK_EQ(D3d12RaytracingInputs::vertex_bytes(34u), 4u);
  CHECK_EQ(D3d12RaytracingInputs::vertex_bytes(87u), 0u); }
