/* One cl_mem over a Direct3D 11 subresource: which it is, what moves for it,
 * which ways its bytes cross, and whose turn it is. */
#include "harness/framework/test_registry.hpp"
#include "icd/opencl/d3d11_object.hpp"

#include <cstddef>

using av::clicd::D3d11Object;
using av::clicd::D3d11Resource;

namespace {

int mem = 0, other_mem = 0, resource = 0;

}  // namespace

TEST(d3d11_object, it_is_its_cl_mem_over_its_subresource)
{ const D3d11Resource texture(D3d11Resource::Kind::texture3d, 2u, 0u, 16u, 8u, 4u, 2u, 1u, 1u);
  const D3d11Object object(&mem, &resource, 1u, texture, CL_MEM_READ_WRITE);
  CHECK(object.is(&mem));
  CHECK(!object.is(&other_mem));
  CHECK(!object.is(nullptr));
  CHECK(object.over(&resource, 1u));
  CHECK(!object.over(&resource, 0u));
  CHECK(!object.over(&other_mem, 1u));
  CHECK(object.resource() == &resource);
  CHECK_EQ(object.subresource(), 1u);
  CHECK(object.described().kind() == D3d11Resource::Kind::texture3d);
  CHECK(!object.buffer());
  std::size_t region[3] = {};
  object.region(region);
  CHECK_EQ(region[0], 8u);
  CHECK_EQ(region[1], 4u);
  CHECK_EQ(region[2], 2u); }

TEST(d3d11_object, its_flags_say_which_ways_the_bytes_cross)
{ const D3d11Resource buffer(D3d11Resource::Kind::buffer, 0u, 256u, 0u, 0u, 0u, 1u, 1u, 1u);
  const D3d11Object both(&mem, &resource, 0u, buffer, CL_MEM_READ_WRITE);
  CHECK(both.buffer());
  CHECK_EQ(both.bytes(), 256u);
  CHECK(both.reads_resource());
  CHECK(both.writes_resource());
  /* No flags is read and write, as the API defaults it. */
  const D3d11Object unflagged(&mem, &resource, 0u, buffer, 0u);
  CHECK(unflagged.reads_resource());
  CHECK(unflagged.writes_resource());
  const D3d11Object reading(&mem, &resource, 0u, buffer, CL_MEM_READ_ONLY);
  CHECK(reading.reads_resource());
  CHECK(!reading.writes_resource());
  const D3d11Object writing(&mem, &resource, 0u, buffer, CL_MEM_WRITE_ONLY);
  CHECK(!writing.reads_resource());
  CHECK(writing.writes_resource()); }

TEST(d3d11_object, it_starts_as_direct3d_s_and_changes_hands)
{ const D3d11Resource buffer(D3d11Resource::Kind::buffer, 0u, 64u, 0u, 0u, 0u, 1u, 1u, 1u);
  D3d11Object object(&mem, &resource, 0u, buffer, CL_MEM_READ_WRITE);
  CHECK(!object.acquired());
  object.acquire();
  CHECK(object.acquired());
  object.release();
  CHECK(!object.acquired()); }
