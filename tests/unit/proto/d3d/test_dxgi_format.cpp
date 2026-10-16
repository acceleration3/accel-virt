/* The one DXGI table (DxgiFormat): each column checked against the headers
 * that define its values - dxgiformat.h's numbering, vulkan_core.h, CL/cl.h,
 * drm_fourcc.h - and the relations between rows every reader relies on: an
 * sRGB sibling is mutual, a family's members name a typeless head, and what
 * the guest's OpenCL driver names as an image the host makes as one or the
 * row says it does not. */
#include "core/support/four_cc.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/d3d/dxgi_format.hpp"

#include <CL/cl.h>
#include <cstdint>
#include <initializer_list>
#include <vulkan/vulkan_core.h>

using av::DxgiFormat;
using av::FourCc;

namespace {

constexpr std::uint32_t r8g8b8a8_typeless = 27u;
constexpr std::uint32_t r8g8b8a8_unorm = 28u;
constexpr std::uint32_t r8g8b8a8_unorm_srgb = 29u;
constexpr std::uint32_t b8g8r8a8_unorm = 87u;
constexpr std::uint32_t b8g8r8x8_unorm = 88u;
constexpr std::uint32_t b8g8r8a8_typeless = 90u;
constexpr std::uint32_t b8g8r8a8_unorm_srgb = 91u;
constexpr std::uint32_t b8g8r8x8_typeless = 92u;
constexpr std::uint32_t b8g8r8x8_unorm_srgb = 93u;
constexpr std::uint32_t a4b4g4r4_unorm = 191u;

}  // namespace

TEST(dxgi_format, every_value_up_to_a4b4g4r4_is_known_and_none_past_it)
{ CHECK(DxgiFormat(0u).known());
  CHECK(DxgiFormat(130u).known());   /* planar, no layout, but DXGI has it */
  CHECK(DxgiFormat(a4b4g4r4_unorm).known());
  CHECK_EQ(DxgiFormat::count, a4b4g4r4_unorm + 1u);
  for (const std::uint32_t past : {192u, 255u, 0xFFFFFFFFu}) {
      const DxgiFormat format(past);
      CHECK(!format.known());
      CHECK_EQ(format.value(), past);
      CHECK_EQ(format.element_bytes(), 0u);
      CHECK_EQ(format.element_width(), 1u);
      CHECK_EQ(format.family(), 0u);
      CHECK_EQ(format.resolved(), 0u);
      CHECK(!format.typeless() && !format.family_head() && !format.depth() && !format.srgb());
      CHECK_EQ(format.vk_format() | format.cl_order() | format.drm_fourcc() | format.ddi_format(), 0u);
  } }

TEST(dxgi_format, a_block_or_pair_element_is_wider_than_one_texel)
{ const DxgiFormat bc1(71u);
  CHECK_EQ(bc1.element_bytes(), 8u);
  CHECK_EQ(bc1.element_width(), 4u);
  CHECK_EQ(bc1.element_height(), 4u);
  const DxgiFormat yuy2(107u);
  CHECK_EQ(yuy2.element_bytes(), 4u);
  CHECK_EQ(yuy2.element_width(), 2u);
  CHECK_EQ(yuy2.element_height(), 1u);
  const DxgiFormat rgba(r8g8b8a8_unorm);
  CHECK_EQ(rgba.element_bytes(), 4u);
  CHECK_EQ(rgba.element_width(), 1u);
  CHECK_EQ(rgba.element_height(), 1u);
  CHECK_EQ(DxgiFormat(a4b4g4r4_unorm).element_bytes(), 2u); }

/* Each sRGB format names its linear sibling and that one names it back; the
 * BGRX pair too, which DXVK views as BGRA's. */
TEST(dxgi_format, an_srgb_sibling_is_mutual_and_exactly_one_of_the_pair_is_srgb)
{ CHECK_EQ(DxgiFormat(r8g8b8a8_unorm).sibling(), r8g8b8a8_unorm_srgb);
  CHECK_EQ(DxgiFormat(b8g8r8x8_unorm_srgb).sibling(), b8g8r8x8_unorm);
  CHECK_EQ(DxgiFormat(98u).sibling(), 99u);   /* BC7 */
  CHECK_EQ(DxgiFormat(r8g8b8a8_typeless).sibling(), 0u);
  std::uint32_t pairs = 0u;
  for (std::uint32_t value = 0u; value < DxgiFormat::count; value++) {
      const DxgiFormat format(value);
      if (format.sibling() == 0u) continue;
      const DxgiFormat sibling(format.sibling());
      CHECK_EQ(sibling.sibling(), value);
      CHECK(format.srgb() != sibling.srgb());
      CHECK_EQ(format.family(), sibling.family());
      pairs++;
  }
  /* R8G8B8A8, B8G8R8A8, B8G8R8X8, BC1, BC2, BC3 and BC7, both ways. */
  CHECK_EQ(pairs, 14u); }

