#include "core/log/driver_log.hpp"
#include "executor/d3d/d3d12_pipeline_builder.hpp"
#include "executor/d3d/d3d12_pipeline_job.hpp"
#include "executor/d3d/d3d_object_table.hpp"
#include "executor/shim/d3d12_host_device.hpp"
#include "executor/shim/host_heap.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/icd/recording_log.hpp"

#include "fixtures/d3d12/fill_cs.hpp"

#include <cstdint>
#include <cstring>

using av::D3d12PipelineBuilder;
using namespace av::d3d12proto;

namespace {
av::HostHeap       objects_heap;
av::D3dObjectTable objects(objects_heap);

}

TEST(d3d12_pipeline_builder, an_unbound_builder_makes_nothing)
{ D3d12PipelineBuilder builder(objects);
  av::D3d12PipelineJob job;
  job.bind(objects_heap);
  CHECK(builder.root_signature(0u, nullptr, 0u, nullptr, 0u, nullptr, 0u) == nullptr);
  const std::uint8_t *code[D3d12PipelineBuilder::stage_count] = {};
  const std::uint64_t sizes[D3d12PipelineBuilder::stage_count] = {};
  CHECK(!builder.graphics(2u, 1u, GraphicsState{}, nullptr, 0u, nullptr, 0u, nullptr, 0u, nullptr, 0u, 0xFFFFFFFFu, code, sizes, job));
  CHECK(!builder.compute(2u, 1u, reinterpret_cast<const std::uint8_t *>(fill_cs::program), fill_cs::program_bytes,
                         job)); }

TEST(d3d12_pipeline_builder, a_device_vkd3d_proton_never_opened_or_a_root_it_never_made_makes_nothing)
{ D3d12PipelineBuilder builder(objects);
  av::RecordingLog    sink;
  av::DriverLog       lines(sink, "d3d12: ");
  av::D3d12HostDevice device(lines);
  builder.bind(&device);
  av::D3d12PipelineJob job;
  job.bind(objects_heap);
  CHECK(builder.root_signature(0u, nullptr, 0u, nullptr, 0u, nullptr, 0u) == nullptr);
  CHECK(!builder.compute(2u, 1u, reinterpret_cast<const std::uint8_t *>(fill_cs::program), fill_cs::program_bytes,
                         job));
  const std::uint8_t *code[D3d12PipelineBuilder::stage_count] = {};
  const std::uint64_t sizes[D3d12PipelineBuilder::stage_count] = {};
  CHECK(!builder.graphics(2u, 1u, GraphicsState{}, nullptr, 0u, nullptr, 0u, nullptr, 0u, nullptr, 0u, 0xFFFFFFFFu, code, sizes, job));
  builder.bind(nullptr); }

TEST(d3d12_pipeline_builder, a_stream_output_declaration_becomes_the_pipeline_s_stream_output)
{ D3d12PipelineBuilder builder(objects);
  av::D3d12PipelineJob job;
  job.bind(objects_heap);
  const StreamOutDeclaration declared[2] = {{0u, 0u, 0u, 0xFu}, {0u, 1u, 1u, 0x6u}};
  const std::uint32_t        strides[2] = {16u, 8u};
  const SignatureEntry       outputs[2] = {{10u, 1u, 0u, 0xFu, 3u, 0u, 0u, 0u}, {10u, 0u, 1u, 0xFu, 3u, 0u, 0u, 0u}};
  D3D12_STREAM_OUTPUT_DESC   made = {};
  CHECK(builder.stream_output(declared, 2u, strides, 2u, 0xFFFFFFFFu, outputs, 2u, job, made));
  D3D12_STREAM_OUTPUT_DESC   unmatched = {};
  CHECK(!builder.stream_output(declared, 2u, strides, 2u, 0xFFFFFFFFu, nullptr, 0u, job, unmatched));
  CHECK_EQ(made.NumEntries, 2u);
  CHECK_EQ(made.NumStrides, 2u);
  CHECK_EQ(made.pBufferStrides[1], 8u);
  CHECK_EQ(made.RasterizedStream, static_cast<UINT>(D3D12_SO_NO_RASTERIZED_STREAM));
  CHECK_EQ(made.pSODeclaration[0].ComponentCount, 4u);
  CHECK_EQ(made.pSODeclaration[1].StartComponent, 1u);
  CHECK_EQ(made.pSODeclaration[1].ComponentCount, 2u);
  CHECK_EQ(made.pSODeclaration[1].OutputSlot, 1u);
  CHECK(made.pSODeclaration[0].SemanticName != nullptr);
  const StreamOutDeclaration user_declared[1] = {{0u, 0u, 2u, 0x7u}};
  const SignatureEntry       user_outputs[1] = {{10u, 0u, 2u, 0x7u, 3u, 0u, 0u, 0u}};
  D3D12_STREAM_OUTPUT_DESC   by_register = {};
  CHECK(builder.stream_output(user_declared, 1u, strides, 1u, 0xFFFFFFFFu, user_outputs, 1u, job, by_register));
  CHECK(std::strcmp(by_register.pSODeclaration[0].SemanticName, "$register") == 0);
  CHECK_EQ(by_register.pSODeclaration[0].SemanticIndex, 2u); }

