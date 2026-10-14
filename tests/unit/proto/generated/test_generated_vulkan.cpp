/* The generated wire, driven the way a host will drive it: what the guest
 * serialized comes back as the structure it was, and a frame that stops short
 * is refused rather than read past.
 *
 * Thousands of commands are generated and none of them is hand-checked; what
 * is checked here is the shape they all share - a plain struct, a struct with
 * a string and an array in it, and a chain - so that a change in the emitter
 * that breaks the shape breaks a test rather than a guest (feature 063). */
#include "harness/framework/test_registry.hpp"
#include "proto/generated/vulkan/vk_serialize.h"
#include "proto/wire/wire_arena.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

#include <cstdint>
#include <cstring>

using av::WireArena;
using av::WireReader;
using av::WireWriter;

namespace {

/* Room enough for what these decodes unflatten into, and no more: an arena
 * that is exactly as big as it needs to be is one that catches a decoder
 * asking for more. */
constexpr std::size_t arena_bytes = 4096u;

}  // namespace

TEST(generated_vulkan, a_plain_struct_round_trips)
{ std::uint8_t frame[64] = {};
  WireWriter writer(frame, sizeof frame);
  const VkExtent2D sent = {1600u, 1200u};
  av::vkproto::serialize_VkExtent2D(writer, sent);
  CHECK_STATUS(writer.status(), av::Status::ok);

  std::uint8_t block[arena_bytes];
  WireArena arena(block, sizeof block);
  WireReader reader(frame, writer.offset());
  VkExtent2D received = {};
  CHECK(av::vkproto::deserialize_VkExtent2D(reader, &received, arena));
  CHECK_EQ(received.width, 1600u);
  CHECK_EQ(received.height, 1200u);
  CHECK_EQ(reader.remaining(), 0u); }

/* A frame cut short at every length there is: none of them may be read past,
 * and none may report success. */
TEST(generated_vulkan, a_truncated_frame_is_refused_at_every_length)
{ std::uint8_t frame[64] = {};
  WireWriter writer(frame, sizeof frame);
  av::vkproto::serialize_VkExtent2D(writer, VkExtent2D{7u, 9u});
  const std::size_t whole = writer.offset();
  CHECK(whole > 1u);

  for (std::size_t length = 0u; length < whole; length++) {
      std::uint8_t block[arena_bytes];
      WireArena arena(block, sizeof block);
      WireReader reader(frame, length);
      VkExtent2D received = {};
      CHECK(!av::vkproto::deserialize_VkExtent2D(reader, &received, arena));
      CHECK_STATUS(reader.status(), av::Status::malformed);
  } }

/* A structure that points at things: the string and the pNext chain are what
 * the arena is for, and what a flat frame has to rebuild. */
TEST(generated_vulkan, a_struct_that_points_at_things_round_trips)
{ std::uint8_t frame[512] = {};
  WireWriter writer(frame, sizeof frame);
  VkApplicationInfo sent = {};
  sent.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  sent.pNext = nullptr;
  sent.pApplicationName = "accel-virt";
  sent.applicationVersion = 3u;
  sent.pEngineName = "av";
  sent.engineVersion = 4u;
  constexpr std::uint32_t api_version_1_3 = (1u << 22) | (3u << 12);
  sent.apiVersion = api_version_1_3;
  av::vkproto::serialize_VkApplicationInfo(writer, sent);
  CHECK_STATUS(writer.status(), av::Status::ok);

  std::uint8_t block[arena_bytes];
  WireArena arena(block, sizeof block);
  WireReader reader(frame, writer.offset());
  VkApplicationInfo received = {};
  CHECK(av::vkproto::deserialize_VkApplicationInfo(reader, &received, arena));
  CHECK_EQ(received.sType, VK_STRUCTURE_TYPE_APPLICATION_INFO);
  CHECK(received.pApplicationName != nullptr);
  CHECK(std::strcmp(received.pApplicationName, "accel-virt") == 0);
  CHECK(std::strcmp(received.pEngineName, "av") == 0);
  CHECK_EQ(received.applicationVersion, 3u);
  CHECK_EQ(received.engineVersion, 4u);
  CHECK_EQ(received.apiVersion, api_version_1_3);
  /* What it points at is the arena's, not the frame's. */
  CHECK(arena.used() > 0u); }

/* An arena too small for what the frame describes: the decode fails rather
 * than the host writing past the block it was given. */
