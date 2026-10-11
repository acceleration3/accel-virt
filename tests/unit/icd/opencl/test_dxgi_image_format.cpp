/* The extension's table from DXGI_FORMAT to cl_image_format: what an image
 * over a texture is made in, and the formats it refuses. */
#include "harness/framework/test_registry.hpp"
#include "icd/opencl/dxgi_image_format.hpp"

#include <initializer_list>

using av::clicd::DxgiImageFormat;

TEST(dxgi_image_format, the_extension_s_table_and_the_orders_khronos_added)
{ const DxgiImageFormat half(10u);   /* R16G16B16A16_FLOAT */
  CHECK(half.mapped());
  CHECK_EQ(half.format().image_channel_order, static_cast<cl_channel_order>(CL_RGBA));
  CHECK_EQ(half.format().image_channel_data_type, static_cast<cl_channel_type>(CL_HALF_FLOAT));
  const DxgiImageFormat single(41u);   /* R32_FLOAT */
  CHECK_EQ(single.format().image_channel_order, static_cast<cl_channel_order>(CL_R));
  CHECK_EQ(single.format().image_channel_data_type, static_cast<cl_channel_type>(CL_FLOAT));
  const DxgiImageFormat bgra(87u);   /* B8G8R8A8_UNORM */
  CHECK_EQ(bgra.format().image_channel_order, static_cast<cl_channel_order>(CL_BGRA));
  const DxgiImageFormat srgb(29u);   /* R8G8B8A8_UNORM_SRGB */
  CHECK_EQ(srgb.format().image_channel_order, static_cast<cl_channel_order>(CL_sRGBA));
  const DxgiImageFormat last(91u);   /* B8G8R8A8_UNORM_SRGB, the table's last row */
  CHECK(last.mapped()); }

TEST(dxgi_image_format, a_typeless_compressed_or_depth_format_maps_to_nothing)
{ for (const unsigned dxgi : {0u, 1u, 9u, 27u, 40u, 45u, 71u, 98u, 200u}) {
      const DxgiImageFormat refused(dxgi);
      CHECK(!refused.mapped());
      CHECK_EQ(refused.format().image_channel_order, 0u);
  } }