TEST(d3d12_pipeline_builder, no_stream_output_declared_leaves_the_pipeline_without_one)
{ D3d12PipelineBuilder builder(objects);
  av::D3d12PipelineJob job;
  job.bind(objects_heap);
  D3D12_STREAM_OUTPUT_DESC made = {};
  made.NumEntries = 7u;
  CHECK(builder.stream_output(nullptr, 0u, nullptr, 0u, 0u, nullptr, 0u, job, made));
  CHECK_EQ(made.NumEntries, 0u);
  CHECK(made.pSODeclaration == nullptr); }

TEST(d3d12_pipeline_builder, a_stream_output_declaration_that_does_not_fit_is_refused)
{ D3d12PipelineBuilder builder(objects);
  av::D3d12PipelineJob job;
  job.bind(objects_heap);
  const StreamOutDeclaration declared[1] = {{0u, 0u, 0u, 0xFu}};
  const std::uint32_t        strides[5] = {16u, 16u, 16u, 16u, 16u};
  D3D12_STREAM_OUTPUT_DESC   made = {};
  CHECK(!builder.stream_output(nullptr, 1u, strides, 1u, 0u, nullptr, 0u, job, made));
  CHECK(!builder.stream_output(declared, av::D3d12PipelineJob::max_stream_outputs + 1u, strides, 1u, 0u, nullptr, 0u, job, made));
  CHECK(!builder.stream_output(declared, 1u, strides, 5u, 0u, nullptr, 0u, job, made));
  CHECK(!builder.stream_output(declared, 1u, nullptr, 1u, 0u, nullptr, 0u, job, made)); }

TEST(d3d12_pipeline_builder, a_stream_output_buffer_slot_or_stream_the_host_arrays_cannot_hold_is_refused)
{ D3d12PipelineBuilder builder(objects);
  av::D3d12PipelineJob job;
  job.bind(objects_heap);
  const std::uint32_t        strides[1] = {16u};
  const StreamOutDeclaration far_slot[1] = {{0u, 200u, 0u, 0xFu}};
  const StreamOutDeclaration far_stream[1] = {{9u, 0u, 0u, 0xFu}};
  const StreamOutDeclaration fine[1] = {{0u, 0u, 0u, 0xFu}};
  D3D12_STREAM_OUTPUT_DESC   made = {};
  CHECK(!builder.stream_output(far_slot, 1u, strides, 1u, 0xFFFFFFFFu, nullptr, 0u, job, made));
  CHECK(!builder.stream_output(far_stream, 1u, strides, 1u, 0xFFFFFFFFu, nullptr, 0u, job, made));
  CHECK(!builder.stream_output(fine, 1u, strides, 1u, 77u, nullptr, 0u, job, made));
  const SignatureEntry       outputs[1] = {{10u, 1u, 0u, 0xFu, 3u, 0u, 0u, 0u}};
  CHECK(builder.stream_output(fine, 1u, strides, 1u, 3u, outputs, 1u, job, made));
  CHECK_EQ(made.RasterizedStream, 3u); }

