/* One guest context's Vulkan on the host's own.
 *
 * What is asserted is what the ABI needs and the generated dispatch does not
 * give: whether an id is carried at all, and whether a frame decoded. The
 * driver's own answer is deliberately not asserted - these must pass on a
 * machine with no GPU, where every command decodes and every VkResult says so. */
#include "device/abi/av_executor.h"
#include "device/executor/imported_blob.hpp"
#include "device/executor/handle_registry.hpp"
#include "device/executor/host_gpu.hpp"
#include "device/executor/vulkan_backend.hpp"
#include "device/shim/loader_host_apis.hpp"
#include "harness/executor/mock_host_apis.hpp"
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

/* This test makes real VkInstances, and the drivers behind them allocate
 * things they never give back - dbus connections inside the loader's ICD
 * enumeration, most of them. Those are named here rather than leak detection
 * being turned off, so a leak of this repository's own still fails. */
extern "C" const char *__lsan_default_suppressions(void);
extern "C" const char *__lsan_default_suppressions(void)
{
    return "leak:libdbus-1\n"
           "leak:libOpenCL\n"
           "leak:intelocl\n"
           "leak:libigdrcl\n"
           "leak:libEGL\n"
           "leak:libglapi\n"
           "leak:libvulkan\n"
           "leak:libnvidia\n"
           "leak:nvidia_icd\n"
           "leak:libGLX\n"
           "leak:iris_dri\n"
           "leak:libz\n";
}

namespace {

constexpr std::size_t block_bytes = 65536u;

/* VK_API_VERSION_1_0 and 1.1, spelled out: the macros are full of C casts
 * this build refuses. */
constexpr std::uint32_t api_1_0 = 1u << 22;
constexpr std::uint32_t api_1_1 = (1u << 22) | (1u << 12);

std::size_t create_instance_frame(std::uint8_t *frame, std::size_t capacity,
                                  std::uint32_t api_version = api_1_1)
{ WireWriter writer(frame, capacity);
  VkApplicationInfo application = {};
  application.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  application.pApplicationName = "test";
  application.apiVersion = api_version;
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
  CHECK(!backend.lost());
  backend.shut_down(); }

/* A version below 1.0 - major zero - is not a version, and the loader only
 * warns about it: the layers behind it then read state no create ever made and
 * the host process dies, which no guest may be able to do. Refused here, and
 * the two versions that are legitimate still pass (T064, found by
 * tests/fuzz/fuzz_executor_vulkan.cpp). */
TEST(vulkan_backend, an_instance_asking_for_a_version_below_one_zero_is_refused)
{ constexpr std::uint32_t refused[] = {1u, api_1_0 - 1u, 65664u};
  for (const std::uint32_t version : refused) {
      VulkanBackend backend;
      std::uint8_t  frame[1024];
      const std::size_t length = create_instance_frame(frame, sizeof frame, version);
      std::uint8_t  block[block_bytes];
      WireArena     arena(block, sizeof block);
      std::uint8_t  reply[256] = {};
      WireReader    payload(frame, length);
      WireWriter    answer(reply, sizeof reply);
      CHECK(backend.run(av::vkproto::kCmd_vkCreateInstance, payload, answer, arena));
      std::int32_t result = 0;
      std::memcpy(&result, reply, sizeof result);
      CHECK_EQ(result, static_cast<std::int32_t>(VK_ERROR_INCOMPATIBLE_DRIVER));
      /* Nothing was made, so nothing is a handle the guest may name back. */
      std::uint64_t made = 0u;
      std::memcpy(&made, reply + sizeof result, sizeof made);
      CHECK_EQ(made, 0u);
      CHECK(!backend.lost());
      backend.shut_down();
  }
  /* Zero is the application saying it asks for nothing in particular, and 1.0
   * is the lowest that is a version at all: both reach the driver. */
  constexpr std::uint32_t allowed[] = {0u, api_1_0};
  for (const std::uint32_t version : allowed) {
      VulkanBackend backend;
      std::uint8_t  frame[1024];
      const std::size_t length = create_instance_frame(frame, sizeof frame, version);
      std::uint8_t  block[block_bytes];
      WireArena     arena(block, sizeof block);
      std::uint8_t  reply[256] = {};
      WireReader    payload(frame, length);
      WireWriter    answer(reply, sizeof reply);
      CHECK(backend.run(av::vkproto::kCmd_vkCreateInstance, payload, answer, arena));
      std::int32_t result = 0;
      std::memcpy(&result, reply, sizeof result);
      CHECK(result != static_cast<std::int32_t>(VK_ERROR_INCOMPATIBLE_DRIVER));
      backend.shut_down();
  } }

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
  AvBlobHandle handle = {};
  handle.kind = AV_BLOB_DMA_BUF;
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
  AvBlobHandle handle = {};
  handle.kind = AV_BLOB_HOST_POINTER;
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
  CHECK(!backend.lost());
  backend.shut_down(); }

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
  }
  backend.shut_down(); }

