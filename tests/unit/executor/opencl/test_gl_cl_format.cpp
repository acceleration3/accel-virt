#include "executor/opencl/gl_cl_format.hpp"
#include "harness/framework/test_registry.hpp"

#include <CL/cl.h>
#include <cstdint>

using av::GlClFormat;

namespace {

constexpr std::uint32_t gl_byte = 0x1400u, gl_unsigned_byte = 0x1401u, gl_short = 0x1402u,
                        gl_unsigned_short = 0x1403u, gl_int = 0x1404u, gl_unsigned_int = 0x1405u,
                        gl_float = 0x1406u, gl_half_float = 0x140Bu;
constexpr std::uint32_t gl_red = 0x1903u, gl_rg = 0x8227u, gl_rgba = 0x1908u, gl_red_integer = 0x8D94u,
                        gl_rg_integer = 0x8228u, gl_rgba_integer = 0x8D99u;

struct Case {
    std::uint32_t internal, order, type, format, transfer, bytes;
};

}

TEST(gl_cl_format, every_format_of_the_extension_s_table_is_named_and_moved_as_its_texels_are)
{ const Case cases[] = {
      {0x8058u, CL_RGBA, CL_UNORM_INT8, gl_rgba, gl_unsigned_byte, 4u},
      {0x1908u, CL_RGBA, CL_UNORM_INT8, gl_rgba, gl_unsigned_byte, 4u},
      {0x8C43u, CL_sRGBA, CL_UNORM_INT8, gl_rgba, gl_unsigned_byte, 4u},
      {0x8F97u, CL_RGBA, CL_SNORM_INT8, gl_rgba, gl_byte, 4u},
      {0x805Bu, CL_RGBA, CL_UNORM_INT16, gl_rgba, gl_unsigned_short, 8u},
      {0x8F9Bu, CL_RGBA, CL_SNORM_INT16, gl_rgba, gl_short, 8u},
      {0x881Au, CL_RGBA, CL_HALF_FLOAT, gl_rgba, gl_half_float, 8u},
      {0x8814u, CL_RGBA, CL_FLOAT, gl_rgba, gl_float, 16u},
      {0x8D8Eu, CL_RGBA, CL_SIGNED_INT8, gl_rgba_integer, gl_byte, 4u},
      {0x8D88u, CL_RGBA, CL_SIGNED_INT16, gl_rgba_integer, gl_short, 8u},
      {0x8D82u, CL_RGBA, CL_SIGNED_INT32, gl_rgba_integer, gl_int, 16u},
      {0x8D7Cu, CL_RGBA, CL_UNSIGNED_INT8, gl_rgba_integer, gl_unsigned_byte, 4u},
      {0x8D76u, CL_RGBA, CL_UNSIGNED_INT16, gl_rgba_integer, gl_unsigned_short, 8u},
      {0x8D70u, CL_RGBA, CL_UNSIGNED_INT32, gl_rgba_integer, gl_unsigned_int, 16u},
      {0x8227u, CL_RG, CL_UNORM_INT8, gl_rg, gl_unsigned_byte, 2u},
      {0x822Bu, CL_RG, CL_UNORM_INT8, gl_rg, gl_unsigned_byte, 2u},
      {0x8F95u, CL_RG, CL_SNORM_INT8, gl_rg, gl_byte, 2u},
      {0x822Cu, CL_RG, CL_UNORM_INT16, gl_rg, gl_unsigned_short, 4u},
      {0x8F99u, CL_RG, CL_SNORM_INT16, gl_rg, gl_short, 4u},
      {0x822Fu, CL_RG, CL_HALF_FLOAT, gl_rg, gl_half_float, 4u},
      {0x8230u, CL_RG, CL_FLOAT, gl_rg, gl_float, 8u},
      {0x8237u, CL_RG, CL_SIGNED_INT8, gl_rg_integer, gl_byte, 2u},
      {0x8239u, CL_RG, CL_SIGNED_INT16, gl_rg_integer, gl_short, 4u},
      {0x823Bu, CL_RG, CL_SIGNED_INT32, gl_rg_integer, gl_int, 8u},
      {0x8238u, CL_RG, CL_UNSIGNED_INT8, gl_rg_integer, gl_unsigned_byte, 2u},
      {0x823Au, CL_RG, CL_UNSIGNED_INT16, gl_rg_integer, gl_unsigned_short, 4u},
      {0x823Cu, CL_RG, CL_UNSIGNED_INT32, gl_rg_integer, gl_unsigned_int, 8u},
      {0x1903u, CL_R, CL_UNORM_INT8, gl_red, gl_unsigned_byte, 1u},
      {0x8229u, CL_R, CL_UNORM_INT8, gl_red, gl_unsigned_byte, 1u},
      {0x8F94u, CL_R, CL_SNORM_INT8, gl_red, gl_byte, 1u},
      {0x822Au, CL_R, CL_UNORM_INT16, gl_red, gl_unsigned_short, 2u},
      {0x8F98u, CL_R, CL_SNORM_INT16, gl_red, gl_short, 2u},
      {0x822Du, CL_R, CL_HALF_FLOAT, gl_red, gl_half_float, 2u},
      {0x822Eu, CL_R, CL_FLOAT, gl_red, gl_float, 4u},
      {0x8231u, CL_R, CL_SIGNED_INT8, gl_red_integer, gl_byte, 1u},
      {0x8233u, CL_R, CL_SIGNED_INT16, gl_red_integer, gl_short, 2u},
      {0x8235u, CL_R, CL_SIGNED_INT32, gl_red_integer, gl_int, 4u},
      {0x8232u, CL_R, CL_UNSIGNED_INT8, gl_red_integer, gl_unsigned_byte, 1u},
      {0x8234u, CL_R, CL_UNSIGNED_INT16, gl_red_integer, gl_unsigned_short, 2u},
      {0x8236u, CL_R, CL_UNSIGNED_INT32, gl_red_integer, gl_unsigned_int, 4u},
  };
  for (const Case &item : cases) {
      const GlClFormat format(item.internal);
      CHECK(format.known());
      CHECK_EQ(format.order(), item.order);
      CHECK_EQ(format.type(), item.type);
      CHECK_EQ(format.transfer_format(), item.format);
      CHECK_EQ(format.transfer_type(), item.transfer);
      CHECK_EQ(format.texel_bytes(), item.bytes);
  } }

TEST(gl_cl_format, three_channels_depth_and_compressed_formats_are_unknown)
{ const std::uint32_t unknown[] = {0u, 0x1907u , 0x8051u , 0x8C41u ,
                                   0x81A5u , 0x8CACu ,
                                   0x88F0u , 0x83F1u ,
                                   0x8059u , 0xFFFFFFFFu};
  for (const std::uint32_t internal : unknown) {
      const GlClFormat format(internal);
      CHECK(!format.known());
      CHECK_EQ(format.order(), 0u);
      CHECK_EQ(format.type(), 0u);
      CHECK_EQ(format.transfer_format(), 0u);
      CHECK_EQ(format.transfer_type(), 0u);
      CHECK_EQ(format.texel_bytes(), 0u);
  } }