TEST(generated_vulkan, a_decode_with_no_room_fails_instead_of_overrunning)
{ std::uint8_t frame[512] = {};
  WireWriter writer(frame, sizeof frame);
  VkApplicationInfo sent = {};
  sent.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  sent.pApplicationName = "a name longer than the block it will be given";
  sent.pEngineName = "av";
  av::vkproto::serialize_VkApplicationInfo(writer, sent);
  CHECK_STATUS(writer.status(), av::Status::ok);

  std::uint8_t block[8];
  WireArena arena(block, sizeof block);
  WireReader reader(frame, writer.offset());
  VkApplicationInfo received = {};
  CHECK(!av::vkproto::deserialize_VkApplicationInfo(reader, &received, arena));
  CHECK(arena.used() <= sizeof block); }

namespace {

/* `sent` serialized as the guest's ICD sends it, and decoded as the host
 * decodes it into `received` over `block`: whether the decoder took the
 * frame. */
template <typename T>
bool decoded(void (*write)(WireWriter &, const T &), bool (*read)(WireReader &, T *, WireArena &), const T &sent,
             T &received, std::uint8_t (&block)[arena_bytes])
{ std::uint8_t frame[512] = {};
  WireWriter writer(frame, sizeof frame);
  write(writer, sent);
  WireArena arena(block, sizeof block);
  WireReader reader(frame, writer.offset());
  return read(reader, &received, arena); }

}  // namespace

/* An array the API requires whenever its count is not zero, left out with a
 * count that says it has elements, is a frame the host refuses: the driver
 * would read that many through a null pointer. With a count of zero it is
 * the null the API allows. */
TEST(generated_vulkan, a_required_array_left_out_with_elements_is_refused)
{ std::uint8_t block[arena_bytes];
  VkSubmitInfo sent = {};
  sent.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  sent.commandBufferCount = 1u;
  VkSubmitInfo received = {};
  CHECK(!decoded(av::vkproto::serialize_VkSubmitInfo, av::vkproto::deserialize_VkSubmitInfo, sent, received, block));
  sent.commandBufferCount = 0u;
  CHECK(decoded(av::vkproto::serialize_VkSubmitInfo, av::vkproto::deserialize_VkSubmitInfo, sent, received, block));
  CHECK(received.pCommandBuffers == nullptr);
  CHECK(received.pWaitDstStageMask == nullptr);
  /* A wait with its semaphore and no stage mask is the same refusal. */
  const VkSemaphore semaphore = reinterpret_cast<VkSemaphore>(std::uintptr_t{0x1000u});
  sent.waitSemaphoreCount = 1u;
  sent.pWaitSemaphores = &semaphore;
  CHECK(!decoded(av::vkproto::serialize_VkSubmitInfo, av::vkproto::deserialize_VkSubmitInfo, sent, received, block));
  const VkPipelineStageFlags stage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
  sent.pWaitDstStageMask = &stage;
  CHECK(decoded(av::vkproto::serialize_VkSubmitInfo, av::vkproto::deserialize_VkSubmitInfo, sent, received, block));
  CHECK_EQ(received.pWaitDstStageMask[0], stage); }

/* A structure the API requires - the create info of a create-info query - is
 * refused when left out, and decoded when it is there. */
TEST(generated_vulkan, a_required_structure_left_out_is_refused)
{ std::uint8_t block[arena_bytes];
  VkDeviceImageMemoryRequirements sent = {};
  sent.sType = VK_STRUCTURE_TYPE_DEVICE_IMAGE_MEMORY_REQUIREMENTS;
  VkDeviceImageMemoryRequirements received = {};
  CHECK(!decoded(av::vkproto::serialize_VkDeviceImageMemoryRequirements, av::vkproto::deserialize_VkDeviceImageMemoryRequirements, sent, received, block));
  VkImageCreateInfo create = {};
  create.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  create.extent = {8u, 8u, 1u};
  sent.pCreateInfo = &create;
  CHECK(decoded(av::vkproto::serialize_VkDeviceImageMemoryRequirements, av::vkproto::deserialize_VkDeviceImageMemoryRequirements, sent, received, block));
  CHECK(received.pCreateInfo != nullptr);
  CHECK_EQ(received.pCreateInfo->extent.width, 8u); }

/* The wire says nothing for a null string and an empty one alike: a string
 * the API requires - a shader's entry point - is the empty one, which a
 * driver can compare, and an optional one stays null. */
TEST(generated_vulkan, a_required_string_left_out_is_empty_and_an_optional_one_null)
{ std::uint8_t block[arena_bytes];
  VkPipelineShaderStageCreateInfo stage = {};
  stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  VkPipelineShaderStageCreateInfo received = {};
  CHECK(decoded(av::vkproto::serialize_VkPipelineShaderStageCreateInfo, av::vkproto::deserialize_VkPipelineShaderStageCreateInfo, stage, received, block));
  CHECK(received.pName != nullptr);
  CHECK_EQ(received.pName[0], '\0');
  VkApplicationInfo application = {};
  application.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  VkApplicationInfo answered = {};
  answered.pApplicationName = "stale";
  CHECK(decoded(av::vkproto::serialize_VkApplicationInfo, av::vkproto::deserialize_VkApplicationInfo, application, answered, block));
  CHECK(answered.pApplicationName == nullptr);
  CHECK(answered.pEngineName == nullptr); }

