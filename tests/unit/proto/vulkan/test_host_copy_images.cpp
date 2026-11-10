#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "proto/vulkan/host_copy_images.hpp"

#include <cstdint>

using av::HostCopyImages;
using av::MockPlatform;

namespace {

VkImageCreateInfo image_of(VkFormat format, std::uint32_t layers, VkImageUsageFlags usage = 0x00400000u)
{ VkImageCreateInfo create = {};
  create.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  create.format = format;
  create.arrayLayers = layers;
  create.usage = usage;
  return create; }

VkImageSubresourceLayers color(std::uint32_t base, std::uint32_t count)
{ return VkImageSubresourceLayers{VK_IMAGE_ASPECT_COLOR_BIT, 0u, base, count}; }

}

TEST(host_copy_images, a_tight_color_region_is_width_times_height_times_the_texel_size)
{ MockPlatform platform;
  HostCopyImages images(platform);
  CHECK(images.record(0x10u, 0x1u, image_of(VK_FORMAT_R8G8B8A8_UNORM, 1u)));
  std::uint64_t bytes = 0u;
  CHECK(images.bytes(0x10u, color(0u, 1u), VkExtent3D{32u, 32u, 1u}, 0u, 0u, bytes));
  CHECK_EQ(bytes, std::uint64_t{32u * 32u * 4u}); }

TEST(host_copy_images, a_row_length_and_image_height_widen_the_stride_and_the_last_row_stays_short)
{ MockPlatform platform;
  HostCopyImages images(platform);
  CHECK(images.record(0x10u, 0x1u, image_of(VK_FORMAT_R8G8B8A8_UNORM, 4u)));
  std::uint64_t bytes = 0u;
  CHECK(images.bytes(0x10u, color(1u, 2u), VkExtent3D{28u, 25u, 1u}, 32u, 40u, bytes));
  const std::uint64_t row = 32u * 4u;
  const std::uint64_t slice = row * 40u;
  CHECK_EQ(bytes, slice + 24u * row + 28u * 4u);
  CHECK(!images.bytes(0x10u, color(1u, 2u), VkExtent3D{28u, 25u, 1u}, 16u, 0u, bytes));
  CHECK(!images.bytes(0x10u, color(3u, 2u), VkExtent3D{4u, 4u, 1u}, 0u, 0u, bytes));
  CHECK(images.bytes(0x10u, color(1u, VK_REMAINING_ARRAY_LAYERS), VkExtent3D{4u, 4u, 1u}, 0u, 0u, bytes));
  CHECK_EQ(bytes, std::uint64_t{2u * 4u * 4u * 4u + 3u * 4u * 4u + 4u * 4u} - std::uint64_t{0u}); }

TEST(host_copy_images, a_compressed_format_counts_blocks_and_depth_aspects_their_packed_size)
{ MockPlatform platform;
  HostCopyImages images(platform);
  CHECK(images.record(0x10u, 0x1u, image_of(VK_FORMAT_BC1_RGB_UNORM_BLOCK, 1u)));
  CHECK(images.record(0x20u, 0x1u, image_of(VK_FORMAT_D32_SFLOAT_S8_UINT, 1u)));
  std::uint64_t bytes = 0u;
  CHECK(images.bytes(0x10u, color(0u, 1u), VkExtent3D{10u, 6u, 1u}, 0u, 0u, bytes));
  CHECK_EQ(bytes, std::uint64_t{3u * 8u + 3u * 8u});
  const VkImageSubresourceLayers depth = {VK_IMAGE_ASPECT_DEPTH_BIT, 0u, 0u, 1u};
  const VkImageSubresourceLayers stencil = {VK_IMAGE_ASPECT_STENCIL_BIT, 0u, 0u, 1u};
  CHECK(images.bytes(0x20u, depth, VkExtent3D{8u, 8u, 1u}, 0u, 0u, bytes));
  CHECK_EQ(bytes, std::uint64_t{8u * 8u * 4u});
  CHECK(images.bytes(0x20u, stencil, VkExtent3D{8u, 8u, 1u}, 0u, 0u, bytes));
  CHECK_EQ(bytes, std::uint64_t{8u * 8u});
  CHECK(!images.bytes(0x20u, color(0u, 1u), VkExtent3D{8u, 8u, 1u}, 0u, 0u, bytes)); }

TEST(host_copy_images, an_image_without_host_transfer_usage_a_planar_format_and_a_stranger_are_refused)
{ MockPlatform platform;
  HostCopyImages images(platform);
  CHECK(images.record(0x10u, 0x1u, image_of(VK_FORMAT_R8G8B8A8_UNORM, 1u, VK_IMAGE_USAGE_SAMPLED_BIT)));
  CHECK(images.record(0x20u, 0x1u, image_of(VK_FORMAT_G8_B8R8_2PLANE_420_UNORM, 1u)));
  CHECK(!images.record(0u, 0x1u, image_of(VK_FORMAT_R8G8B8A8_UNORM, 1u)));
  std::uint64_t bytes = 7u;
  CHECK(!images.bytes(0x10u, color(0u, 1u), VkExtent3D{4u, 4u, 1u}, 0u, 0u, bytes));
  CHECK_EQ(bytes, std::uint64_t{0u});
  CHECK(!images.bytes(0x20u, color(0u, 1u), VkExtent3D{4u, 4u, 1u}, 0u, 0u, bytes));
  CHECK(!images.bytes(0x99u, color(0u, 1u), VkExtent3D{4u, 4u, 1u}, 0u, 0u, bytes)); }

TEST(host_copy_images, a_region_past_the_ceiling_or_with_overflowing_slices_is_refused_and_forgetting_works)
{ MockPlatform platform;
  HostCopyImages images(platform);
  CHECK(images.record(0x10u, 0x1u, image_of(VK_FORMAT_R32G32B32A32_SFLOAT, 0xFFFFu)));
  std::uint64_t bytes = 0u;
  CHECK(!images.bytes(0x10u, color(0u, 0xFFFFu), VkExtent3D{16384u, 16384u, 64u}, 0u, 0u, bytes));
  CHECK(images.bytes(0x10u, color(0u, 1u), VkExtent3D{0u, 4u, 1u}, 0u, 0u, bytes));
  CHECK_EQ(bytes, std::uint64_t{0u});
  CHECK_EQ(images.live(), 1u);
  images.forget(0x10u);
  CHECK_EQ(images.live(), 0u);
  CHECK(images.record(0x30u, 0x5u, image_of(VK_FORMAT_R8_UNORM, 1u)));
  images.forget_device(0x5u);
  CHECK_EQ(images.live(), 0u);
  CHECK(images.invariants_hold()); }
