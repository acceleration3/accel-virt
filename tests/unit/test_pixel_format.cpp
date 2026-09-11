#include "../../src/core/pixel_format.hpp"
#include "../harness/test_registry.hpp"

using av::PixelFormat;

TEST(pixel_format_bytes_per_pixel, is_four_for_the_32_bpp_formats)
{ const unsigned formats[] = {21u, 22u, 32u, 33u};   /* A8R8G8B8, X8R8G8B8, A8B8G8R8, X8B8G8R8 */
  for (unsigned format : formats) CHECK_EQ(PixelFormat(format).bytes_per_pixel(), 4u); }

TEST(pixel_format_bytes_per_pixel, is_zero_for_anything_else)
{ const unsigned formats[] = {0u, 20u, 23u, 31u, 34u, 41u, 0xFFFFFFFFu};
  for (unsigned format : formats) CHECK_EQ(PixelFormat(format).bytes_per_pixel(), 0u); }
