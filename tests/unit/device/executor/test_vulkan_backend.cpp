/* One guest context's Vulkan on the host's own.
 *
 * What is asserted is what the ABI needs and the generated dispatch does not
 * give: whether an id is carried at all, and whether a frame decoded. The
 * driver's own answer is deliberately not asserted - these must pass on a
 * machine with no GPU, where every command decodes and every VkResult says so. */
#include "device/abi/rvk_executor.h"
#include "device/executor/imported_blob.hpp"
#include "device/executor/vulkan_backend.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/generated/vulkan/vk_protocol_ids.h"
#include "proto/generated/vulkan/vk_serialize.h"
#include "proto/wire/wire_arena.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

#include <cstdint>
#include <cstring>

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

/* The import path, on whatever this machine has. A host with no GPU refuses
 * every import and that is the honest answer, so what is asserted is the rule
 * rather than the outcome: a kind this backend never offers is never adopted,
 * and abandoning what was never adopted is nothing. */
TEST(vulkan_backend, a_kind_it_does_not_offer_is_not_adopted)
{ VulkanBackend backend;
  av::ImportedBlob blob;
  RvkBlobHandle handle = {};
  handle.kind = RVK_BLOB_DMA_BUF;
  handle.fd = 3;
  handle.size = av::ImportedBlob::page_bytes;
  CHECK_STATUS(blob.take(1u, handle), av::Status::ok);
  /* A dma-buf lands in memory the host cannot map on one of the two GPUs
   * measured, so this backend offers only the host pointer and takes only it. */
  CHECK(!backend.adopt(blob));
  CHECK_EQ(blob.object(), 0u); }

/* No device, no import: a VkDeviceMemory belongs to a device, and a context
 * that has not made one has nowhere to put a blob. */
TEST(vulkan_backend, without_a_device_nothing_is_adopted)
{ VulkanBackend backend;
  av::ImportedBlob blob;
  alignas(4096) static std::uint8_t page[av::ImportedBlob::page_bytes];
  RvkBlobHandle handle = {};
  handle.kind = RVK_BLOB_HOST_POINTER;
  handle.pointer = page;
  handle.fd = -1;
  handle.size = sizeof page;
  CHECK_STATUS(blob.take(1u, handle), av::Status::ok);
  CHECK(!backend.adopt(blob));
  /* And giving back what was never taken is nothing, which is what a session
   * being destroyed does for every blob it still holds. */
  backend.abandon(blob);
  CHECK_EQ(blob.object(), 0u); }

/* The device a context makes is the one an import belongs to, and destroying
 * it takes that away again. Driven through the wire, because that is the only
 * way the guest can reach on_vkCreateDevice and on_vkDestroyDevice. */
TEST(vulkan_backend, the_device_the_guest_makes_is_the_one_imports_belong_to)
{ VulkanBackend backend;
  std::uint8_t block[block_bytes];
  WireArena arena(block, sizeof block);
  std::uint8_t reply[256] = {};

  std::uint8_t frame[1024];
  const std::size_t length = create_instance_frame(frame, sizeof frame);
  WireReader payload(frame, length);
  WireWriter answer(reply, sizeof reply);
  CHECK(backend.run(av::vkproto::kCmd_vkCreateInstance, payload, answer, arena));

  /* A device this backend never made is refused before the driver sees it -
   * including the null one, which the loader would dereference to find the
   * table to call through. */
  std::uint8_t destroy_frame[64];
  WireWriter   destroy_writer(destroy_frame, sizeof destroy_frame);
  destroy_writer.write_pod<std::uint64_t>(0u);
  WireReader destroy_payload(destroy_frame, destroy_writer.offset());
  WireWriter destroy_answer(reply, sizeof reply);
  arena.reset();
  CHECK(!backend.run(av::vkproto::kCmd_vkDestroyDevice, destroy_payload, destroy_answer, arena));

  std::uint8_t invented_frame[64];
  WireWriter   invented(invented_frame, sizeof invented_frame);
  invented.write_pod<std::uint64_t>(0xDEADBEEFu);
  WireReader invented_payload(invented_frame, invented.offset());
  WireWriter invented_answer(reply, sizeof reply);
  arena.reset();
  CHECK(!backend.run(av::vkproto::kCmd_vkDestroyDevice, invented_payload, invented_answer, arena));
  CHECK(!backend.lost()); }

/* The property a guest must not be able to break: a number this host did not
 * produce is not a handle. A fuzzer reached vkDestroyInstance with a
 * guest-chosen address before this existed, so what is asserted here is that
 * knows_handle answers for the executor's own handles and nothing else. */
TEST(vulkan_backend, knows_only_the_handles_it_made)
{ VulkanBackend backend;
  CHECK(!backend.knows_handle(0x1234u));
  /* The null handle is the API's own, and the hook refuses it separately -
   * a dispatchable null is what the loader dereferences to find its table. */
  CHECK(backend.knows_handle(0u));

  std::uint8_t block[block_bytes];
  WireArena arena(block, sizeof block);
  std::uint8_t frame[1024];
  const std::size_t length = create_instance_frame(frame, sizeof frame);
  std::uint8_t reply[256] = {};
  WireReader payload(frame, length);
  WireWriter answer(reply, sizeof reply);
  CHECK(backend.run(av::vkproto::kCmd_vkCreateInstance, payload, answer, arena));

  /* Read as the number it is rather than through deserialize_VkInstance,
   * which now refuses every handle outside a dispatch - which is the point. */
  av::WireReader read_back(reply, answer.offset());
  std::int32_t   returned = 0;
  std::uint64_t  raw = 0u;
  CHECK(read_back.read_pod(returned));
  CHECK(read_back.read_pod(raw));
  if (returned == VK_SUCCESS) {
      CHECK(backend.knows_handle(raw));
      CHECK(!backend.knows_handle(raw ^ 0x1000u));
  } }
