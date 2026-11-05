#include "harness/framework/test_registry.hpp"
#include "miniport/memory/cross_adapter_format.hpp"
#include "miniport/memory/pixel_format.hpp"

using av::CrossAdapterFormat;

TEST(cross_adapter_format, sizes_the_ten_bit_and_float_formats_a_cross_adapter_surface_may_hold)
{ CHECK_EQ(CrossAdapterFormat(31u).bytes_per_pixel(), 4u);
  CHECK_EQ(CrossAdapterFormat(35u).bytes_per_pixel(), 4u);
  CHECK_EQ(CrossAdapterFormat(113u).bytes_per_pixel(), 8u);
  CHECK_EQ(CrossAdapterFormat(116u).bytes_per_pixel(), 16u); }

TEST(cross_adapter_format, sizes_every_other_format_as_the_pixel_format_does)
{ const unsigned formats[] = {87u, 28u, 2u, 10u, 54u};
  for (const unsigned format : formats)
      CHECK_EQ(CrossAdapterFormat(format).bytes_per_pixel(), av::PixelFormat(format).bytes_per_pixel()); }
