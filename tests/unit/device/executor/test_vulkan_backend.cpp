/* One guest context's Vulkan on the host's own.
 *
 * What is asserted is what the ABI needs and the generated dispatch does not
 * give: whether an id is carried at all, and whether a frame decoded. The
 * driver's own answer is deliberately not asserted - these must pass on a
 * machine with no GPU, where every command decodes and every VkResult says so. */
#include "device/executor/vulkan_backend.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/generated/vulkan/vk_protocol_ids.h"
#include "proto/generated/vulkan/vk_serialize.h"
#include "proto/wire/wire_arena.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

#include <cstdint>

using av::VulkanBackend;
using av::WireArena;
using av::WireReader;
using av::WireWriter;

namespace {

constexpr std::size_t block_bytes = 65536u;

std::size_t create_instance_frame(std::uint8_t *frame, std::size_t capacity)
{ WireWriter writer(frame, capacity);
  VkApplicationInfo application = {};
  application.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  application.pApplicationName = "test";
  application.apiVersion = (1u << 22) | (1u << 12);
  VkInstanceCreateInfo create = {};
  create.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  create.pApplicationInfo = &application;
  av::vkproto::serialize_VkInstanceCreateInfo(writer, create);
  return writer.offset(); }

}  // namespace

/* An id the profile carries and one it does not. The device skips what a
 * backend does not know and faults what it cannot decode, so this is the line
 * between a guest using a command this build left out and a guest lying. */
TEST(vulkan_backend, knows_the_ids_the_profile_carries_and_no_others)
{ VulkanBackend backend;
  CHECK(backend.knows(av::vkproto::kCmd_vkCreateInstance));
  CHECK(backend.knows(av::vkproto::kCmd_vkDestroyInstance));
  /* An id inside Vulkan's range that the profile does not carry. */
  CHECK(!backend.knows(0x1FFFFu));
  /* And another API's range entirely, which the device should never send. */
  CHECK(!backend.knows(0x30001u));
  CHECK(!backend.knows(0u)); }

TEST(vulkan_backend, a_well_formed_command_decodes_and_answers)
{ VulkanBackend backend;
  std::uint8_t frame[1024];
  const std::size_t length = create_instance_frame(frame, sizeof frame);
  CHECK(length > 0u);

  std::uint8_t block[block_bytes];
  WireArena arena(block, sizeof block);
  std::uint8_t reply[256] = {};
  WireReader payload(frame, length);
  WireWriter answer(reply, sizeof reply);
  CHECK(backend.run(av::vkproto::kCmd_vkCreateInstance, payload, answer, arena));
  CHECK_EQ(backend.ran(), 1u);
  /* A VkResult and a handle come back, whatever the driver made of it. */
  CHECK_EQ(answer.offset(), 12u);
  CHECK(!backend.lost()); }

/* A frame that stops short is refused rather than read past: every truncation
 * of a real command, not just a convenient one. */
TEST(vulkan_backend, a_truncated_command_is_refused_at_every_length)
{ VulkanBackend backend;
  std::uint8_t frame[1024];
  const std::size_t whole = create_instance_frame(frame, sizeof frame);
  for (std::size_t length = 0u; length < whole; length++) {
      std::uint8_t block[block_bytes];
      WireArena arena(block, sizeof block);
      std::uint8_t reply[256] = {};
      WireReader payload(frame, length);
      WireWriter answer(reply, sizeof reply);
      CHECK(!backend.run(av::vkproto::kCmd_vkCreateInstance, payload, answer, arena));
  }
  /* Nothing that failed to decode counts as having run. */
  CHECK_EQ(backend.ran(), 0u); }

/* An id the backend does not know is never dispatched, because the session
 * asks first - but if one arrived anyway it must not decode. */
TEST(vulkan_backend, an_id_it_does_not_know_does_not_decode)
{ VulkanBackend backend;
  std::uint8_t block[block_bytes];
  WireArena arena(block, sizeof block);
  std::uint8_t frame[8] = {};
  std::uint8_t reply[16] = {};
  WireReader payload(frame, sizeof frame);
  WireWriter answer(reply, sizeof reply);
  CHECK(!backend.run(0x1FFFFu, payload, answer, arena));
  CHECK_EQ(backend.ran(), 0u); }

/* A decode that needs more room than the arena has is refused, and the arena
 * says it was the room rather than the frame. */
TEST(vulkan_backend, a_command_the_arena_will_not_hold_is_refused_and_counted)
{ VulkanBackend backend;
  std::uint8_t frame[1024];
  const std::size_t length = create_instance_frame(frame, sizeof frame);

  std::uint8_t block[8];
  WireArena arena(block, sizeof block);
  std::uint8_t reply[256] = {};
  WireReader payload(frame, length);
  WireWriter answer(reply, sizeof reply);
  CHECK(!backend.run(av::vkproto::kCmd_vkCreateInstance, payload, answer, arena));
  CHECK(arena.refusals() > 0u); }
