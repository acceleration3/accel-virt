/* The image a shared Direct3D 11 texture is made as: the formats interop
 * shares, mutable between linear and sRGB where the format has both, drawn and
 * sampled whatever the binds, over dma-buf memory laid out by a DRM format
 * modifier - linear with rows 256 bytes aligned, one of the GPU's tilings the
 * driver picks, or a layout another image was given - and nothing for a
 * texture interop does not share. */
#include "proto/vulkan/exportable_image_info.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>

using av::ExportableImageInfo;

namespace {

/* DRM_FORMAT_MOD_LINEAR. */
constexpr std::uint64_t linear_modifier = 0u;

/* The last structure of `info`'s chain, which lays the image out. */
const VkImageDrmFormatModifierExplicitCreateInfoEXT *layout_of(const VkImageCreateInfo &info)
{
    const auto *node = static_cast<const VkBaseInStructure *>(info.pNext);
    while (node != nullptr && node->pNext != nullptr) node = node->pNext;
    return reinterpret_cast<const VkImageDrmFormatModifierExplicitCreateInfoEXT *>(node);
}

constexpr std::uint32_t r8g8b8a8_unorm = 28u;
constexpr std::uint32_t b8g8r8a8_unorm_srgb = 91u;
constexpr std::uint32_t r16g16b16a16_float = 10u;
constexpr std::uint32_t r32_float = 41u;
constexpr std::uint32_t b8g8r8a8_unorm = 87u;
constexpr std::uint32_t r32g32b32a32_float = 2u;
constexpr std::uint32_t r8_unorm = 61u;
constexpr std::uint32_t bind_shader_resource = 0x8u;
constexpr std::uint32_t bind_render_target = 0x20u;
constexpr std::uint32_t bind_unordered_access = 0x80u;

/* The bytes between two rows of a shared texture `width` texels wide. */
VkDeviceSize row_pitch(std::uint32_t width, std::uint32_t format)
{
    const ExportableImageInfo shared(width, 1u, 1u, 1u, 1u, format, bind_shader_resource | bind_render_target);
    return shared.supported() ? layout_of(shared.info())->pPlaneLayouts[0].rowPitch : 0u;
}

}  // namespace

