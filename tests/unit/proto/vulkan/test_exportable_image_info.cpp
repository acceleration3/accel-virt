/* The image a shared Direct3D 11 texture is made as when another API must open
 * its memory: the formats interop shares, mutable between linear and sRGB where
 * the format has both, drawn and sampled whatever the binds, over dma-buf
 * memory - and nothing for a texture interop does not share. */
#include "proto/vulkan/exportable_image_info.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>

using av::ExportableImageInfo;

namespace {

constexpr std::uint32_t r8g8b8a8_unorm = 28u;
constexpr std::uint32_t b8g8r8a8_unorm_srgb = 91u;
constexpr std::uint32_t r16g16b16a16_float = 10u;
constexpr std::uint32_t r32_float = 41u;
constexpr std::uint32_t bind_shader_resource = 0x8u;
constexpr std::uint32_t bind_render_target = 0x20u;
constexpr std::uint32_t bind_unordered_access = 0x80u;

}  // namespace

TEST(exportable_image_info, a_shared_colour_texture_is_an_exportable_image_mutable_to_its_srgb_sibling)
{ const ExportableImageInfo shared(640u, 360u, 1u, 1u, 1u, r8g8b8a8_unorm, bind_shader_resource | bind_render_target);
  CHECK(shared.supported());
  const VkImageCreateInfo &info = shared.info();
  CHECK_EQ(info.imageType, VK_IMAGE_TYPE_2D);
  CHECK_EQ(info.format, VK_FORMAT_R8G8B8A8_UNORM);
  CHECK_EQ(info.extent.width, 640u);
  CHECK_EQ(info.extent.height, 360u);
  CHECK_EQ(info.tiling, VK_IMAGE_TILING_OPTIMAL);
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

  /* An sRGB texture's sibling is its linear format; one sampled only is still
   * made drawable, since the opening API draws into it. */
  const ExportableImageInfo srgb(4u, 4u, 1u, 1u, 0u, b8g8r8a8_unorm_srgb, bind_shader_resource);
  CHECK(srgb.supported());
  CHECK_EQ(srgb.info().format, VK_FORMAT_B8G8R8A8_SRGB);
  CHECK((srgb.info().usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) != 0u);
  const auto *srgb_external = static_cast<const VkExternalMemoryImageCreateInfo *>(srgb.info().pNext);
  const auto *srgb_views = static_cast<const VkImageFormatListCreateInfo *>(srgb_external->pNext);
  CHECK_EQ(srgb_views->pViewFormats[1], VK_FORMAT_B8G8R8A8_UNORM);

  /* A format with no sibling is not mutable and lists no views. */
  const ExportableImageInfo wide(4u, 4u, 1u, 1u, 1u, r16g16b16a16_float, bind_render_target);
  CHECK(wide.supported());
  CHECK_EQ(wide.info().format, VK_FORMAT_R16G16B16A16_SFLOAT);
  CHECK_EQ(wide.info().flags, 0u);
  CHECK(static_cast<const VkExternalMemoryImageCreateInfo *>(wide.info().pNext)->pNext == nullptr); }

TEST(exportable_image_info, what_interop_does_not_share_is_left_to_dxvk)
{ const std::uint32_t binds = bind_shader_resource | bind_render_target;
  CHECK(!ExportableImageInfo(4u, 4u, 1u, 1u, 1u, r32_float, binds).supported());
  CHECK(!ExportableImageInfo(4u, 4u, 2u, 1u, 1u, r8g8b8a8_unorm, binds).supported());
  CHECK(!ExportableImageInfo(4u, 4u, 1u, 6u, 1u, r8g8b8a8_unorm, binds).supported());
  CHECK(!ExportableImageInfo(4u, 4u, 1u, 1u, 4u, r8g8b8a8_unorm, binds).supported());
  CHECK(!ExportableImageInfo(4u, 4u, 1u, 1u, 1u, r8g8b8a8_unorm, binds | bind_unordered_access).supported());
  CHECK(!ExportableImageInfo(0u, 4u, 1u, 1u, 1u, r8g8b8a8_unorm, binds).supported());
  CHECK(!ExportableImageInfo(4u, 0u, 1u, 1u, 1u, r8g8b8a8_unorm, binds).supported());
  /* The edge of each refusal: one mip, one layer, one sample are shared. */
  CHECK(ExportableImageInfo(1u, 1u, 1u, 1u, 1u, r8g8b8a8_unorm, binds).supported()); }
