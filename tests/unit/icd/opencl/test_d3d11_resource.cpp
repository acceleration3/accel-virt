#include "harness/framework/test_registry.hpp"
#include "icd/opencl/d3d11_resource.hpp"

using av::clicd::D3d11Resource;

TEST(d3d11_resource, a_texture_array_s_subresources_are_mip_plus_slice_times_mips)
{
  const D3d11Resource texture(D3d11Resource::Kind::texture2d, 10u, 0u, 64u, 32u, 1u, 3u, 2u, 1u);
  CHECK(texture.kind() == D3d11Resource::Kind::texture2d);
  CHECK_EQ(texture.format(), 10u);
  CHECK(texture.holds(5u));
  CHECK(!texture.holds(6u));
  CHECK_EQ(texture.width_of(4u), 32u);
  CHECK_EQ(texture.height_of(4u), 16u);
  CHECK_EQ(texture.depth_of(4u), 1u);
  CHECK_EQ(texture.width_of(5u), 16u);
  CHECK(!texture.multisampled()); }

TEST(d3d11_resource, a_mip_level_is_never_less_than_one_texel)
{ const D3d11Resource tall(D3d11Resource::Kind::texture3d, 2u, 0u, 4u, 256u, 8u, 9u, 1u, 1u);
  CHECK(tall.holds(8u));
  CHECK(!tall.holds(9u));
  CHECK_EQ(tall.width_of(8u), 1u);
  CHECK_EQ(tall.height_of(8u), 1u);
  CHECK_EQ(tall.depth_of(3u), 1u);
  CHECK_EQ(tall.depth_of(1u), 4u);
  const D3d11Resource deep(D3d11Resource::Kind::texture3d, 2u, 0u, 4u, 4u, 4u, 40u, 1u, 1u);
  CHECK_EQ(deep.width_of(39u), 1u); }

TEST(d3d11_resource, a_buffer_has_one_subresource_and_its_bytes)
{ const D3d11Resource buffer(D3d11Resource::Kind::buffer, 0u, 4096u, 0u, 0u, 0u, 1u, 1u, 1u);
  CHECK(buffer.holds(0u));
  CHECK(!buffer.holds(1u));
  CHECK_EQ(buffer.bytes(), 4096u);
  const D3d11Resource none;
  CHECK(none.kind() == D3d11Resource::Kind::none);
  CHECK(!none.holds(0u));
  const D3d11Resource empty(D3d11Resource::Kind::texture2d, 10u, 0u, 4u, 4u, 1u, 0u, 1u, 1u);
  CHECK(!empty.holds(0u));
  CHECK_EQ(empty.width_of(0u), 4u);
  const D3d11Resource sampled(D3d11Resource::Kind::texture2d, 10u, 0u, 4u, 4u, 1u, 1u, 1u, 4u);
  CHECK(sampled.multisampled()); }