/* A context that dies still owning its instance must take it with it. A guest
 * can create and abandon contexts as often as it likes, and a host object left
 * behind each time is a leak it controls. The fuzz target found this one. */
TEST(vulkan_backend, shutting_down_gives_back_what_the_guest_never_destroyed)
{ VulkanBackend backend;
  std::uint8_t block[block_bytes];
  WireArena arena(block, sizeof block);
  std::uint8_t frame[1024];
  const std::size_t length = create_instance_frame(frame, sizeof frame);
  std::uint8_t reply[256] = {};
  WireReader payload(frame, length);
  WireWriter answer(reply, sizeof reply);
  CHECK(backend.run(av::vkproto::kCmd_vkCreateInstance, payload, answer, arena));

  av::WireReader read_back(reply, answer.offset());
  std::int32_t   returned = 0;
  std::uint64_t  raw = 0u;
  CHECK(read_back.read_pod(returned));
  CHECK(read_back.read_pod(raw));

  backend.shut_down();
  /* Nothing it made is still its own, so a handle from before is refused the
   * way any other guest-chosen number is. */
  CHECK(!backend.knows_handle(raw));
  CHECK_EQ(backend.ran(), 0u);
  CHECK(!backend.lost());
  /* And shutting down twice is not a second destroy. */
  backend.shut_down();
  CHECK(!backend.knows_handle(raw)); }

/* One archive serves the host and the guest. On the host a backend is always
 * deciding, because run() sets it around every dispatch; outside one, the
 * numbers are this process's own and identity is the whole of the mapping,
 * which is what the guest's ICD needs. What must not happen is the checking
 * going missing where it is meant to be. */
TEST(vulkan_backend, outside_a_dispatch_a_handle_is_this_process_s_own)
{ CHECK(av::VulkanBackend::deciding() == nullptr);
  av::WireWriter writer(nullptr, 0u);
  std::uint8_t frame[16];
  av::WireWriter handle_writer(frame, sizeof frame);
  handle_writer.write_pod<std::uint64_t>(0xDEADBEEFu);
  av::WireReader reader(frame, handle_writer.offset());
  VkDevice device = VK_NULL_HANDLE;
  CHECK(av::vkproto::deserialize_VkDevice(reader, device));
  std::uint64_t raw = 0u;
  std::memcpy(&raw, &device, sizeof device);
  CHECK_EQ(raw, 0xDEADBEEFu);
  (void)writer; }

/* And inside one it is refused, which is the same call with a backend set. */
TEST(vulkan_backend, inside_a_dispatch_a_handle_it_never_made_is_refused)
{ VulkanBackend backend;
  std::uint8_t block[block_bytes];
  WireArena arena(block, sizeof block);
  std::uint8_t frame[16];
  WireWriter handle_writer(frame, sizeof frame);
  handle_writer.write_pod<std::uint64_t>(0xDEADBEEFu);
  std::uint8_t reply[64] = {};
  WireReader payload(frame, handle_writer.offset());
  WireWriter answer(reply, sizeof reply);
  CHECK(!backend.run(av::vkproto::kCmd_vkDestroyDevice, payload, answer, arena));
  CHECK(av::VulkanBackend::deciding() == nullptr); }

/* A registry that cannot remember a handle it is about to hand out loses the
 * device rather than hand out one it would refuse when the guest sent it back. */
