#include "harness/framework/test_registry.hpp"
#include "icd/opencl/cl_mem_extent.hpp"
#include "icd/opencl/cl_mem_flags.hpp"

#include <cstddef>
#include <cstdint>

using av::clicd::ClMemExtent;
using av::clicd::ClMemFlags;

TEST(cl_mem_extent, a_buffer_holds_the_ranges_inside_it_and_none_that_is_empty_or_past_the_end)
{ ClMemExtent buffer;
  buffer.as_buffer(64u, 256u);
  CHECK(buffer.shape() == ClMemExtent::Shape::buffer);
  CHECK_EQ(buffer.base(), 64u);
  CHECK_EQ(buffer.bytes(), 256u);
  CHECK(buffer.holds_range(0u, 256u));
  CHECK(buffer.holds_range(255u, 1u));
  CHECK(!buffer.holds_range(255u, 2u));
  CHECK(!buffer.holds_range(0u, 0u));
  CHECK(!buffer.holds_range(257u, 1u));
  CHECK(!buffer.holds_range(~std::uint64_t{0}, 2u));
  ClMemExtent unknown;
  CHECK(unknown.holds_range(1u << 30u, 1u << 30u)); }

TEST(cl_mem_extent, an_image_judges_a_region_against_its_dimensions_by_type)
{ const cl_image_format rgba = {CL_RGBA, CL_UNORM_INT8};
  ClMemExtent flat;
  flat.as_image(CL_MEM_OBJECT_IMAGE2D, 16u, 8u, 0u, 0u, rgba);
  const std::size_t origin[3] = {0u, 0u, 0u};
  const std::size_t whole[3] = {16u, 8u, 1u};
  const std::size_t past[3] = {17u, 8u, 1u};
  const std::size_t deep[3] = {16u, 8u, 2u};
  const std::size_t empty[3] = {0u, 8u, 1u};
  CHECK_EQ(flat.judge_region(origin, whole), CL_SUCCESS);
  CHECK_EQ(flat.judge_region(origin, past), CL_INVALID_VALUE);
  CHECK_EQ(flat.judge_region(origin, deep), CL_INVALID_VALUE);
  CHECK_EQ(flat.judge_region(origin, empty), CL_INVALID_VALUE);
  CHECK_EQ(flat.judge_region(nullptr, whole), CL_INVALID_VALUE);
  const std::size_t edge[3] = {15u, 7u, 0u};
  const std::size_t one[3] = {1u, 1u, 1u};
  CHECK_EQ(flat.judge_region(edge, one), CL_SUCCESS);
  ClMemExtent array;
  array.as_image(CL_MEM_OBJECT_IMAGE1D_ARRAY, 8u, 0u, 0u, 4u, rgba);
  const std::size_t layers[3] = {8u, 4u, 1u};
  const std::size_t extra[3] = {8u, 5u, 1u};
  CHECK_EQ(array.judge_region(origin, layers), CL_SUCCESS);
  CHECK_EQ(array.judge_region(origin, extra), CL_INVALID_VALUE);
  ClMemExtent volume;
  volume.as_image(CL_MEM_OBJECT_IMAGE3D, 4u, 4u, 4u, 0u, rgba);
  const std::size_t cube[3] = {4u, 4u, 4u};
  CHECK_EQ(volume.judge_region(origin, cube), CL_SUCCESS);
  CHECK_EQ(volume.judge_region(edge, cube), CL_INVALID_VALUE); }

TEST(cl_mem_extent, two_images_compare_by_format_and_geometry)
{ const cl_image_format rgba = {CL_RGBA, CL_UNORM_INT8};
  const cl_image_format half = {CL_R, CL_HALF_FLOAT};
  ClMemExtent first, second, third;
  first.as_image(CL_MEM_OBJECT_IMAGE2D, 16u, 8u, 0u, 0u, rgba);
  second.as_image(CL_MEM_OBJECT_IMAGE2D, 16u, 8u, 0u, 0u, rgba);
  third.as_image(CL_MEM_OBJECT_IMAGE2D, 16u, 4u, 0u, 0u, half);
  CHECK(first.same_format(second));
  CHECK(first.same_geometry(second));
  CHECK(!first.same_format(third));
  CHECK(!first.same_geometry(third));
  CHECK_EQ(first.element_bytes(), 4u);
  CHECK_EQ(third.element_bytes(), 2u);
  const std::size_t one[3] = {0u, 0u, 0u};
  const std::size_t other[3] = {8u, 4u, 0u};
  const std::size_t region[3] = {4u, 2u, 1u};
  CHECK(!first.regions_overlap(one, second, other, region));
  const std::size_t near[3] = {2u, 1u, 0u};
  CHECK(first.regions_overlap(one, second, near, region)); }

TEST(cl_mem_flags, access_flags_that_contradict_each_other_are_refused)
{ CHECK_EQ(ClMemFlags(CL_MEM_READ_WRITE).judge(), CL_SUCCESS);
  CHECK_EQ(ClMemFlags(CL_MEM_READ_ONLY | CL_MEM_HOST_WRITE_ONLY).judge(), CL_SUCCESS);
  CHECK_EQ(ClMemFlags(CL_MEM_READ_WRITE | CL_MEM_READ_ONLY).judge(), CL_INVALID_VALUE);
  CHECK_EQ(ClMemFlags(CL_MEM_HOST_WRITE_ONLY | CL_MEM_HOST_READ_ONLY).judge(), CL_INVALID_VALUE);
  CHECK_EQ(ClMemFlags(CL_MEM_HOST_NO_ACCESS | CL_MEM_HOST_READ_ONLY).judge(), CL_INVALID_VALUE);
  CHECK_EQ(ClMemFlags(CL_MEM_USE_HOST_PTR | CL_MEM_ALLOC_HOST_PTR).judge(), CL_INVALID_VALUE);
  CHECK_EQ(ClMemFlags(CL_MEM_USE_HOST_PTR | CL_MEM_COPY_HOST_PTR).judge(), CL_INVALID_VALUE);
  CHECK_EQ(ClMemFlags(cl_mem_flags{1} << 62u).judge(), CL_INVALID_VALUE); }

TEST(cl_mem_flags, the_host_access_flags_stay_with_the_guest_and_gate_its_transfers)
{ const ClMemFlags read_only(CL_MEM_READ_WRITE | CL_MEM_HOST_READ_ONLY);
  CHECK_EQ(read_only.for_host(), static_cast<cl_mem_flags>(CL_MEM_READ_WRITE));
  CHECK_EQ(read_only.asked(), static_cast<cl_mem_flags>(CL_MEM_READ_WRITE | CL_MEM_HOST_READ_ONLY));
  CHECK(read_only.lets_host_read());
  CHECK(!read_only.lets_host_write());
  const ClMemFlags write_only(CL_MEM_HOST_WRITE_ONLY);
  CHECK(!write_only.lets_host_read());
  CHECK(write_only.lets_host_write());
  const ClMemFlags none(CL_MEM_HOST_NO_ACCESS);
  CHECK(!none.lets_host_read());
  CHECK(!none.lets_host_write());
  const ClMemFlags open(CL_MEM_READ_WRITE);
  CHECK(open.lets_host_read());
  CHECK(open.lets_host_write()); }