TEST(exportable_image_info, a_shared_colour_texture_is_an_exportable_image_mutable_to_its_srgb_sibling)
{ const ExportableImageInfo shared(640u, 360u, 1u, 1u, 1u, r8g8b8a8_unorm, bind_shader_resource | bind_render_target);
  CHECK(shared.supported());
  const VkImageCreateInfo &info = shared.info();
  CHECK_EQ(info.imageType, VK_IMAGE_TYPE_2D);
  CHECK_EQ(info.format, VK_FORMAT_R8G8B8A8_UNORM);
  CHECK_EQ(info.extent.width, 640u);
  CHECK_EQ(info.extent.height, 360u);
  /* Linear under an explicit modifier: a driver may export an image as a
   * dma-buf only so (RADV from Mesa 26.2 refuses VK_IMAGE_TILING_LINEAR). */
  CHECK_EQ(info.tiling, VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT);
  CHECK_EQ(info.flags, static_cast<VkImageCreateFlags>(VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT));
  const VkImageUsageFlags drawn = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
                                  VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  CHECK_EQ(info.usage & drawn, drawn);
  const auto *external = static_cast<const VkExternalMemoryImageCreateInfo *>(info.pNext);
  CHECK_EQ(external->sType, VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO);
  CHECK_EQ(external->handleTypes, static_cast<VkExternalMemoryHandleTypeFlags>(VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT));
  const auto *views = static_cast<const VkImageFormatListCreateInfo *>(external->pNext);
  CHECK_EQ(views->viewFormatCount, 2u);
  CHECK_EQ(views->pViewFormats[0], VK_FORMAT_R8G8B8A8_UNORM);
  CHECK_EQ(views->pViewFormats[1], VK_FORMAT_R8G8B8A8_SRGB);
  /* The layout is the description's, not the driver's choice: one linear
   * plane at the start of the memory, its rows a pitch apart this sets. */
  const auto *layout = static_cast<const VkImageDrmFormatModifierExplicitCreateInfoEXT *>(views->pNext);
  CHECK_EQ(layout->sType, VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_EXPLICIT_CREATE_INFO_EXT);
  CHECK_EQ(layout->drmFormatModifier, linear_modifier);
  CHECK_EQ(layout->drmFormatModifierPlaneCount, 1u);
  CHECK_EQ(layout->pPlaneLayouts[0].offset, 0u);
  CHECK_EQ(layout->pPlaneLayouts[0].rowPitch, 640u * 4u);
  /* What the extension requires of a single-layer 2D image's one plane. */
  CHECK_EQ(layout->pPlaneLayouts[0].size, 0u);
  CHECK_EQ(layout->pPlaneLayouts[0].arrayPitch, 0u);
  CHECK_EQ(layout->pPlaneLayouts[0].depthPitch, 0u);
  CHECK(layout->pNext == nullptr);

  /* An sRGB texture's sibling is its linear format; one sampled only is still
   * made drawable, since the opening API draws into it. */
  const ExportableImageInfo srgb(4u, 4u, 1u, 1u, 0u, b8g8r8a8_unorm_srgb, bind_shader_resource);
  CHECK(srgb.supported());
  CHECK_EQ(srgb.info().format, VK_FORMAT_B8G8R8A8_SRGB);
  CHECK((srgb.info().usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) != 0u);
  const auto *srgb_external = static_cast<const VkExternalMemoryImageCreateInfo *>(srgb.info().pNext);
  const auto *srgb_views = static_cast<const VkImageFormatListCreateInfo *>(srgb_external->pNext);
  CHECK_EQ(srgb_views->pViewFormats[1], VK_FORMAT_B8G8R8A8_UNORM);
  CHECK(layout_of(srgb.info()) == static_cast<const void *>(srgb_views->pNext));

  /* A format with no sibling is not mutable and lists no views. */
  const ExportableImageInfo wide(4u, 4u, 1u, 1u, 1u, r16g16b16a16_float, bind_render_target);
  CHECK(wide.supported());
  CHECK_EQ(wide.info().format, VK_FORMAT_R16G16B16A16_SFLOAT);
  CHECK_EQ(wide.info().flags, 0u);
  const auto *wide_external = static_cast<const VkExternalMemoryImageCreateInfo *>(wide.info().pNext);
  const auto *wide_layout = static_cast<const VkImageDrmFormatModifierExplicitCreateInfoEXT *>(wide_external->pNext);
  CHECK_EQ(wide_layout->sType, VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_EXPLICIT_CREATE_INFO_EXT);
  CHECK(layout_of(wide.info()) == wide_layout);
  CHECK_EQ(wide_layout->drmFormatModifier, linear_modifier);
  CHECK_EQ(wide_layout->pPlaneLayouts[0].rowPitch, 256u);
  CHECK_EQ(wide.info().tiling, VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT); }

/* NVIDIA's driver, left to lay out a linear image, puts its rows 32 bytes
 * apart, and its 3D engine faults on clearing a target whose rows are not 128
 * bytes aligned: a Windows.Graphics.Capture window capture 614 texels wide
 * (2464-byte rows) lost the host's Direct3D 11 device. Every row here is a
 * multiple of 256 bytes, whatever the width and the texel. */
TEST(exportable_image_info, every_row_starts_256_bytes_aligned_whatever_the_width)
{ CHECK_EQ(row_pitch(614u, b8g8r8a8_unorm), 2560u);
  CHECK_EQ(row_pitch(600u, b8g8r8a8_unorm), 2560u);
  CHECK_EQ(row_pitch(33u, r8g8b8a8_unorm), 256u);
  CHECK_EQ(row_pitch(1u, r8_unorm), 256u);
  CHECK_EQ(row_pitch(300u, r8_unorm), 512u);
  /* A row that is already a multiple is kept as it is. */
  CHECK_EQ(row_pitch(2560u, b8g8r8a8_unorm), 10240u);
  CHECK_EQ(row_pitch(64u, r32g32b32a32_float), 1024u);
  CHECK_EQ(row_pitch(65u, r32g32b32a32_float), 1280u);
  for (std::uint32_t width = 1u; width <= 4096u; width++) {
      const VkDeviceSize pitch = row_pitch(width, b8g8r8a8_unorm);
      CHECK_EQ(pitch % 256u, 0u);
      CHECK(pitch >= width * 4u && pitch < width * 4u + 256u);
  } }