/* A family's members name its typeless head, which is its own family; the
 * BGRA families are named member by member, outside DXGI's runs. */
TEST(dxgi_format, a_family_is_its_typeless_head_and_the_members_numbered_after_it)
{ CHECK_EQ(DxgiFormat(r8g8b8a8_unorm_srgb).family(), r8g8b8a8_typeless);
  CHECK_EQ(DxgiFormat(b8g8r8a8_unorm).family(), b8g8r8a8_typeless);
  CHECK_EQ(DxgiFormat(b8g8r8a8_unorm_srgb).family(), b8g8r8a8_typeless);
  CHECK_EQ(DxgiFormat(b8g8r8x8_unorm_srgb).family(), b8g8r8x8_typeless);
  CHECK_EQ(DxgiFormat(26u).family(), 26u);    /* R11G11B10_FLOAT: in no family */
  CHECK_EQ(DxgiFormat(21u).family(), 19u);    /* R32_FLOAT_X8X24_TYPELESS under R32G8X24 */
  CHECK(DxgiFormat(21u).typeless());
  CHECK(!DxgiFormat(21u).family_head());
  std::uint32_t heads = 0u;
  for (std::uint32_t value = 0u; value < DxgiFormat::count; value++) {
      const DxgiFormat format(value);
      const DxgiFormat head(format.family());
      CHECK_EQ(head.family(), format.family());
      CHECK(head.family_head() || format.family() == value);
      heads += format.family_head() ? 1u : 0u;
  }
  /* DXGI's 20 typeless runs and the two BGRA families. */
  CHECK_EQ(heads, 22u); }

TEST(dxgi_format, the_depth_formats_are_the_four_with_a_depth_stencil_view)
{ std::uint32_t found = 0u;
  for (std::uint32_t value = 0u; value < DxgiFormat::count; value++) {
      if (!DxgiFormat(value).depth()) continue;
      CHECK(value == 20u || value == 40u || value == 45u || value == 55u);
      found++;
  }
  CHECK_EQ(found, 4u); }

/* ResolveSubresource takes a typed format; Direct3D 9 on 12 makes its back
 * buffers in a typeless four-byte colour family. */
TEST(dxgi_format, a_typeless_colour_head_resolves_in_its_unorm_member_and_others_in_none)
{ CHECK_EQ(DxgiFormat(23u).resolved(), 24u);                  /* R10G10B10A2 */
  CHECK_EQ(DxgiFormat(r8g8b8a8_typeless).resolved(), r8g8b8a8_unorm);
  CHECK_EQ(DxgiFormat(b8g8r8a8_typeless).resolved(), b8g8r8a8_unorm);
  CHECK_EQ(DxgiFormat(b8g8r8x8_typeless).resolved(), b8g8r8x8_unorm);
  CHECK_EQ(DxgiFormat(b8g8r8a8_unorm).resolved(), b8g8r8a8_unorm);
  CHECK_EQ(DxgiFormat(r8g8b8a8_unorm_srgb).resolved(), r8g8b8a8_unorm_srgb);
  CHECK_EQ(DxgiFormat(b8g8r8a8_unorm_srgb).resolved(), b8g8r8a8_unorm_srgb);
  for (const std::uint32_t typeless : {0u, 1u, 21u, 22u, 33u, 39u, 44u, 46u, 47u, 97u})
      CHECK_EQ(DxgiFormat(typeless).resolved(), 0u); }

/* The D3DDDIFORMAT a presented surface is scanned out as (d3dukmdt.h), an
 * sRGB or typeless member as its family's. */
TEST(dxgi_format, a_presentable_family_names_its_d3dddiformat)
{ CHECK_EQ(DxgiFormat(b8g8r8a8_unorm).ddi_format(), 21u);
  CHECK_EQ(DxgiFormat(b8g8r8a8_typeless).ddi_format(), 21u);
  CHECK_EQ(DxgiFormat(b8g8r8a8_unorm_srgb).ddi_format(), 21u);
  CHECK_EQ(DxgiFormat(b8g8r8x8_unorm).ddi_format(), 22u);
  CHECK_EQ(DxgiFormat(b8g8r8x8_unorm_srgb).ddi_format(), 22u);
  CHECK_EQ(DxgiFormat(24u).ddi_format(), 31u);
  CHECK_EQ(DxgiFormat(10u).ddi_format(), 113u);
  CHECK_EQ(DxgiFormat(103u).ddi_format(), FourCc('N', 'V', '1', '2').value());
  CHECK_EQ(DxgiFormat(107u).ddi_format(), FourCc('Y', 'U', 'Y', '2').value());
  CHECK_EQ(DxgiFormat(r8g8b8a8_unorm).ddi_format(), 32u);   /* A8B8G8R8, a desktop mode */
  CHECK_EQ(DxgiFormat(r8g8b8a8_unorm_srgb).ddi_format(), 0u);
  CHECK_EQ(DxgiFormat(41u).ddi_format(), 0u); }