TEST(vulkan_backend, a_handle_it_cannot_remember_loses_the_device)
{ VulkanBackend backend;
  for (std::uint64_t raw = 1u; raw <= av::HandleRegistry::max_handles; raw++) backend.made(raw);
  CHECK(!backend.lost());
  backend.made(av::HandleRegistry::max_handles + 1u);
  CHECK(backend.lost());
  backend.shut_down();
  CHECK(!backend.lost()); }

/* Outside a dispatch nobody is deciding, in both directions: a handle goes out
 * as the number it is without being remembered, and a non-dispatchable one comes
 * back as the number it was. That is the guest side of one shared archive. */
TEST(vulkan_backend, outside_a_dispatch_handles_cross_as_numbers)
{ std::uint8_t frame[16];
  WireWriter writer(frame, sizeof frame);
  VkBuffer buffer = VK_NULL_HANDLE;
  std::uint64_t raw = 0xABCDu;
  std::memcpy(&buffer, &raw, sizeof buffer);
  av::vkproto::serialize_VkBuffer(writer, buffer);
  CHECK_EQ(writer.offset(), 8u);

  WireReader reader(frame, writer.offset());
  VkBuffer back = VK_NULL_HANDLE;
  CHECK(av::vkproto::deserialize_VkBuffer(reader, back));
  std::uint64_t returned = 0u;
  std::memcpy(&returned, &back, sizeof back);
  CHECK_EQ(returned, 0xABCDu); }

/* A create the driver refuses records nothing: there is no instance to destroy
 * later. A layer that does not exist is how to ask for one. */
TEST(vulkan_backend, an_instance_the_driver_refuses_is_not_kept)
{ VulkanBackend backend;
  std::uint8_t frame[1024];
  WireWriter writer(frame, sizeof frame);
  VkInstanceCreateInfo create = {};
  create.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  const char *layers[] = {"VK_LAYER_accel_virt_no_such_layer"};
  create.enabledLayerCount = 1u;
  create.ppEnabledLayerNames = layers;
  av::vkproto::serialize_VkInstanceCreateInfo(writer, create);

  std::uint8_t block[block_bytes];
  WireArena arena(block, sizeof block);
  std::uint8_t reply[64] = {};
  WireReader payload(frame, writer.offset());
  WireWriter answer(reply, sizeof reply);
  CHECK(backend.run(av::vkproto::kCmd_vkCreateInstance, payload, answer, arena));
  WireReader read(reply, answer.offset());
  std::int32_t returned = 0;
  CHECK(read.read_pod(returned));
  CHECK(returned != 0);
  backend.shut_down(); }

namespace {

/* An instance made through the wire, so the backend made and remembers it,
 * with the API version the guest's own clients ask for or 1.0, which has no
 * vkGetPhysicalDeviceProperties2 of its own. Null when the loader refuses. */
VkInstance instance_through(VulkanBackend &backend, bool vulkan_1_1, std::uint32_t extension_count = 0u,
                            const char *const *extensions = nullptr)
{ std::uint8_t frame[8192];
  WireWriter writer(frame, sizeof frame);
  VkApplicationInfo application = {};
  application.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  application.apiVersion = vulkan_1_1 ? (1u << 22) | (1u << 12) : (1u << 22);
  VkInstanceCreateInfo create = {};
  create.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  create.pApplicationInfo = &application;
  create.enabledExtensionCount = extension_count;
  create.ppEnabledExtensionNames = extensions;
  av::vkproto::serialize_VkInstanceCreateInfo(writer, create);
  std::uint8_t block[block_bytes];
  WireArena arena(block, sizeof block);
  std::uint8_t reply[64] = {};
  WireReader payload(frame, writer.offset());
  WireWriter answer(reply, sizeof reply);
  if (!backend.run(av::vkproto::kCmd_vkCreateInstance, payload, answer, arena)) return VK_NULL_HANDLE;
  WireReader read(reply, answer.offset());
  std::int32_t returned = -1;
  std::uint64_t raw = 0u;
  (void)read.read_pod(returned);
  (void)read.read_pod(raw);
  VkInstance instance = VK_NULL_HANDLE;
  if (returned == 0) std::memcpy(&instance, &raw, sizeof instance);
  return instance; }

std::uint64_t raw_of(VkPhysicalDevice device)
{ std::uint64_t raw = 0u;
  std::memcpy(&raw, &device, sizeof device);
  return raw; }

constexpr std::uint8_t nobody_s_gpu[16] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
                                           0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

const VkPhysicalDevice sentinel = reinterpret_cast<VkPhysicalDevice>(static_cast<std::uintptr_t>(0x5E5Eu));

}  // namespace