/* A B8G8R8X8 texture is what DXVK makes it: B8G8R8A8 memory whose alpha no
 * view reads, mutable to its sRGB sibling as B8G8R8A8's is - the same Vulkan
 * format the scanout's export copies an X8 frame into (DxgiFormat's one row
 * for both). */
TEST(exportable_image_info, an_x8_texture_is_shared_as_the_b8g8r8a8_memory_dxvk_makes)
{ constexpr std::uint32_t b8g8r8x8_unorm = 88u;
  constexpr std::uint32_t b8g8r8x8_unorm_srgb = 93u;
  const ExportableImageInfo x8(4u, 4u, 1u, 1u, 1u, b8g8r8x8_unorm, bind_shader_resource | bind_render_target);
  CHECK(x8.supported());
  CHECK_EQ(x8.info().format, VK_FORMAT_B8G8R8A8_UNORM);
  const auto *external = static_cast<const VkExternalMemoryImageCreateInfo *>(x8.info().pNext);
  const auto *views = static_cast<const VkImageFormatListCreateInfo *>(external->pNext);
  CHECK_EQ(views->viewFormatCount, 2u);
  CHECK_EQ(views->pViewFormats[1], VK_FORMAT_B8G8R8A8_SRGB);
  const ExportableImageInfo srgb(4u, 4u, 1u, 1u, 1u, b8g8r8x8_unorm_srgb, bind_shader_resource);
  CHECK(srgb.supported());
  CHECK_EQ(srgb.info().format, VK_FORMAT_B8G8R8A8_SRGB); }

TEST(exportable_image_info, what_interop_does_not_share_is_left_to_dxvk)
{ const std::uint32_t binds = bind_shader_resource | bind_render_target;
  constexpr std::uint32_t r32g32b32_float = 6u;
  CHECK(!ExportableImageInfo(4u, 4u, 1u, 1u, 1u, r32g32b32_float, binds).supported());
  /* A single-channel float texture is shared, for an OpenCL image over it. */
  const ExportableImageInfo plane(4u, 4u, 1u, 1u, 1u, r32_float, binds);
  CHECK(plane.supported());
  CHECK_EQ(plane.info().format, VK_FORMAT_R32_SFLOAT);
  CHECK(!ExportableImageInfo(4u, 4u, 2u, 1u, 1u, r8g8b8a8_unorm, binds).supported());
  CHECK(!ExportableImageInfo(4u, 4u, 1u, 6u, 1u, r8g8b8a8_unorm, binds).supported());
  CHECK(!ExportableImageInfo(4u, 4u, 1u, 1u, 4u, r8g8b8a8_unorm, binds).supported());
  CHECK(!ExportableImageInfo(4u, 4u, 1u, 1u, 1u, r8g8b8a8_unorm, binds | bind_unordered_access).supported());
  CHECK(!ExportableImageInfo(0u, 4u, 1u, 1u, 1u, r8g8b8a8_unorm, binds).supported());
  CHECK(!ExportableImageInfo(4u, 0u, 1u, 1u, 1u, r8g8b8a8_unorm, binds).supported());
  /* The edge of each refusal: one mip, one layer, one sample are shared. */
  CHECK(ExportableImageInfo(1u, 1u, 1u, 1u, 1u, r8g8b8a8_unorm, binds).supported()); }

/* Tiled: the same image, its last link the list of tilings the driver picks
 * from, read where the caller keeps them. */