TEST(dxgi_format, a_shared_format_is_made_as_the_vulkan_format_dxvk_makes_it)
{ struct Case {
      std::uint32_t dxgi;
      VkFormat      format;
      VkFormat      sibling;
  };
  const Case cases[] = {
      {r8g8b8a8_unorm, VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_R8G8B8A8_SRGB},
      {r8g8b8a8_unorm_srgb, VK_FORMAT_R8G8B8A8_SRGB, VK_FORMAT_R8G8B8A8_UNORM},
      {b8g8r8a8_unorm, VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_B8G8R8A8_SRGB},
      {b8g8r8a8_unorm_srgb, VK_FORMAT_B8G8R8A8_SRGB, VK_FORMAT_B8G8R8A8_UNORM},
      {b8g8r8x8_unorm, VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_B8G8R8A8_SRGB},
      {b8g8r8x8_unorm_srgb, VK_FORMAT_B8G8R8A8_SRGB, VK_FORMAT_B8G8R8A8_UNORM},
      {24u, VK_FORMAT_A2B10G10R10_UNORM_PACK32, VK_FORMAT_UNDEFINED},
      {10u, VK_FORMAT_R16G16B16A16_SFLOAT, VK_FORMAT_UNDEFINED},
      {2u, VK_FORMAT_R32G32B32A32_SFLOAT, VK_FORMAT_UNDEFINED},
      {11u, VK_FORMAT_R16G16B16A16_UNORM, VK_FORMAT_UNDEFINED},
      {16u, VK_FORMAT_R32G32_SFLOAT, VK_FORMAT_UNDEFINED},
      {34u, VK_FORMAT_R16G16_SFLOAT, VK_FORMAT_UNDEFINED},
      {41u, VK_FORMAT_R32_SFLOAT, VK_FORMAT_UNDEFINED},
      {49u, VK_FORMAT_R8G8_UNORM, VK_FORMAT_UNDEFINED},
      {54u, VK_FORMAT_R16_SFLOAT, VK_FORMAT_UNDEFINED},
      {61u, VK_FORMAT_R8_UNORM, VK_FORMAT_UNDEFINED},
  };
  std::uint32_t shared = 0u;
  for (const Case &item : cases) {
      const DxgiFormat format(item.dxgi);
      CHECK_EQ(format.vk_format(), static_cast<std::uint32_t>(item.format));
      CHECK_EQ(format.vk_sibling(), static_cast<std::uint32_t>(item.sibling));
  }
  for (std::uint32_t value = 0u; value < DxgiFormat::count; value++)
      shared += DxgiFormat(value).vk_format() != 0u ? 1u : 0u;
  CHECK_EQ(shared, sizeof cases / sizeof cases[0]); }

/* cl_khr_d3d11_sharing's table, by DXGI's own numbers, with the orders
 * Khronos added later; a typeless, compressed or depth format maps to none. */
TEST(dxgi_format, opencl_names_each_format_as_the_extension_s_table_does)
{ struct Case {
      std::uint32_t dxgi, order, type;
  };
  const Case cases[] = {
      {2u, CL_RGBA, CL_FLOAT},          {3u, CL_RGBA, CL_UNSIGNED_INT32}, {14u, CL_RGBA, CL_SIGNED_INT16},
      {11u, CL_RGBA, CL_UNORM_INT16},   {28u, CL_RGBA, CL_UNORM_INT8},    {29u, CL_sRGBA, CL_UNORM_INT8},
      {31u, CL_RGBA, CL_SNORM_INT8},    {37u, CL_RG, CL_SNORM_INT16},     {41u, CL_R, CL_FLOAT},
      {54u, CL_R, CL_HALF_FLOAT},       {64u, CL_R, CL_SIGNED_INT8},      {65u, CL_A, CL_UNORM_INT8},
      {87u, CL_BGRA, CL_UNORM_INT8},    {91u, CL_sBGRA, CL_UNORM_INT8},
  };
  for (const Case &item : cases) {
      const DxgiFormat format(item.dxgi);
      CHECK_EQ(format.cl_order(), item.order);
      CHECK_EQ(format.cl_type(), item.type);
  }
  std::uint32_t named = 0u;
  for (std::uint32_t value = 0u; value < DxgiFormat::count; value++)
      named += DxgiFormat(value).cl_order() != 0u ? 1u : 0u;
  CHECK_EQ(named, 40u);
  for (const std::uint32_t refused : {0u, 1u, 9u, 27u, 40u, 45u, 71u, 98u, b8g8r8x8_unorm, 200u}) {
      CHECK_EQ(DxgiFormat(refused).cl_order(), 0u);
      CHECK_EQ(DxgiFormat(refused).cl_type(), 0u);
  } }

