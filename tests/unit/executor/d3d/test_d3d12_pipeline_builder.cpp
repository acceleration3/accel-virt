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
  CHECK(!builder.graphics(2u, 1u, GraphicsState{}, nullptr, 0u, nullptr, 0u, code, sizes, job));
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
  CHECK(!builder.graphics(2u, 1u, GraphicsState{}, nullptr, 0u, nullptr, 0u, code, sizes, job));
  builder.bind(nullptr); }
