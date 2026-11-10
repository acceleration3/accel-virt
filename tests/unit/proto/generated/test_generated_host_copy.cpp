#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "harness/transport/mock_transport.hpp"
#include "proto/generated/vulkan/vk_default_dispatch.h"
#include "proto/generated/vulkan/vk_dispatch_client.h"
#include "proto/generated/vulkan/vk_dispatch_server.h"
#include "proto/generated/vulkan/vk_handle_translation.h"
#include "proto/generated/vulkan/vk_protocol_ids.h"
#include "proto/vulkan/host_copy_images.hpp"
#include "proto/wire/wire_arena.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

#include <cstdint>
#include <cstring>
#include <vector>

using av::HostCopyImages;
using av::MockPlatform;
using av::MockTransport;
using av::WireArena;
using av::WireReader;
using av::WireWriter;
namespace vkproto = av::vkproto;

namespace {

constexpr std::uint64_t image_number = 0x4000u;

class Sizing final : public vkproto::HandleTranslation {
public:
    explicit Sizing(HostCopyImages &images) : images_(images) {}
    uint64_t wire_for(vkproto::HandleKind, uint64_t handle) override { return handle; }
    bool     handle_for(vkproto::HandleKind, uint64_t wire, uint64_t &handle) const override
    { handle = wire; return true; }
    uint64_t template_bytes(VkDescriptorUpdateTemplate) const override { return 0u; }
    bool     image_copy_bytes(VkImage image, const VkImageSubresourceLayers &layers, VkExtent3D extent,
                              uint32_t row_length, uint32_t image_height, uint64_t &bytes) const override
    { return images_.bytes(reinterpret_cast<uint64_t>(image), layers, extent, row_length, image_height, bytes); }

private:
    HostCopyImages &images_;
};

class Host final : public vkproto::DefaultServerDispatch {
public:
    explicit Host(HostCopyImages &images) : images_(images) {}
    bool image_copy_bytes(VkImage image, const VkImageSubresourceLayers &layers, VkExtent3D extent,
                          uint32_t row_length, uint32_t image_height, uint64_t &bytes) const override
    { return images_.bytes(reinterpret_cast<uint64_t>(image), layers, extent, row_length, image_height, bytes); }
    VkResult on_vkCopyMemoryToImageEXT(VkDevice, const VkCopyMemoryToImageInfo *info) override
    { uploads++;
      for (uint32_t at = 0u; at < info->regionCount; at++) {
          const auto *bytes = static_cast<const std::uint8_t *>(info->pRegions[at].pHostPointer);
          seen.assign(bytes, bytes + expected_bytes);
      }
      return VK_SUCCESS; }
    VkResult on_vkCopyImageToMemoryEXT(VkDevice, const VkCopyImageToMemoryInfo *info) override
    { downloads++;
      for (uint32_t at = 0u; at < info->regionCount; at++) {
          auto *bytes = static_cast<std::uint8_t *>(info->pRegions[at].pHostPointer);
          for (std::uint32_t index = 0u; index < expected_bytes; index++) bytes[index] = static_cast<std::uint8_t>(index * 3u + 1u);
      }
      return VK_SUCCESS; }

    std::uint32_t             uploads = 0u;
    std::uint32_t             downloads = 0u;
    std::uint32_t             expected_bytes = 0u;
    std::vector<std::uint8_t> seen;

private:
    HostCopyImages &images_;
};

VkImageCreateInfo host_image()
{ VkImageCreateInfo create = {};
  create.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  create.format = VK_FORMAT_R8G8B8A8_UNORM;
  create.arrayLayers = 1u;
  create.usage = 0x00400000u;
  return create; }

bool run(Host &host, std::uint32_t command, const std::uint8_t *payload, std::size_t length, WireWriter &reply,
         std::size_t cut = ~std::size_t{0})
{ static std::uint8_t block[1u << 16u];
  WireArena           arena(block, sizeof block);
  WireReader          reader(payload, cut < length ? cut : length);
  return vkproto::dispatch_command(host, command, reader, reply, arena); }

}