TEST(exportable_image_info, a_tiled_texture_lists_the_tilings_the_driver_picks_from)
{ const std::uint64_t tilings[] = {0x0100000000000002ull, 0x0100000000000001ull};
  const ExportableImageInfo tiled(2560u, 1440u, 1u, 1u, 1u, b8g8r8a8_unorm, bind_shader_resource | bind_render_target,
                                  tilings, 2u);
  CHECK(tiled.supported());
  const VkImageCreateInfo &info = tiled.info();
  CHECK_EQ(info.tiling, VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT);
  CHECK_EQ(info.format, VK_FORMAT_B8G8R8A8_UNORM);
  CHECK_EQ(info.extent.width, 2560u);
  CHECK_EQ(info.flags, static_cast<VkImageCreateFlags>(VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT));
  const auto *external = static_cast<const VkExternalMemoryImageCreateInfo *>(info.pNext);
  CHECK_EQ(external->handleTypes, static_cast<VkExternalMemoryHandleTypeFlags>(VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT));
  const auto *views = static_cast<const VkImageFormatListCreateInfo *>(external->pNext);
  CHECK_EQ(views->viewFormatCount, 2u);
  const auto *listed = reinterpret_cast<const VkImageDrmFormatModifierListCreateInfoEXT *>(layout_of(info));
  CHECK(static_cast<const void *>(listed) == views->pNext);
  CHECK_EQ(listed->sType, VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_LIST_CREATE_INFO_EXT);
  CHECK_EQ(listed->drmFormatModifierCount, 2u);
  CHECK(listed->pDrmFormatModifiers == tilings);
  CHECK(listed->pNext == nullptr);

  /* A format with no sibling lists the tilings straight after the external
   * memory. */
  const std::uint64_t one[] = {0x0100000000000002ull};
  const ExportableImageInfo wide(4u, 4u, 1u, 1u, 1u, r16g16b16a16_float, bind_render_target, one, 1u);
  CHECK(wide.supported());
  const auto *wide_external = static_cast<const VkExternalMemoryImageCreateInfo *>(wide.info().pNext);
  const auto *wide_listed = static_cast<const VkImageDrmFormatModifierListCreateInfoEXT *>(wide_external->pNext);
  CHECK_EQ(wide_listed->sType, VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_LIST_CREATE_INFO_EXT);
  CHECK_EQ(wide_listed->drmFormatModifierCount, 1u);

  /* No tiling to pick from is no image; nor is a texture rows would refuse. */
  CHECK(!ExportableImageInfo(4u, 4u, 1u, 1u, 1u, b8g8r8a8_unorm, bind_render_target, tilings, 0u).supported());
  CHECK(!ExportableImageInfo(4u, 4u, 1u, 1u, 1u, b8g8r8a8_unorm, bind_render_target, nullptr, 2u).supported());
  CHECK(!ExportableImageInfo(4u, 4u, 2u, 1u, 1u, b8g8r8a8_unorm, bind_render_target, tilings, 2u).supported()); }

/* Laid out as told: how an image another made lays the memory out - its
 * modifier, its plane's start and pitch - so the same GPU opens it again over
 * the same memory (CarriedShare). */
TEST(exportable_image_info, a_layout_another_image_was_given_is_described_as_it_was)
{ const ExportableImageInfo carried(614u, 289u, 1u, 1u, 1u, b8g8r8a8_unorm, bind_shader_resource | bind_render_target,
                                    std::uint64_t{0x0100000000000002ull}, std::uint64_t{0u}, std::uint64_t{2560u});
  CHECK(carried.supported());
  const auto *layout = layout_of(carried.info());
  CHECK_EQ(layout->sType, VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_EXPLICIT_CREATE_INFO_EXT);
  CHECK_EQ(layout->drmFormatModifier, std::uint64_t{0x0100000000000002ull});
  CHECK_EQ(layout->drmFormatModifierPlaneCount, 1u);
  CHECK_EQ(layout->pPlaneLayouts[0].offset, 0u);
  CHECK_EQ(layout->pPlaneLayouts[0].rowPitch, 2560u);
  CHECK_EQ(carried.info().tiling, VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT);
  /* A plane placed past the start keeps its offset; rows keep their pitch. */
  const ExportableImageInfo offset(4u, 4u, 1u, 1u, 1u, r8g8b8a8_unorm, bind_render_target, linear_modifier,
                                   std::uint64_t{4096u}, std::uint64_t{512u});
  CHECK(offset.supported());
  CHECK_EQ(layout_of(offset.info())->pPlaneLayouts[0].offset, 4096u);
  CHECK_EQ(layout_of(offset.info())->pPlaneLayouts[0].rowPitch, 512u);
  /* No pitch is no layout. */
  CHECK(!ExportableImageInfo(4u, 4u, 1u, 1u, 1u, r8g8b8a8_unorm, bind_render_target, linear_modifier,
                             std::uint64_t{0u}, std::uint64_t{0u})
             .supported()); }
