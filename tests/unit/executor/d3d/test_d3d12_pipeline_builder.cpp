/* Root signatures and pipelines from the wire's descriptions. Making one needs
 * vkd3d-proton on the host GPU (test_d3d12_session_device.cpp); what any host
 * judges is that a builder with no device, or with a device vkd3d-proton never
 * opened, or naming a root signature it never made, makes nothing. */
#include "executor/d3d/d3d12_pipeline_builder.hpp"
#include "executor/d3d/d3d_object_table.hpp"
#include "executor/shim/d3d12_host_device.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/wire/wire_arena.hpp"

#include "fixtures/d3d12/fill_cs.hpp"

#include <cstdint>

using av::D3d12PipelineBuilder;
using namespace av::d3d12proto;

namespace {

/* Most of a megabyte: kept out of the stack. */
av::D3dObjectTable objects;
std::uint8_t       scratch_block[65536];

}  // namespace

TEST(d3d12_pipeline_builder, an_unbound_builder_makes_nothing)
{ D3d12PipelineBuilder builder(objects);
  av::WireArena scratch(scratch_block, sizeof scratch_block);
  CHECK(builder.root_signature(0u, nullptr, 0u, nullptr, 0u, nullptr, 0u) == nullptr);
  const std::uint8_t *code[D3d12PipelineBuilder::stage_count] = {};
  const std::uint64_t sizes[D3d12PipelineBuilder::stage_count] = {};
  CHECK(builder.graphics(1u, GraphicsState{}, nullptr, 0u, nullptr, 0u, code, sizes, scratch) == nullptr);
  CHECK(builder.compute(1u, reinterpret_cast<const std::uint8_t *>(fill_cs::program), fill_cs::program_bytes,
                        scratch) == nullptr); }

TEST(d3d12_pipeline_builder, a_device_vkd3d_proton_never_opened_or_a_root_it_never_made_makes_nothing)
{ D3d12PipelineBuilder builder(objects);
  av::D3d12HostDevice device;
  builder.bind(&device);
  av::WireArena scratch(scratch_block, sizeof scratch_block);
  CHECK(builder.root_signature(0u, nullptr, 0u, nullptr, 0u, nullptr, 0u) == nullptr);
  CHECK(builder.compute(1u, reinterpret_cast<const std::uint8_t *>(fill_cs::program), fill_cs::program_bytes,
                        scratch) == nullptr);
  const std::uint8_t *code[D3d12PipelineBuilder::stage_count] = {};
  const std::uint64_t sizes[D3d12PipelineBuilder::stage_count] = {};
  CHECK(builder.graphics(1u, GraphicsState{}, nullptr, 0u, nullptr, 0u, code, sizes, scratch) == nullptr);
  builder.bind(nullptr); }