/* A context bound to one of this host's GPUs is shown that GPU's physical
 * device and no other, by its own UUID, as a list and as a group; the host's
 * other GPUs are handles it was never given and cannot name. Both API versions
 * a guest might create its instance with. A host with no GPU has nothing to
 * judge. */
TEST(vulkan_backend, a_bound_context_lists_its_gpu_alone)
{ av::LoaderHostApis host;
  host.open();
  for (std::uint32_t i = 0u; i < host.gpu_count(); i++) {
      av::HostGpu gpu;
      gpu.describe(host, i);
      constexpr bool versions[] = {true, false};
      for (const bool vulkan_1_1 : versions) {
          VulkanBackend backend;
          backend.initialize(gpu);
          const VkInstance instance = instance_through(backend, vulkan_1_1);
          CHECK(instance != VK_NULL_HANDLE);
          if (instance == VK_NULL_HANDLE) continue;

          std::uint32_t count = 7u;
          CHECK_EQ(backend.on_vkEnumeratePhysicalDevices(instance, &count, nullptr), VK_SUCCESS);
          CHECK_EQ(count, 1u);
          VkPhysicalDevice listed[3] = {sentinel, sentinel, sentinel};
          count = 3u;
          CHECK_EQ(backend.on_vkEnumeratePhysicalDevices(instance, &count, listed), VK_SUCCESS);
          CHECK_EQ(count, 1u);
          CHECK(listed[1] == sentinel);
          VkPhysicalDeviceIDProperties identity = {};
          identity.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES;
          VkPhysicalDeviceProperties2 properties = {};
          properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
          properties.pNext = &identity;
          ::vkGetPhysicalDeviceProperties2(listed[0], &properties);
          CHECK(gpu.matches(identity.deviceUUID));

          VkPhysicalDevice none[1] = {sentinel};
          count = 0u;
          CHECK_EQ(backend.on_vkEnumeratePhysicalDevices(instance, &count, none), VK_INCOMPLETE);
          CHECK_EQ(count, 0u);
          CHECK(none[0] == sentinel);

          count = 7u;
          CHECK_EQ(backend.on_vkEnumeratePhysicalDeviceGroups(instance, &count, nullptr), VK_SUCCESS);
          CHECK_EQ(count, 1u);
          VkPhysicalDeviceGroupProperties groups[2] = {};
          groups[0].sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GROUP_PROPERTIES;
          groups[0].physicalDeviceCount = 5u;
          groups[0].subsetAllocation = VK_TRUE;
          groups[1].physicalDeviceCount = 5u;
          count = 2u;
          CHECK_EQ(backend.on_vkEnumeratePhysicalDeviceGroupsKHR(instance, &count, groups), VK_SUCCESS);
          CHECK_EQ(count, 1u);
          CHECK_EQ(groups[0].sType, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GROUP_PROPERTIES);
          CHECK_EQ(groups[0].physicalDeviceCount, 1u);
          CHECK(groups[0].physicalDevices[0] == listed[0]);
          CHECK_EQ(groups[0].subsetAllocation, VkBool32{VK_FALSE});
          CHECK_EQ(groups[1].physicalDeviceCount, 5u);
          count = 0u;
          CHECK_EQ(backend.on_vkEnumeratePhysicalDeviceGroups(instance, &count, groups), VK_INCOMPLETE);
          CHECK_EQ(count, 0u);

          /* Through the wire: what the guest was shown it can name, and the
           * host's other GPUs it cannot. */
          std::uint8_t frame[32];
          WireWriter writer(frame, sizeof frame);
          std::uint64_t instance_raw = 0u;
          std::memcpy(&instance_raw, &instance, sizeof instance);
          writer.write_pod(instance_raw);
          writer.write_pod<std::uint8_t>(1u);
          writer.write_pod<std::uint32_t>(4u);
          std::uint8_t block[block_bytes];
          WireArena arena(block, sizeof block);
          std::uint8_t reply[64] = {};
          WireReader payload(frame, writer.offset());
          WireWriter answer(reply, sizeof reply);
          CHECK(backend.run(av::vkproto::kCmd_vkEnumeratePhysicalDevices, payload, answer, arena));
          CHECK(backend.knows_handle(raw_of(listed[0])));
          VkPhysicalDevice every[16] = {};
          std::uint32_t all = 16u;
          CHECK(::vkEnumeratePhysicalDevices(instance, &all, every) >= 0);
          CHECK_EQ(all, host.gpu_count());
          for (std::uint32_t d = 0u; d < all; d++) {
              if (every[d] != listed[0]) CHECK(!backend.knows_handle(raw_of(every[d])));
          }
          backend.shut_down();
      }
  }
  host.close(); }

