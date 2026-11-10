#include "executor/d3d/d3d12_indirect_signature.hpp"
#include "executor/d3d/d3d12_root_layout.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>

using av::D3d12IndirectSignature;
using av::D3d12RootLayout;
using av::d3d12proto::IndirectArgument;
using av::d3d12proto::RootParameter;

namespace {

enum : std::uint32_t {
    draw, draw_indexed, dispatch, vertex_buffer_view, index_buffer_view, constant, constant_buffer_view,
    shader_resource_view, unordered_access_view, dispatch_rays, dispatch_mesh, incrementing_constant
};

D3d12RootLayout rooted()
{
    RootParameter parameters[5] = {};
    parameters[0].type = 1u;
    parameters[0].values = 4u;
    parameters[1].type = 2u;
    parameters[2].type = 3u;
    parameters[3].type = 4u;
    parameters[4].type = 0u;
    return D3d12RootLayout(parameters, 5u);
}

bool holds(const IndirectArgument *arguments, std::uint32_t count, std::uint32_t stride,
           const D3d12RootLayout *root)
{
    return D3d12IndirectSignature(arguments, count, stride).holds(root);
}

}

TEST(d3d12_indirect_signature, the_shapes_applications_use_hold)
{ const D3d12RootLayout root = rooted();
  const IndirectArgument draws[] = {{draw, 0u, 0u, 0u}};
  CHECK(holds(draws, 1u, 16u, nullptr));
  const IndirectArgument indexed[] = {{draw_indexed, 0u, 0u, 0u}};
  CHECK(holds(indexed, 1u, 20u, nullptr));
  const IndirectArgument dispatches[] = {{dispatch, 0u, 0u, 0u}};
  CHECK(holds(dispatches, 1u, 12u, nullptr));
  const IndirectArgument with_constants[] = {{constant, 0u, 0u, 3u}, {draw, 0u, 0u, 0u}};
  CHECK(holds(with_constants, 2u, 28u, &root));
  CHECK(!holds(with_constants, 2u, 24u, &root));
  CHECK(holds(draws, 1u, 16u, &root)); }

TEST(d3d12_indirect_signature, an_argument_that_carries_a_gpu_address_holds_with_the_bytes_of_its_view)
{ const D3d12RootLayout root = rooted();
  const IndirectArgument vertex[] = {{vertex_buffer_view, 3u, 0u, 0u}, {draw, 0u, 0u, 0u}};
  CHECK(holds(vertex, 2u, 32u, &root));
  CHECK(!holds(vertex, 2u, 28u, &root));
  CHECK(D3d12IndirectSignature(vertex, 2u, 32u).addressed());
  const IndirectArgument indexed[] = {{index_buffer_view, 0u, 0u, 0u}, {draw_indexed, 0u, 0u, 0u}};
  CHECK(holds(indexed, 2u, 36u, nullptr));
  const IndirectArgument views[] = {{constant_buffer_view, 1u, 0u, 0u}, {shader_resource_view, 2u, 0u, 0u},
                                    {unordered_access_view, 3u, 0u, 0u}, {draw, 0u, 0u, 0u}};
  CHECK(holds(views, 4u, 40u, &root));
  const IndirectArgument draws[] = {{draw, 0u, 0u, 0u}};
  CHECK(!D3d12IndirectSignature(draws, 1u, 16u).addressed()); }

TEST(d3d12_indirect_signature, an_address_argument_off_its_slot_or_root_parameter_is_refused)
{ const D3d12RootLayout root = rooted();
  const IndirectArgument slot[] = {{vertex_buffer_view, 16u, 0u, 0u}, {draw, 0u, 0u, 0u}};
  CHECK(!holds(slot, 2u, 32u, &root));
  const IndirectArgument wrong_type[] = {{shader_resource_view, 1u, 0u, 0u}, {draw, 0u, 0u, 0u}};
  CHECK(!holds(wrong_type, 2u, 24u, &root));
  const IndirectArgument no_root[] = {{constant_buffer_view, 1u, 0u, 0u}, {draw, 0u, 0u, 0u}};
  CHECK(!holds(no_root, 2u, 24u, nullptr));
  const IndirectArgument past[] = {{unordered_access_view, 9u, 0u, 0u}, {draw, 0u, 0u, 0u}};
  CHECK(!holds(past, 2u, 24u, &root)); }

