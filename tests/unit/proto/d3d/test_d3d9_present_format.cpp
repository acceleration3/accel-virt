#include "harness/framework/test_registry.hpp"
#include "proto/d3d/d3d9_present_format.hpp"

#include <cstdint>

using av::D3d9PresentFormat;

namespace {

constexpr std::uint32_t ddi_a8r8g8b8 = 21u, ddi_x8r8g8b8 = 22u, ddi_a2b10g10r10 = 31u, ddi_a8b8g8r8 = 32u,
                        ddi_a16b16g16r16f = 113u, ddi_r5g6b5 = 23u;
constexpr std::uint32_t dxgi_b8g8r8a8_unorm = 87u, dxgi_b8g8r8x8_unorm = 88u, dxgi_r10g10b10a2_unorm = 24u,
                        dxgi_r16g16b16a16_float = 10u, dxgi_r8g8b8a8_unorm = 28u;

}

TEST(d3d9_present_format, a_direct3d_9_format_round_trips)
{ const std::uint32_t formats[] = {ddi_a8r8g8b8, ddi_x8r8g8b8, ddi_a2b10g10r10, ddi_a16b16g16r16f, ddi_a8b8g8r8};
  const std::uint32_t dxgi[] = {dxgi_b8g8r8a8_unorm, dxgi_b8g8r8x8_unorm, dxgi_r10g10b10a2_unorm,
                                dxgi_r16g16b16a16_float, dxgi_r8g8b8a8_unorm};
  for (std::uint32_t at = 0u; at < 5u; at++) {
      D3d9PresentFormat format(0u);
      CHECK(format.take_ddi(formats[at]));
      CHECK_EQ(format.dxgi(), dxgi[at]);
      CHECK_EQ(format.ddi(), formats[at]);
  } }

TEST(d3d9_present_format, a_format_the_kernel_cannot_present_is_refused)
{ D3d9PresentFormat format(dxgi_b8g8r8a8_unorm);
  CHECK(!format.take_ddi(ddi_r5g6b5));
  CHECK(!format.take_ddi(0u));
  CHECK(!format.take_ddi(0xFFFFFFFFu));
  CHECK_EQ(format.dxgi(), dxgi_b8g8r8a8_unorm); }

TEST(d3d9_present_format, every_dxgi_variant_scans_out_as_its_family)
{ CHECK_EQ(D3d9PresentFormat(91u).ddi(), ddi_a8r8g8b8);
  CHECK_EQ(D3d9PresentFormat(90u).ddi(), ddi_a8r8g8b8);
  CHECK_EQ(D3d9PresentFormat(93u).ddi(), ddi_x8r8g8b8);
  CHECK_EQ(D3d9PresentFormat(92u).ddi(), ddi_x8r8g8b8);
  CHECK_EQ(D3d9PresentFormat(23u).ddi(), ddi_a2b10g10r10);
  CHECK_EQ(D3d9PresentFormat(dxgi_r8g8b8a8_unorm).ddi(), ddi_a8b8g8r8);
  CHECK_EQ(D3d9PresentFormat(41u).ddi(), ddi_a8b8g8r8);
  CHECK_EQ(D3d9PresentFormat(500u).ddi(), ddi_a8b8g8r8); }

TEST(d3d9_present_format, video_is_named_by_its_fourcc_both_ways)
{ constexpr std::uint32_t ddi_nv12 = 0x3231564Eu, ddi_yuy2 = 0x32595559u;
  CHECK_EQ(D3d9PresentFormat(103u).ddi(), ddi_nv12);
  CHECK_EQ(D3d9PresentFormat(107u).ddi(), ddi_yuy2);
  D3d9PresentFormat format(0u);
  CHECK(format.take_ddi(ddi_nv12));
  CHECK_EQ(format.dxgi(), 103u);
  CHECK(format.take_ddi(ddi_yuy2));
  CHECK_EQ(format.dxgi(), 107u); }

TEST(d3d9_present_format, an_x8_surface_presents_as_x8_and_is_opened_as_bgra)
{ D3d9PresentFormat x8(0u);
  CHECK(x8.take_ddi(ddi_x8r8g8b8));
  CHECK_EQ(x8.dxgi(), dxgi_b8g8r8x8_unorm);
  CHECK_EQ(x8.viewed(), dxgi_b8g8r8a8_unorm);
  D3d9PresentFormat a8(0u);
  CHECK(a8.take_ddi(ddi_a8r8g8b8));
  CHECK_EQ(a8.viewed(), dxgi_b8g8r8a8_unorm);
  CHECK_EQ(D3d9PresentFormat(dxgi_r8g8b8a8_unorm).viewed(), dxgi_r8g8b8a8_unorm);
  CHECK_EQ(D3d9PresentFormat(dxgi_r10g10b10a2_unorm).viewed(), dxgi_r10g10b10a2_unorm); }
