#include "executor/d3d/d3d12_root_layout.hpp"
#include "harness/executor/private_data_object.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>

using av::D3d12RootLayout;
using av::PrivateDataObject;
using av::d3d12proto::RootParameter;

namespace {

constexpr std::uint32_t table = 0u, constants = 1u, cbv = 2u, srv = 3u, uav = 4u;

RootParameter parameter(std::uint32_t type, std::uint32_t values)
{
    RootParameter made = {};
    made.type = type;
    made.values = values;
    return made;
}

}

TEST(d3d12_root_layout, kept_on_a_signature_it_judges_each_parameter_by_its_kind)
{ const RootParameter parameters[] = {parameter(constants, 4u), parameter(cbv, 0u), parameter(table, 0u),
                                      parameter(srv, 0u), parameter(uav, 0u)};
  PrivateDataObject<ID3D12RootSignature> root;
  CHECK(D3d12RootLayout(parameters, 5u).attach(&root));
  D3d12RootLayout layout;
  CHECK(layout.read(&root));
  CHECK(layout.has_constants(0u, 0u, 4u));
  CHECK(layout.has_constants(0u, 3u, 1u));
  CHECK(!layout.has_constants(0u, 3u, 2u));
  CHECK(!layout.has_constants(0u, 0u, 0u));
  CHECK(!layout.has_constants(0u, 0xFFFFFFFFu, 2u));
  CHECK(!layout.has_constants(1u, 0u, 1u));
  CHECK(layout.has_descriptor(1u, cbv));
  CHECK(!layout.has_descriptor(1u, srv));
  CHECK(layout.has_descriptor(3u, srv));
  CHECK(layout.has_descriptor(4u, uav));
  CHECK(!layout.has_descriptor(2u, cbv));
  CHECK(!layout.has_descriptor(0u, constants));
  CHECK(!layout.has_descriptor(5u, cbv));
  CHECK(!layout.has_constants(5u, 0u, 1u)); }

TEST(d3d12_root_layout, a_new_layout_names_no_parameter)
{ const D3d12RootLayout layout;
  CHECK(!layout.has_constants(0u, 0u, 1u));
  CHECK(!layout.has_descriptor(0u, cbv)); }

TEST(d3d12_root_layout, parameters_past_the_most_are_cut)
{ RootParameter parameters[D3d12RootLayout::max_parameters + 1u] = {};
  for (RootParameter &each : parameters) each = parameter(cbv, 0u);
  PrivateDataObject<ID3D12RootSignature> root;
  CHECK(D3d12RootLayout(parameters, D3d12RootLayout::max_parameters + 1u).attach(&root));
  D3d12RootLayout layout;
  CHECK(layout.read(&root));
  CHECK(layout.has_descriptor(D3d12RootLayout::max_parameters - 1u, cbv));
  CHECK(!layout.has_descriptor(D3d12RootLayout::max_parameters, cbv)); }

TEST(d3d12_root_layout, a_signature_that_keeps_none_gives_none_back)
{ const RootParameter parameters[] = {parameter(cbv, 0u)};
  PrivateDataObject<ID3D12RootSignature> refusing;
  refusing.refuse();
  CHECK(!D3d12RootLayout(parameters, 1u).attach(&refusing));
  D3d12RootLayout layout;
  CHECK(!layout.read(&refusing));
  PrivateDataObject<ID3D12RootSignature> other;
  CHECK(D3d12RootLayout(parameters, 1u).attach(&other));
  other.resize(8u);
  CHECK(!layout.read(&other));
  CHECK(!layout.has_descriptor(0u, cbv)); }