/* B-4: the guest named sRGB textures as OpenCL images, interop exported
 * them, and the host made a buffer over them - so a twin the guest took for
 * an image was bytes. Every format the guest names as an image and interop
 * shares is either one the host makes an image over, or marked as not one,
 * and the guest reads the same mark before it takes a twin. */
TEST(dxgi_format, a_format_the_cl_icd_maps_and_the_host_exports_is_viewed_as_an_image_or_marked_not)
{ std::uint32_t viewed = 0u;
  for (std::uint32_t value = 0u; value < DxgiFormat::count; value++) {
      const DxgiFormat format(value);
      if (format.cl_image_on_host()) {
          CHECK(format.cl_order() != 0u && format.vk_format() != 0u && !format.srgb());
          viewed++;
          continue;
      }
      CHECK(format.cl_order() == 0u || format.vk_format() == 0u || format.srgb());
  }
  /* The eleven the host made images over before the table was one. */
  for (const std::uint32_t dxgi : {2u, 10u, 11u, 16u, 28u, 34u, 41u, 49u, 54u, 61u, 87u})
      CHECK(DxgiFormat(dxgi).cl_image_on_host());
  CHECK_EQ(viewed, 11u);
  CHECK(!DxgiFormat(r8g8b8a8_unorm_srgb).cl_image_on_host());
  CHECK(!DxgiFormat(b8g8r8a8_unorm_srgb).cl_image_on_host()); }

/* A DRM format is the bytes in memory read as one little-endian word
 * (drm_fourcc.h); an sRGB format is its linear sibling's. */
TEST(dxgi_format, a_dma_buf_names_the_bytes_as_drm_does)
{ CHECK_EQ(DxgiFormat(b8g8r8a8_unorm).drm_fourcc(), FourCc('A', 'R', '2', '4').value());
  CHECK_EQ(DxgiFormat(b8g8r8a8_unorm_srgb).drm_fourcc(), FourCc('A', 'R', '2', '4').value());
  CHECK_EQ(DxgiFormat(b8g8r8x8_unorm).drm_fourcc(), FourCc('X', 'R', '2', '4').value());
  CHECK_EQ(DxgiFormat(b8g8r8x8_unorm_srgb).drm_fourcc(), FourCc('X', 'R', '2', '4').value());
  CHECK_EQ(DxgiFormat(r8g8b8a8_unorm).drm_fourcc(), FourCc('A', 'B', '2', '4').value());
  CHECK_EQ(DxgiFormat(r8g8b8a8_unorm_srgb).drm_fourcc(), FourCc('A', 'B', '2', '4').value());
  CHECK_EQ(DxgiFormat(24u).drm_fourcc(), FourCc('A', 'B', '3', '0').value());
  CHECK_EQ(DxgiFormat(10u).drm_fourcc(), FourCc('A', 'B', '4', 'H').value());
  CHECK_EQ(DxgiFormat(41u).drm_fourcc(), 0u);
  /* Every format with a DRM format is one interop exports. */
  for (std::uint32_t value = 0u; value < DxgiFormat::count; value++)
      if (DxgiFormat(value).drm_fourcc() != 0u) CHECK(DxgiFormat(value).vk_format() != 0u); }

/* What a scanout's image is copied into the display's dma-buf as: the 32-bit
 * UNORM colour formats, each one interop exports with a DRM format. */
TEST(dxgi_format, a_scanout_is_copied_out_in_its_32_bit_colour_formats)
{ std::uint32_t scanned = 0u;
  for (std::uint32_t value = 0u; value < DxgiFormat::count; value++) {
      const DxgiFormat format(value);
      if (!format.scanned_out()) continue;
      CHECK(value == b8g8r8a8_unorm || value == b8g8r8x8_unorm || value == r8g8b8a8_unorm);
      CHECK(format.vk_format() != 0u && format.drm_fourcc() != 0u);
      scanned++;
  }
  CHECK_EQ(scanned, 3u); }