/* A context bound to a GPU its instance does not list, and one never bound,
 * are shown nothing - an empty list and no group - rather than some other GPU. */
TEST(vulkan_backend, a_context_whose_gpu_is_not_listed_lists_nothing)
{ av::MockHostApis apis;
  apis.add_gpu(nobody_s_gpu, AV_EXECUTOR_SUB_API_VULKAN, 0u, 0u, 0u);
  av::HostGpu gpu;
  gpu.describe(apis, 0u);
  VulkanBackend bound;
  bound.initialize(gpu);
  VulkanBackend unbound;
  VulkanBackend *const backends[] = {&bound, &unbound};
  for (VulkanBackend *const backend : backends) {
      const VkInstance instance = instance_through(*backend, true);
      if (instance == VK_NULL_HANDLE) continue;
      std::uint32_t count = 7u;
      CHECK_EQ(backend->on_vkEnumeratePhysicalDevices(instance, &count, nullptr), VK_SUCCESS);
      CHECK_EQ(count, 0u);
      VkPhysicalDevice listed[2] = {sentinel, sentinel};
      count = 2u;
      CHECK_EQ(backend->on_vkEnumeratePhysicalDevices(instance, &count, listed), VK_SUCCESS);
      CHECK_EQ(count, 0u);
      CHECK(listed[0] == sentinel);
      count = 7u;
      CHECK_EQ(backend->on_vkEnumeratePhysicalDeviceGroups(instance, &count, nullptr), VK_SUCCESS);
      CHECK_EQ(count, 0u);
      VkPhysicalDeviceGroupProperties groups[1] = {};
      groups[0].physicalDeviceCount = 5u;
      count = 1u;
      CHECK_EQ(backend->on_vkEnumeratePhysicalDeviceGroups(instance, &count, groups), VK_SUCCESS);
      CHECK_EQ(count, 0u);
      CHECK_EQ(groups[0].physicalDeviceCount, 5u);
      backend->shut_down();
  } }

/* A guest's Vulkan 1.0 instance is made with the extension that lets its
 * physical devices say which GPU they are, so it is shown its GPU too; up to a
 * list with no room left for it, which is shown none rather than written past.
 * A list the loader refuses - names counted and not sent - is refused as it
 * would have been. The loader accepts a name twice, which is what fills a list. */
TEST(vulkan_backend, a_vulkan_1_0_instance_is_made_able_to_name_its_gpu)
{ av::LoaderHostApis host;
  host.open();
  if (host.gpu_count() == 0u) { host.close(); return; }
  av::HostGpu gpu;
  gpu.describe(host, 0u);
  static const char *surfaces[256];
  for (const char *&name : surfaces) name = "VK_KHR_surface";
  const char *const identity[] = {"VK_KHR_get_physical_device_properties2", "VK_KHR_surface"};

  struct Case { std::uint32_t count; const char *const *names; bool made; std::uint32_t listed; };
  const Case cases[] = {{255u, surfaces, true, 1u},
                        {256u, surfaces, true, 1u},
                        {2u, identity, true, 1u},
                        {3u, nullptr, false, 0u}};
  for (const Case &c : cases) {
      VulkanBackend backend;
      backend.initialize(gpu);
      const VkInstance instance = instance_through(backend, false, c.count, c.names);
      CHECK_EQ(instance != VK_NULL_HANDLE, c.made);
      if (instance != VK_NULL_HANDLE) {
          std::uint32_t count = 7u;
          CHECK_EQ(backend.on_vkEnumeratePhysicalDevices(instance, &count, nullptr), VK_SUCCESS);
          CHECK_EQ(count, c.listed);
      }
      backend.shut_down();
  }
  host.close(); }

