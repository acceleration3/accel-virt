/* A DXGI format as OpenCL names the same bytes (DxgiClFormat): BGRA as
 * CL_BGRA, never as RGBA swapped; sRGB and what OpenCL has no order for not
 * at all. */
#include "executor/opencl/dxgi_cl_format.hpp"
#include "harness/framework/test_registry.hpp"

#include <CL/cl.h>
#include <cstdint>

using av::DxgiClFormat;

TEST(dxgi_cl_format, each_format_is_named_in_the_order_its_bytes_are)
{ struct Case {
      std::uint32_t dxgi, order, type;
  };
  const Case cases[] = {{87u, CL_BGRA, CL_UNORM_INT8},  {28u, CL_RGBA, CL_UNORM_INT8},  {10u, CL_RGBA, CL_HALF_FLOAT},
                        {2u, CL_RGBA, CL_FLOAT},        {11u, CL_RGBA, CL_UNORM_INT16}, {41u, CL_R, CL_FLOAT},
                        {54u, CL_R, CL_HALF_FLOAT},     {61u, CL_R, CL_UNORM_INT8},     {34u, CL_RG, CL_HALF_FLOAT},
                        {16u, CL_RG, CL_FLOAT},         {49u, CL_RG, CL_UNORM_INT8}};
  for (const Case &item : cases) {
      const DxgiClFormat format(item.dxgi);
      CHECK(format.known());
      CHECK_EQ(format.order(), item.order);
      CHECK_EQ(format.type(), item.type);
  } }

TEST(dxgi_cl_format, srgb_and_formats_opencl_has_no_order_for_are_unknown)
{ const std::uint32_t unknown[] = {0u, 29u, 91u, 24u, 6u, 45u};
  for (const std::uint32_t dxgi : unknown) {
      const DxgiClFormat format(dxgi);
      CHECK(!format.known());
      CHECK_EQ(format.order(), 0u);
      CHECK_EQ(format.type(), 0u);
  } }
