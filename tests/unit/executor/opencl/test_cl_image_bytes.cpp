#include "executor/opencl/cl_image_bytes.hpp"
#include "harness/framework/test_registry.hpp"

#include <CL/cl.h>

#include <cstdint>

using av::ClImageBytes;

namespace {

cl_image_desc plane(std::size_t width, std::size_t height)
{ cl_image_desc desc = {};
  desc.image_type = CL_MEM_OBJECT_IMAGE2D;
  desc.image_width = width;
  desc.image_height = height;
  return desc; }

cl_image_format format(cl_channel_order order, cl_channel_type type)
{ cl_image_format made = {};
  made.image_channel_order = order;
  made.image_channel_data_type = type;
  return made; }

}

TEST(cl_image_bytes, an_rgba_float_plane_is_sixteen_bytes_a_texel)
{ const cl_image_format rgba = format(CL_RGBA, CL_FLOAT);
  const cl_image_desc   desc = plane(64u, 32u);
  CHECK_EQ(ClImageBytes(&rgba, &desc).bytes(), 64u * 32u * 16u); }

TEST(cl_image_bytes, channel_count_and_width_decide_the_texel)
{ const cl_image_desc desc = plane(10u, 10u);
  const cl_image_format r8 = format(CL_R, CL_UNORM_INT8);
  const cl_image_format rg16 = format(CL_RG, CL_HALF_FLOAT);
  const cl_image_format rgb8 = format(CL_RGB, CL_UNORM_INT8);
  const cl_image_format bgra8 = format(CL_BGRA, CL_UNORM_INT8);
  CHECK_EQ(ClImageBytes(&r8, &desc).bytes(), 100u);
  CHECK_EQ(ClImageBytes(&rg16, &desc).bytes(), 400u);
  CHECK_EQ(ClImageBytes(&rgb8, &desc).bytes(), 300u);
  CHECK_EQ(ClImageBytes(&bgra8, &desc).bytes(), 400u); }

TEST(cl_image_bytes, packed_types_are_one_texel_whatever_the_order_says)
{ const cl_image_desc desc = plane(8u, 8u);
  const cl_image_format r565 = format(CL_RGB, CL_UNORM_SHORT_565);
  const cl_image_format r101010 = format(CL_RGB, CL_UNORM_INT_101010);
  CHECK_EQ(ClImageBytes(&r565, &desc).bytes(), 128u);
  CHECK_EQ(ClImageBytes(&r101010, &desc).bytes(), 256u); }

TEST(cl_image_bytes, depth_layers_and_mip_levels_multiply_in)
{ const cl_image_format r8 = format(CL_R, CL_UNORM_INT8);
  cl_image_desc volume = plane(4u, 4u);
  volume.image_type = CL_MEM_OBJECT_IMAGE3D;
  volume.image_depth = 4u;
  CHECK_EQ(ClImageBytes(&r8, &volume).bytes(), 64u);
  cl_image_desc layers = plane(4u, 4u);
  layers.image_array_size = 3u;
  CHECK_EQ(ClImageBytes(&r8, &layers).bytes(), 48u);
  cl_image_desc mips = plane(8u, 8u);
  mips.num_mip_levels = 4u;
  CHECK_EQ(ClImageBytes(&r8, &mips).bytes(), 64u + 21u); }

TEST(cl_image_bytes, a_one_dimensional_image_has_no_height_to_count)
{ const cl_image_format r8 = format(CL_R, CL_UNORM_INT8);
  cl_image_desc line = {};
  line.image_type = CL_MEM_OBJECT_IMAGE1D;
  line.image_width = 100u;
  CHECK_EQ(ClImageBytes(&r8, &line).bytes(), 100u); }

TEST(cl_image_bytes, an_image_over_a_buffer_adds_no_memory_of_its_own)
{ const cl_image_format r8 = format(CL_R, CL_UNORM_INT8);
  cl_image_desc over = plane(16u, 16u);
  over.mem_object = reinterpret_cast<cl_mem>(std::uintptr_t{0x1000});
  CHECK_EQ(ClImageBytes(&r8, &over).bytes(), 0u); }

TEST(cl_image_bytes, a_missing_description_is_zero_and_a_missing_format_is_four_bytes)
{ const cl_image_desc desc = plane(2u, 2u);
  CHECK_EQ(ClImageBytes(nullptr, nullptr).bytes(), 0u);
  CHECK_EQ(ClImageBytes(nullptr, &desc).bytes(), 16u); }