/* Outside a context's own decode there is no backend deciding, and nothing is
 * sized: a hook answering some other context's length would be worse than
 * answering none (feature 047). */
TEST(vulkan_backend, with_no_context_deciding_nothing_is_sized)
{ CHECK_EQ(av::vkproto::descriptor_template_bytes(reinterpret_cast<VkDescriptorUpdateTemplate>(0x4242u)), 0u);
  CHECK_EQ(av::vkproto::descriptor_template_bytes(VK_NULL_HANDLE), 0u); }

/* A template a guest never created sizes no blob, so an update naming it is
 * refused unless it carries none. What the backend answers here is what the
 * generated decoder checks the guest's own length against (feature 047,
 * contracts/descriptor_templates.md); it must not need a GPU to answer. */
TEST(vulkan_backend, a_template_it_never_made_sizes_no_blob)
{ VulkanBackend backend;
  CHECK_EQ(backend.template_bytes(VK_NULL_HANDLE), 0u);
  CHECK_EQ(backend.template_bytes(reinterpret_cast<VkDescriptorUpdateTemplate>(0x4242u)), 0u);
  /* An update naming that template, carrying one image descriptor: the length
   * the guest wrote is not the length the backend accounts for, so the frame
   * does not decode. */
  std::uint8_t frame[256] = {};
  WireWriter   writer(frame, sizeof frame);
  writer.write_u64(0x1u);                                  /* the device */
  writer.write_u64(0x2u);                                  /* the descriptor set */
  writer.write_u64(0x4242u);                               /* the template */
  writer.write_varint(sizeof(VkDescriptorImageInfo));      /* a blob it did not size */
  for (std::size_t at = 0u; at < sizeof(VkDescriptorImageInfo); at++) writer.write_u8(0u);
  std::uint8_t block[block_bytes] = {};
  WireArena    arena(block, sizeof block);
  WireReader   reader(frame, writer.offset());
  std::uint8_t reply_bytes[64] = {};
  WireWriter   reply(reply_bytes, sizeof reply_bytes);
  CHECK(!backend.run(static_cast<std::uint32_t>(av::vkproto::kCmd_vkUpdateDescriptorSetWithTemplate), reader, reply,
                     arena)); }

/* Feature 053: a backend with no device of its own shares nothing. Every
 * shared-object command reaches a backend whether or not its guest ever made a
 * device, so the answer has to be a refusal and not a call into a null. */
TEST(vulkan_backend, a_backend_with_no_device_exports_imports_and_releases_nothing)
{ VulkanBackend backend;
  std::uint32_t handle_kind = 9u;
  int           fd = 9;
  CHECK(!backend.export_object(AV_SHARED_MEMORY, 0x40u, handle_kind, fd));
  CHECK_EQ(handle_kind, 0u);
  CHECK_EQ(fd, -1);
  CHECK(!backend.export_object(AV_SHARED_MONITORED_FENCE, 0x40u, handle_kind, fd));

  std::uint64_t made = 9u;
  CHECK(!backend.import_object(AV_SHARED_MEMORY, AV_SHARED_HANDLE_DMA_BUF, 3, 4096u, made));
  CHECK_EQ(made, 0u);
  CHECK(!backend.import_object(AV_SHARED_MONITORED_FENCE, AV_SHARED_HANDLE_OPAQUE_FD, 3, 0u, made));
  /* And a release does nothing at all, with or without a handle. */
  backend.release_object(AV_SHARED_MEMORY, 0x40u);
  backend.release_object(AV_SHARED_MONITORED_FENCE, 0u); }

namespace {

void never_called(void *) {}

}  // namespace

/* Its commands finish as they run: a wake is nothing it needs, and binding
 * one leaves it waiting on no GPU. */
TEST(vulkan_backend, a_signal_changes_nothing_it_waits_on)
{ VulkanBackend backend;
  backend.bind_signal(&never_called, nullptr);
  CHECK(!backend.poll());
  std::uint64_t marked = 7u, reached = 7u;
  backend.gpu_progress(marked, reached);
  CHECK_EQ(marked, 0u);
  CHECK_EQ(reached, 0u); }