namespace {
GraphicsState plain_state()
{ GraphicsState state = {};
  state.sample_count = 1u;
  return state; }
}

TEST(d3d12_pipeline_builder, depth_bounds_back_masks_and_view_instances_that_fit_are_accepted)
{ D3d12PipelineBuilder builder(objects);
  GraphicsState state = plain_state();
  state.depth_stencil.depth_bounds_enable = 1u;
  state.depth_stencil.back_read_mask = 0xFFu;
  state.depth_stencil.back_write_mask = 0x0Fu;
  state.view_instance_count = 4u;
  state.view_instancing_flags = 1u;
  state.view_instances[3] = {15u, 2047u};
  CHECK(builder.state_holds(state)); }

TEST(d3d12_pipeline_builder, depth_stencil_and_view_instancing_fields_past_their_range_are_refused)
{ D3d12PipelineBuilder builder(objects);
  GraphicsState state = plain_state();
  state.depth_stencil.depth_bounds_enable = 2u;
  CHECK(!builder.state_holds(state));
  state = plain_state();
  state.depth_stencil.back_read_mask = 0x100u;
  CHECK(!builder.state_holds(state));
  state = plain_state();
  state.depth_stencil.back_write_mask = 0x100u;
  CHECK(!builder.state_holds(state));
  state = plain_state();
  state.depth_stencil.stencil_read_mask = 0x1234u;
  CHECK(!builder.state_holds(state));
  state = plain_state();
  state.depth_stencil.back.func = 9u;
  CHECK(!builder.state_holds(state));
  state = plain_state();
  state.depth_stencil.front.pass = 9u;
  CHECK(!builder.state_holds(state));
  state = plain_state();
  state.depth_stencil.depth_func = 9u;
  CHECK(!builder.state_holds(state));
  state = plain_state();
  state.view_instance_count = 5u;
  CHECK(!builder.state_holds(state));
  state = plain_state();
  state.view_instance_count = 1u;
  state.view_instancing_flags = 2u;
  CHECK(!builder.state_holds(state));
  state = plain_state();
  state.view_instance_count = 1u;
  state.view_instances[0].viewport_index = 16u;
  CHECK(!builder.state_holds(state));
  state = plain_state();
  state.view_instance_count = 1u;
  state.view_instances[0].render_target_index = 2048u;
  CHECK(!builder.state_holds(state)); }

TEST(d3d12_pipeline_builder, a_pipeline_is_either_classic_or_mesh_and_an_amplification_shader_needs_a_mesh_shader)
{ D3d12PipelineBuilder builder(objects);
  std::uint64_t sizes[D3d12PipelineBuilder::stage_count] = {};
  CHECK(!builder.shape_holds(sizes, 0u));
  sizes[0] = 8u;
  CHECK(builder.shape_holds(sizes, 0u));
  sizes[3] = 8u;
  sizes[4] = 8u;
  CHECK(builder.shape_holds(sizes, 2u));
  sizes[6] = 8u;
  CHECK(!builder.shape_holds(sizes, 0u));
  sizes[6] = 0u;
  sizes[5] = 8u;
  CHECK(!builder.shape_holds(sizes, 0u));
  std::uint64_t mesh[D3d12PipelineBuilder::stage_count] = {};
  mesh[5] = 8u;
  CHECK(builder.shape_holds(mesh, 0u));
  mesh[6] = 8u;
  mesh[4] = 8u;
  CHECK(builder.shape_holds(mesh, 0u));
  CHECK(!builder.shape_holds(mesh, 1u));
  mesh[1] = 8u;
  CHECK(!builder.shape_holds(mesh, 0u));
  mesh[1] = 0u;
  mesh[3] = 8u;
  CHECK(!builder.shape_holds(mesh, 0u)); }
