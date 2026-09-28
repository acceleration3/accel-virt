/* A command signature's arguments judged before vkd3d-proton sees them: the
 * shapes applications use hold, and each thing vkd3d-proton would trust
 * without checking - an argument type this driver does not report, a root
 * argument naming a parameter that is not there or not of its kind, a vertex
 * buffer slot past the last, a constant count that could wrap - is refused,
 * as is a stride that is not whole words, too large, or too small for the
 * arguments. Making one takes a device, which the GPU tests have. */
#include "device/executor/d3d12_indirect_signature.hpp"
#include "device/executor/d3d12_root_layout.hpp"
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

/* Parameter 0 is four root constants, 1 a CBV, 2 an SRV, 3 a UAV, 4 a table. */
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

}  // namespace

/* What wgpu makes at device creation, and what it adds for a pipeline whose
 * shaders read the first vertex and instance: a draw alone, and the same with
 * root constants in front of it. */
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
  CHECK(!holds(with_constants, 2u, 24u, &root));   /* 12 bytes of constants and a 16-byte draw */
  const IndirectArgument views[] = {{vertex_buffer_view, 31u, 0u, 0u}, {index_buffer_view, 0u, 0u, 0u},
                                    {constant_buffer_view, 1u, 0u, 0u}, {shader_resource_view, 2u, 0u, 0u},
                                    {unordered_access_view, 3u, 0u, 0u}, {draw_indexed, 0u, 0u, 0u}};
  CHECK(holds(views, 6u, 16u + 16u + 8u + 8u + 8u + 20u, &root)); }

TEST(d3d12_indirect_signature, a_stride_that_is_no_command_s_is_refused)
{ const IndirectArgument draws[] = {{draw, 0u, 0u, 0u}};
  CHECK(!holds(draws, 1u, 12u, nullptr));   /* shorter than a draw */
  CHECK(!holds(draws, 1u, 18u, nullptr));   /* not whole words */
  CHECK(!holds(draws, 1u, 0u, nullptr));
  CHECK(holds(draws, 1u, D3d12IndirectSignature::max_stride, nullptr));
  CHECK(!holds(draws, 1u, D3d12IndirectSignature::max_stride + 4u, nullptr));
  CHECK(!holds(draws, 0u, 16u, nullptr));
  IndirectArgument many[D3d12IndirectSignature::max_arguments + 1u] = {};
  for (IndirectArgument &each : many) each = {index_buffer_view, 0u, 0u, 0u};
  many[D3d12IndirectSignature::max_arguments] = {draw, 0u, 0u, 0u};
  CHECK(!holds(many, D3d12IndirectSignature::max_arguments + 1u, 1024u, nullptr)); }

/* Rays, mesh shaders and incrementing constants are past the tier this
 * driver reports; a number that is no type at all is refused the same. */
TEST(d3d12_indirect_signature, an_argument_type_the_driver_does_not_report_is_refused)
{ const D3d12RootLayout root = rooted();
  const std::uint32_t unreported[] = {dispatch_rays, dispatch_mesh, incrementing_constant, 0xFFFFFFFFu};
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
  /* A count that would wrap the bytes it adds up to a small number. */
  const IndirectArgument wrapping[] = {{constant, 0u, 0u, 0x40000000u}, {draw, 0u, 0u, 0u}};
  CHECK(!holds(wrapping, 2u, 20u, &root));
  const IndirectArgument slot[] = {{vertex_buffer_view, 32u, 0u, 0u}, {draw, 0u, 0u, 0u}};
  CHECK(!holds(slot, 2u, 32u, &root)); }