TEST(d3d12_indirect_signature, a_stride_that_is_no_command_s_is_refused)
{ const IndirectArgument draws[] = {{draw, 0u, 0u, 0u}};
  CHECK(!holds(draws, 1u, 12u, nullptr));
  CHECK(!holds(draws, 1u, 18u, nullptr));
  CHECK(!holds(draws, 1u, 0u, nullptr));
  CHECK(holds(draws, 1u, D3d12IndirectSignature::max_stride, nullptr));
  CHECK(!holds(draws, 1u, D3d12IndirectSignature::max_stride + 4u, nullptr));
  CHECK(!holds(draws, 0u, 16u, nullptr));
  IndirectArgument many[D3d12IndirectSignature::max_arguments + 1u] = {};
  const D3d12RootLayout root = rooted();
  for (IndirectArgument &each : many) each = {constant, 0u, 0u, 1u};
  many[D3d12IndirectSignature::max_arguments - 1u] = {draw, 0u, 0u, 0u};
  CHECK(holds(many, D3d12IndirectSignature::max_arguments, 1024u, &root));
  many[D3d12IndirectSignature::max_arguments] = {draw, 0u, 0u, 0u};
  CHECK(!holds(many, D3d12IndirectSignature::max_arguments + 1u, 1024u, &root)); }

TEST(d3d12_indirect_signature, an_argument_type_the_driver_does_not_report_is_refused)
{ const D3d12RootLayout root = rooted();
  const std::uint32_t unreported[] = {dispatch_rays, incrementing_constant, 0xFFFFFFFFu};
  for (const std::uint32_t type : unreported) {
      const IndirectArgument arguments[] = {{type, 0u, 0u, 0u}};
      CHECK(!holds(arguments, 1u, 256u, &root));
  } }

TEST(d3d12_indirect_signature, a_root_argument_that_names_no_parameter_of_its_kind_is_refused)
{ const D3d12RootLayout root = rooted();
  const IndirectArgument without_root[] = {{constant, 0u, 0u, 1u}, {draw, 0u, 0u, 0u}};
  CHECK(!holds(without_root, 2u, 20u, nullptr));
  const IndirectArgument past_last[] = {{constant_buffer_view, 5u, 0u, 0u}, {draw, 0u, 0u, 0u}};
  CHECK(!holds(past_last, 2u, 24u, &root));
  const IndirectArgument wrong_kind[] = {{shader_resource_view, 1u, 0u, 0u}, {draw, 0u, 0u, 0u}};
  CHECK(!holds(wrong_kind, 2u, 24u, &root));
  const IndirectArgument table[] = {{unordered_access_view, 4u, 0u, 0u}, {draw, 0u, 0u, 0u}};
  CHECK(!holds(table, 2u, 24u, &root));
  const IndirectArgument descriptor_as_constants[] = {{constant, 1u, 0u, 1u}, {draw, 0u, 0u, 0u}};
  CHECK(!holds(descriptor_as_constants, 2u, 20u, &root));
  const IndirectArgument past_width[] = {{constant, 0u, 2u, 3u}, {draw, 0u, 0u, 0u}};
  CHECK(!holds(past_width, 2u, 28u, &root));
  const IndirectArgument wrapping[] = {{constant, 0u, 0u, 0x40000000u}, {draw, 0u, 0u, 0u}};
  CHECK(!holds(wrapping, 2u, 20u, &root));
  const IndirectArgument slot[] = {{vertex_buffer_view, 32u, 0u, 0u}, {draw, 0u, 0u, 0u}};
  CHECK(!holds(slot, 2u, 32u, &root)); }

TEST(d3d12_indirect_signature, a_mesh_dispatch_is_three_words_and_ray_dispatch_or_incrementing_constants_are_refused)
{ const IndirectArgument meshes[] = {{dispatch_mesh, 0u, 0u, 0u}};
  CHECK(!holds(meshes, 1u, 12u, nullptr));
  CHECK(D3d12IndirectSignature(meshes, 1u, 12u).holds(nullptr, true));
  CHECK(!holds(meshes, 1u, 8u, nullptr));
  const IndirectArgument rays[] = {{dispatch_rays, 0u, 0u, 0u}};
  CHECK(!holds(rays, 1u, 64u, nullptr));
  const IndirectArgument counting[] = {{incrementing_constant, 0u, 0u, 0u}};
  CHECK(!holds(counting, 1u, 64u, nullptr)); }

TEST(d3d12_indirect_signature, an_action_that_is_not_the_last_argument_is_refused)
{ const IndirectArgument misplaced[] = {{draw, 0u, 0u, 0u}, {dispatch, 0u, 0u, 0u}};
  CHECK(!holds(misplaced, 2u, 64u, nullptr)); }