TEST(generated_host_copy, a_memory_to_image_copy_sends_exactly_the_bytes_the_region_touches)
{ MockPlatform platform;
  HostCopyImages guest_images(platform);
  HostCopyImages host_images(platform);
  CHECK(guest_images.record(image_number, 1u, host_image()));
  CHECK(host_images.record(image_number, 1u, host_image()));
  Sizing        guest(guest_images);
  MockTransport transport;
  std::vector<std::uint8_t> source(32u * 32u * 4u);
  for (std::size_t at = 0u; at < source.size(); at++) source[at] = static_cast<std::uint8_t>(at * 5u + 2u);
  VkMemoryToImageCopy region = {};
  region.sType = VK_STRUCTURE_TYPE_MEMORY_TO_IMAGE_COPY;
  region.pHostPointer = source.data();
  region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u};
  region.imageExtent = {32u, 32u, 1u};
  VkCopyMemoryToImageInfo info = {};
  info.sType = VK_STRUCTURE_TYPE_COPY_MEMORY_TO_IMAGE_INFO;
  info.dstImage = reinterpret_cast<VkImage>(image_number);
  info.dstImageLayout = VK_IMAGE_LAYOUT_GENERAL;
  info.regionCount = 1u;
  info.pRegions = &region;
  std::uint8_t answer[16] = {};
  WireWriter   scripted(answer, sizeof answer);
  scripted.write_pod<std::int32_t>(0);
  transport.set_reply(answer, scripted.offset());
  CHECK_EQ(static_cast<int>(vkproto::av_vkCopyMemoryToImageEXT(transport, guest, reinterpret_cast<VkDevice>(0x10u), &info)),
           static_cast<int>(VK_SUCCESS));
  Host host(host_images);
  host.expected_bytes = 32u * 32u * 4u;
  std::uint8_t reply_bytes[16] = {};
  WireWriter   reply(reply_bytes, sizeof reply_bytes);
  CHECK(run(host, vkproto::kCmd_vkCopyMemoryToImageEXT, transport.last_payload(), transport.last_length(), reply));
  CHECK_EQ(host.uploads, 1u);
  CHECK(host.seen == source);
  const std::size_t whole = transport.last_length();
  for (std::size_t cut = 0u; cut < whole; cut += 97u) {
      Host cut_host(host_images);
      CHECK(!run(cut_host, vkproto::kCmd_vkCopyMemoryToImageEXT, transport.last_payload(), whole, reply, cut));
      CHECK_EQ(cut_host.uploads, 0u);
  } }

TEST(generated_host_copy, a_region_larger_than_the_frame_claims_or_for_an_image_the_host_never_made_is_refused)
{ MockPlatform platform;
  HostCopyImages guest_images(platform);
  HostCopyImages host_images(platform);
  CHECK(guest_images.record(image_number, 1u, host_image()));
  Sizing        guest(guest_images);
  MockTransport transport;
  std::vector<std::uint8_t> source(8u * 8u * 4u, 0x5Au);
  VkMemoryToImageCopy region = {};
  region.sType = VK_STRUCTURE_TYPE_MEMORY_TO_IMAGE_COPY;
  region.pHostPointer = source.data();
  region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u};
  region.imageExtent = {8u, 8u, 1u};
  VkCopyMemoryToImageInfo info = {};
  info.sType = VK_STRUCTURE_TYPE_COPY_MEMORY_TO_IMAGE_INFO;
  info.dstImage = reinterpret_cast<VkImage>(image_number);
  info.regionCount = 1u;
  info.pRegions = &region;
  std::uint8_t answer[16] = {};
  WireWriter   scripted(answer, sizeof answer);
  scripted.write_pod<std::int32_t>(0);
  transport.set_reply(answer, scripted.offset());
  CHECK_EQ(static_cast<int>(vkproto::av_vkCopyMemoryToImageEXT(transport, guest, reinterpret_cast<VkDevice>(0x10u), &info)),
           static_cast<int>(VK_SUCCESS));
  std::uint8_t reply_bytes[16] = {};
  WireWriter   reply(reply_bytes, sizeof reply_bytes);
  Host stranger(host_images);
  CHECK(!run(stranger, vkproto::kCmd_vkCopyMemoryToImageEXT, transport.last_payload(), transport.last_length(), reply));
  CHECK_EQ(stranger.uploads, 0u);
  VkImageCreateInfo other = host_image();
  other.format = VK_FORMAT_R8_UNORM;
  CHECK(host_images.record(image_number, 1u, other));
  Host disagreeing(host_images);
  disagreeing.expected_bytes = 8u * 8u;
  CHECK(!run(disagreeing, vkproto::kCmd_vkCopyMemoryToImageEXT, transport.last_payload(), transport.last_length(), reply));
  CHECK_EQ(disagreeing.uploads, 0u); }

