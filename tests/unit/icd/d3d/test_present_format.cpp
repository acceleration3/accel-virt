/* A presentable format both ways it is named (PresentFormat): the DXGI format
 * a surface is made in and the D3DDDIFORMAT it is scanned out as. */
#include "harness/framework/test_registry.hpp"
#include "icd/d3d/present_format.hpp"

#include <cstdint>

using av::d3d::PresentFormat;

namespace {

constexpr std::uint32_t ddi_a8r8g8b8 = 21u, ddi_x8r8g8b8 = 22u, ddi_a2b10g10r10 = 31u, ddi_a8b8g8r8 = 32u,
                        ddi_a16b16g16r16f = 113u, ddi_r5g6b5 = 23u;
constexpr std::uint32_t dxgi_b8g8r8a8_unorm = 87u, dxgi_b8g8r8x8_unorm = 88u, dxgi_r10g10b10a2_unorm = 24u,
                        dxgi_r16g16b16a16_float = 10u, dxgi_r8g8b8a8_unorm = 28u;

}  // namespace

/* Each Direct3D 9 back buffer format presents in its DXGI twin, and comes
 * back out as itself. */
TEST(present_format, a_direct3d_9_format_round_trips)
{ const std::uint32_t formats[] = {ddi_a8r8g8b8, ddi_x8r8g8b8, ddi_a2b10g10r10, ddi_a16b16g16r16f};
  const std::uint32_t dxgi[] = {dxgi_b8g8r8a8_unorm, dxgi_b8g8r8x8_unorm, dxgi_r10g10b10a2_unorm,
                                dxgi_r16g16b16a16_float};
  for (std::uint32_t at = 0u; at < 4u; at++) {
      PresentFormat format(0u);
      CHECK(format.take_ddi(formats[at]));
      CHECK_EQ(format.dxgi(), dxgi[at]);
      CHECK_EQ(format.ddi(), formats[at]);
  } }

TEST(present_format, a_format_the_kernel_cannot_present_is_refused)
{ PresentFormat format(dxgi_b8g8r8a8_unorm);
  CHECK(!format.take_ddi(ddi_r5g6b5));
  CHECK_EQ(format.dxgi(), dxgi_b8g8r8a8_unorm); }

/* sRGB and typeless twins scan out as their plain format; anything else as
 * A8B8G8R8. */
TEST(present_format, every_dxgi_variant_scans_out_as_its_family)
{ CHECK_EQ(PresentFormat(91u).ddi(), ddi_a8r8g8b8);
  CHECK_EQ(PresentFormat(90u).ddi(), ddi_a8r8g8b8);
  CHECK_EQ(PresentFormat(93u).ddi(), ddi_x8r8g8b8);
  CHECK_EQ(PresentFormat(92u).ddi(), ddi_x8r8g8b8);
  CHECK_EQ(PresentFormat(23u).ddi(), ddi_a2b10g10r10);
  CHECK_EQ(PresentFormat(dxgi_r8g8b8a8_unorm).ddi(), ddi_a8b8g8r8); }