/* An array the registry leaves to conditions - a write's image, buffer and
 * texel infos, only one of which its type reads - is zeroes when left out
 * with elements, never a null pointer the driver might read; an optional
 * array stays null. */
TEST(generated_vulkan, a_conditional_array_left_out_is_zeroes_and_an_optional_one_null)
{ std::uint8_t block[arena_bytes];
  VkWriteDescriptorSet write = {};
  write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  write.descriptorCount = 2u;
  write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
  VkWriteDescriptorSet received = {};
  CHECK(decoded(av::vkproto::serialize_VkWriteDescriptorSet, av::vkproto::deserialize_VkWriteDescriptorSet, write, received, block));
  CHECK(received.pImageInfo != nullptr);
  CHECK(received.pBufferInfo != nullptr);
  CHECK(received.pTexelBufferView != nullptr);
  for (std::uint32_t i = 0u; i < 2u; i++) {
      CHECK(received.pImageInfo[i].imageView == VK_NULL_HANDLE);
      CHECK_EQ(received.pBufferInfo[i].range, VkDeviceSize{0u});
      CHECK(received.pTexelBufferView[i] == VK_NULL_HANDLE);
  }
  write.descriptorCount = 0u;
  CHECK(decoded(av::vkproto::serialize_VkWriteDescriptorSet, av::vkproto::deserialize_VkWriteDescriptorSet, write, received, block));
  CHECK(received.pImageInfo == nullptr);

  VkAttachmentReference colour = {0u, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
  VkSubpassDescription subpass = {};
  subpass.colorAttachmentCount = 1u;
  subpass.pColorAttachments = &colour;
  VkSubpassDescription unresolved = {};
  CHECK(decoded(av::vkproto::serialize_VkSubpassDescription, av::vkproto::deserialize_VkSubpassDescription, subpass, unresolved, block));
  CHECK(unresolved.pColorAttachments != nullptr);
  CHECK(unresolved.pResolveAttachments == nullptr);
  CHECK(unresolved.pDepthStencilAttachment == nullptr); }

/* A conditional structure left out is a zeroed one. */
TEST(generated_vulkan, a_conditional_structure_left_out_is_zeroes)
{ std::uint8_t block[arena_bytes];
  VkCommandBufferInheritanceViewportScissorInfoNV sent = {};
  sent.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_VIEWPORT_SCISSOR_INFO_NV;
  VkCommandBufferInheritanceViewportScissorInfoNV received = {};
  CHECK(decoded(av::vkproto::serialize_VkCommandBufferInheritanceViewportScissorInfoNV, av::vkproto::deserialize_VkCommandBufferInheritanceViewportScissorInfoNV, sent, received, block));
  CHECK(received.pViewportDepths != nullptr);
  CHECK_EQ(received.pViewportDepths->maxDepth, 0.0f); }

/* Zeroes a frame asks for are the arena's like anything else it decodes: a
 * count past the block is refused, never written past it. */
TEST(generated_vulkan, zeroes_past_the_arena_are_refused_instead_of_overrunning)
{ std::uint8_t block[arena_bytes];
  VkWriteDescriptorSet write = {};
  write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  write.descriptorCount = 1u << 20;
  write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
  VkWriteDescriptorSet received = {};
  CHECK(!decoded(av::vkproto::serialize_VkWriteDescriptorSet, av::vkproto::deserialize_VkWriteDescriptorSet, write, received, block)); }

/* A handle the API requires in a structure, named null, is refused: the
 * driver dereferences a non-dispatchable handle as it would a pointer. */
TEST(generated_vulkan, a_required_handle_named_null_is_refused)
{ std::uint8_t block[arena_bytes];
  VkImageMemoryRequirementsInfo2 sent = {};
  sent.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2;
  VkImageMemoryRequirementsInfo2 received = {};
  CHECK(!decoded(av::vkproto::serialize_VkImageMemoryRequirementsInfo2, av::vkproto::deserialize_VkImageMemoryRequirementsInfo2, sent, received, block));
  sent.image = reinterpret_cast<VkImage>(std::uintptr_t{0x2000u});
  CHECK(decoded(av::vkproto::serialize_VkImageMemoryRequirementsInfo2, av::vkproto::deserialize_VkImageMemoryRequirementsInfo2, sent, received, block));
  CHECK(received.image == sent.image); }