TEST(generated_host_copy, an_unknown_image_or_format_is_refused_by_the_client_before_anything_is_sent)
{ MockPlatform platform;
  HostCopyImages guest_images(platform);
  Sizing        guest(guest_images);
  MockTransport transport;
  VkMemoryToImageCopy region = {};
  region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u};
  region.imageExtent = {4u, 4u, 1u};
  VkCopyMemoryToImageInfo info = {};
  info.dstImage = reinterpret_cast<VkImage>(image_number);
  info.regionCount = 1u;
  info.pRegions = &region;
  CHECK_EQ(static_cast<int>(vkproto::av_vkCopyMemoryToImageEXT(transport, guest, reinterpret_cast<VkDevice>(0x10u), &info)),
           static_cast<int>(VK_ERROR_FORMAT_NOT_SUPPORTED));
  CHECK_EQ(transport.sends(), 0u);
  VkCopyImageToMemoryInfo back = {};
  VkImageToMemoryCopy     out = {};
  out.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u};
  out.imageExtent = {4u, 4u, 1u};
  back.srcImage = reinterpret_cast<VkImage>(image_number);
  back.regionCount = 1u;
  back.pRegions = &out;
  CHECK_EQ(static_cast<int>(vkproto::av_vkCopyImageToMemoryEXT(transport, guest, reinterpret_cast<VkDevice>(0x10u), &back)),
           static_cast<int>(VK_ERROR_FORMAT_NOT_SUPPORTED));
  CHECK_EQ(transport.sends(), 0u); }

TEST(generated_host_copy, an_image_to_memory_copy_returns_exactly_the_bytes_the_region_touches)
{ MockPlatform platform;
  HostCopyImages guest_images(platform);
  HostCopyImages host_images(platform);
  CHECK(guest_images.record(image_number, 1u, host_image()));
  CHECK(host_images.record(image_number, 1u, host_image()));
  Sizing        guest(guest_images);
  MockTransport transport;
  std::vector<std::uint8_t> back(16u * 8u * 4u, 0xEEu);
  VkImageToMemoryCopy region = {};
  region.sType = VK_STRUCTURE_TYPE_IMAGE_TO_MEMORY_COPY;
  region.pHostPointer = back.data();
  region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u};
  region.imageExtent = {16u, 8u, 1u};
  VkCopyImageToMemoryInfo info = {};
  info.sType = VK_STRUCTURE_TYPE_COPY_IMAGE_TO_MEMORY_INFO;
  info.srcImage = reinterpret_cast<VkImage>(image_number);
  info.srcImageLayout = VK_IMAGE_LAYOUT_GENERAL;
  info.regionCount = 1u;
  info.pRegions = &region;
  static_cast<void>(vkproto::av_vkCopyImageToMemoryEXT(transport, guest, reinterpret_cast<VkDevice>(0x10u), &info));
  Host host(host_images);
  host.expected_bytes = 16u * 8u * 4u;
  std::vector<std::uint8_t> answer(host.expected_bytes + 16u);
  WireWriter   reply(answer.data(), answer.size());
  CHECK(run(host, vkproto::kCmd_vkCopyImageToMemoryEXT, transport.last_payload(), transport.last_length(), reply));
  CHECK_EQ(host.downloads, 1u);
  CHECK_EQ(reply.offset(), std::size_t{4u} + host.expected_bytes);
  transport.set_reply(answer.data(), reply.offset());
  CHECK_EQ(static_cast<int>(vkproto::av_vkCopyImageToMemoryEXT(transport, guest, reinterpret_cast<VkDevice>(0x10u), &info)),
           static_cast<int>(VK_SUCCESS));
  for (std::size_t at = 0u; at < back.size(); at++) CHECK_EQ(back[at], static_cast<std::uint8_t>(at * 3u + 1u));
  transport.set_reply(answer.data(), reply.offset() - 1u);
  CHECK(vkproto::av_vkCopyImageToMemoryEXT(transport, guest, reinterpret_cast<VkDevice>(0x10u), &info) != VK_SUCCESS); }
