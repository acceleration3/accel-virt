#include "core/log/driver_log.hpp"
#include "device/abi/av_executor.h"
#include "executor/executor_log.hpp"
#include "executor/import_table.hpp"
#include "executor/owned_fd.hpp"
#include "executor/exported_memory.hpp"
#include "executor/source_memory.hpp"
#include "executor/host_gpu.hpp"
#include "executor/host_memory_source.hpp"
#include "executor/vulkan/guest_memory_types.hpp"
#include "executor/vram_ledger.hpp"
#include "executor/vulkan/vulkan_backend.hpp"
#include "executor/shim/loader_host_apis.hpp"
#include "harness/executor/mock_host_apis.hpp"
#include "harness/executor/mock_memory_exporter.hpp"
#include "harness/platform/mock_platform.hpp"
#include "harness/executor/vulkan_backend_probe.hpp"
#include "executor/sharing/shared_image_shape.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/icd/recording_log.hpp"
#include "proto/generated/vulkan/vk_protocol_ids.h"
#include "proto/submission/blob_role.hpp"
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
std::uint64_t detail = 0u;

struct Rig {
    Rig() : memory(), imports(memory), sink(), lines(sink, "vulkan: ") {}

    av::MockPlatform memory;
    av::ImportTable  imports;
    av::RecordingLog sink;
    av::DriverLog    lines;
};

constexpr std::size_t block_bytes = 65536u;

av::vkproto::HandleIdentity identity_translation;

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
  av::vkproto::serialize_VkInstanceCreateInfo(writer, identity_translation, create);
  return writer.offset(); }

}

TEST(vulkan_backend, knows_the_ids_the_profile_carries_and_no_others)
{ Rig rig;
  VulkanBackend backend(rig.lines);
  CHECK(backend.knows(av::vkproto::kCmd_vkCreateInstance));
  CHECK(backend.knows(av::vkproto::kCmd_vkDestroyInstance));
  CHECK(!backend.knows(0x1FFFFu));
  CHECK(!backend.knows(0x30001u));
  CHECK(!backend.knows(0u)); }

TEST(vulkan_backend, a_well_formed_command_decodes_and_answers)
{ Rig rig;
  VulkanBackend backend(rig.lines);
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
  CHECK_EQ(answer.offset(), 12u);
  CHECK(!backend.lost());
  backend.shut_down(); }

TEST(vulkan_backend, an_instance_asking_for_a_version_below_one_zero_is_refused)
{ constexpr std::uint32_t refused[] = {1u, api_1_0 - 1u, 65664u};
  for (const std::uint32_t version : refused) {
      Rig rig;
      VulkanBackend backend(rig.lines);
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
      std::uint64_t made = 0u;
      std::memcpy(&made, reply + sizeof result, sizeof made);
      CHECK_EQ(made, 0u);
      CHECK(!backend.lost());
      backend.shut_down();
  }
  constexpr std::uint32_t allowed[] = {0u, api_1_0};
  for (const std::uint32_t version : allowed) {
      Rig rig;
      VulkanBackend backend(rig.lines);
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

TEST(vulkan_backend, a_truncated_command_is_refused_at_every_length)
{ Rig rig;
  VulkanBackend backend(rig.lines);
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
  CHECK_EQ(backend.ran(), 0u); }

TEST(vulkan_backend, an_id_it_does_not_know_does_not_decode)
{ Rig rig;
  VulkanBackend backend(rig.lines);
  std::uint8_t block[block_bytes];
  WireArena arena(block, sizeof block);
  std::uint8_t frame[8] = {};
  std::uint8_t reply[16] = {};
  WireReader payload(frame, sizeof frame);
  WireWriter answer(reply, sizeof reply);
  CHECK(!backend.run(0x1FFFFu, payload, answer, arena));
  CHECK_EQ(backend.ran(), 0u); }

TEST(vulkan_backend, a_command_the_arena_will_not_hold_is_refused_and_counted)
{ Rig rig;
  VulkanBackend backend(rig.lines);
  std::uint8_t frame[1024];
  const std::size_t length = create_instance_frame(frame, sizeof frame);

  std::uint8_t block[8];
  WireArena arena(block, sizeof block);
  std::uint8_t reply[256] = {};
  WireReader payload(frame, length);
  WireWriter answer(reply, sizeof reply);
  CHECK(!backend.run(av::vkproto::kCmd_vkCreateInstance, payload, answer, arena));
  CHECK(arena.refusals() > 0u); }

TEST(vulkan_backend, without_a_device_nothing_is_adopted)
{ Rig rig;
  VulkanBackend backend(rig.lines);
  av::HostGpu gpu;
  backend.initialize(gpu, nullptr, rig.imports);
  alignas(4096) static std::uint8_t page[4096];
  av::SourceMemory memory(1u, 1u, 0u, 1u, av::ExportedMemory(0x10u, -1, page, sizeof page));
  std::uint64_t handle = 9u;
  CHECK(!backend.adopt(memory, av::BlobRole(), handle));
  CHECK_EQ(handle, 0u);
  backend.abandon(memory, 0x10u);
  CHECK(av::VulkanBackendProbe(backend).imports_held() == 0u);
  backend.shut_down(); }

TEST(vulkan_backend, the_device_the_guest_makes_is_the_one_imports_belong_to)
{ Rig rig;
  VulkanBackend backend(rig.lines);
  std::uint8_t block[block_bytes];
  WireArena arena(block, sizeof block);
  std::uint8_t reply[256] = {};

  std::uint8_t frame[1024];
  const std::size_t length = create_instance_frame(frame, sizeof frame);
  WireReader payload(frame, length);
  WireWriter answer(reply, sizeof reply);
  CHECK(backend.run(av::vkproto::kCmd_vkCreateInstance, payload, answer, arena));

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

TEST(vulkan_backend, knows_only_the_handles_it_made)
{ Rig rig;
  VulkanBackend backend(rig.lines);
  CHECK(!backend.knows_handle(0x1234u));
  CHECK(backend.knows_handle(0u));

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
  if (returned == VK_SUCCESS) {
      CHECK(backend.knows_handle(raw));
      CHECK(!backend.knows_handle(raw ^ 0x1000u));
  }
  backend.shut_down(); }

TEST(vulkan_backend, shutting_down_gives_back_what_the_guest_never_destroyed)
{ Rig rig;
  VulkanBackend backend(rig.lines);
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
  CHECK(!backend.knows_handle(raw));
  CHECK_EQ(backend.ran(), 0u);
  CHECK(!backend.lost());
  backend.shut_down();
  CHECK(!backend.knows_handle(raw)); }

TEST(vulkan_backend, an_identity_translation_reads_a_handle_as_the_number_it_is)
{ std::uint8_t frame[16];
  av::WireWriter handle_writer(frame, sizeof frame);
  handle_writer.write_pod<std::uint64_t>(0xDEADBEEFu);
  av::WireReader reader(frame, handle_writer.offset());
  VkDevice device = VK_NULL_HANDLE;
  CHECK(av::vkproto::deserialize_VkDevice(reader, identity_translation, device));
  std::uint64_t raw = 0u;
  std::memcpy(&raw, &device, sizeof device);
  CHECK_EQ(raw, 0xDEADBEEFu); }

TEST(vulkan_backend, inside_a_dispatch_a_handle_it_never_made_is_refused)
{ Rig rig;
  VulkanBackend backend(rig.lines);
  std::uint8_t block[block_bytes];
  WireArena arena(block, sizeof block);
  std::uint8_t frame[16];
  WireWriter handle_writer(frame, sizeof frame);
  handle_writer.write_pod<std::uint64_t>(0xDEADBEEFu);
  std::uint8_t reply[64] = {};
  WireReader payload(frame, handle_writer.offset());
  WireWriter answer(reply, sizeof reply);
  CHECK(!backend.run(av::vkproto::kCmd_vkDestroyDevice, payload, answer, arena));
  CHECK(!backend.lost()); }

TEST(vulkan_backend, a_handle_it_made_is_known_by_its_kind_until_it_ends)
{ Rig rig;
  VulkanBackend backend(rig.lines);
  CHECK(backend.room_for(4u));
  backend.made(av::vkproto::HandleKind::physical_device, 0x100u);
  backend.made(av::vkproto::HandleKind::queue, 0x200u);
  std::uint64_t handle = 0u;
  CHECK(backend.knows_handle(0x100u));
  CHECK(backend.knows_handle(0x100u, av::VulkanObjectKind::physical_device));
  CHECK(!backend.knows_handle(0x100u, av::VulkanObjectKind::queue));
  CHECK(backend.handle_for(av::vkproto::HandleKind::queue, 0x200u, handle));
  CHECK_EQ(handle, 0x200u);
  CHECK(!backend.handle_for(av::vkproto::HandleKind::physical_device, 0x200u, handle));
  CHECK(!backend.handle_for(av::vkproto::HandleKind::queue, 0x300u, handle));
  backend.ended(0x200u);
  CHECK(!backend.knows_handle(0x200u));
  CHECK(backend.knows_handle(0x100u));
  backend.shut_down();
  CHECK(!backend.knows_handle(0x100u)); }

TEST(vulkan_backend, a_handle_made_through_a_parent_ends_with_the_parent_s_children)
{ Rig rig;
  VulkanBackend backend(rig.lines);
  backend.made(av::vkproto::HandleKind::physical_device, 0x200u);
  backend.made_through(0x200u);
  backend.made(av::vkproto::HandleKind::queue, 0x201u);
  backend.made(av::vkproto::HandleKind::command_buffer, 0x202u);
  backend.made_through(0u);
  CHECK(backend.knows_handle(0x201u, av::VulkanObjectKind::queue));
  CHECK(backend.knows_handle(0x202u, av::VulkanObjectKind::command_buffer));
  backend.children_ended(0x200u);
  CHECK(backend.knows_handle(0x200u));
  CHECK(!backend.knows_handle(0x201u));
  CHECK(!backend.knows_handle(0x202u));
  backend.shut_down(); }

TEST(vulkan_backend, it_makes_room_for_a_reply_s_handles_or_says_it_cannot)
{ Rig rig;
  VulkanBackend backend(rig.lines);
  CHECK(backend.room_for(0u));
  CHECK(backend.room_for(1000u));
  CHECK(!backend.room_for(~std::uint64_t{0}));
  CHECK(!backend.lost());
  CHECK(av::VulkanBackendProbe(backend).handles().invariants_hold()); }

TEST(vulkan_backend, an_identity_translation_writes_handles_as_numbers)
{ std::uint8_t frame[16];
  WireWriter writer(frame, sizeof frame);
  VkBuffer buffer = VK_NULL_HANDLE;
  std::uint64_t raw = 0xABCDu;
  std::memcpy(&buffer, &raw, sizeof buffer);
  av::vkproto::serialize_VkBuffer(writer, identity_translation, buffer);
  CHECK_EQ(writer.offset(), 8u);

  WireReader reader(frame, writer.offset());
  VkBuffer back = VK_NULL_HANDLE;
  CHECK(av::vkproto::deserialize_VkBuffer(reader, identity_translation, back));
  std::uint64_t returned = 0u;
  std::memcpy(&returned, &back, sizeof back);
  CHECK_EQ(returned, 0xABCDu); }

TEST(vulkan_backend, an_instance_the_driver_refuses_is_not_kept)
{ Rig rig;
  VulkanBackend backend(rig.lines);
  std::uint8_t frame[1024];
  WireWriter writer(frame, sizeof frame);
  VkInstanceCreateInfo create = {};
  create.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  const char *layers[] = {"VK_LAYER_accel_virt_no_such_layer"};
  create.enabledLayerCount = 1u;
  create.ppEnabledLayerNames = layers;
  av::vkproto::serialize_VkInstanceCreateInfo(writer, identity_translation, create);

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
  av::vkproto::serialize_VkInstanceCreateInfo(writer, identity_translation, create);
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

}

TEST(vulkan_backend, a_bound_context_lists_its_gpu_alone)
{ av::RecordingLog   host_sink;
  av::ExecutorLog    host_log(host_sink);
  av::LoaderHostApis host(host_log);
  host.open();
  for (std::uint32_t i = 0u; i < host.gpu_count(); i++) {
      av::HostGpu gpu;
      gpu.describe(host, i);
      constexpr bool versions[] = {true, false};
      for (const bool vulkan_1_1 : versions) {
          Rig rig;
          VulkanBackend backend(rig.lines);
          backend.initialize(gpu, nullptr, rig.imports);
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

TEST(vulkan_backend, a_context_whose_gpu_is_not_listed_lists_nothing)
{ av::MockHostApis apis;
  apis.add_gpu(nobody_s_gpu, AV_EXECUTOR_SUB_API_VULKAN);
  av::HostGpu gpu;
  gpu.describe(apis, 0u);
  Rig rig;
  VulkanBackend bound(rig.lines);
  bound.initialize(gpu, nullptr, rig.imports);
  Rig bare;
  VulkanBackend unbound(bare.lines);
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

TEST(vulkan_backend, a_vulkan_1_0_instance_is_made_able_to_name_its_gpu)
{ av::RecordingLog   host_sink;
  av::ExecutorLog    host_log(host_sink);
  av::LoaderHostApis host(host_log);
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
      Rig rig;
      VulkanBackend backend(rig.lines);
      backend.initialize(gpu, nullptr, rig.imports);
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

TEST(vulkan_backend, an_identity_translation_sizes_no_template)
{ CHECK_EQ(identity_translation.template_bytes(reinterpret_cast<VkDescriptorUpdateTemplate>(0x4242u)), 0u);
  CHECK_EQ(identity_translation.template_bytes(VK_NULL_HANDLE), 0u); }

TEST(vulkan_backend, a_template_it_never_made_sizes_no_blob)
{ Rig rig;
  VulkanBackend backend(rig.lines);
  CHECK_EQ(backend.template_bytes(VK_NULL_HANDLE), 0u);
  CHECK_EQ(backend.template_bytes(reinterpret_cast<VkDescriptorUpdateTemplate>(0x4242u)), 0u);
  std::uint8_t frame[256] = {};
  WireWriter   writer(frame, sizeof frame);
  writer.write_u64(0x1u);
  writer.write_u64(0x2u);
  writer.write_u64(0x4242u);
  writer.write_varint(sizeof(VkDescriptorImageInfo));
  for (std::size_t at = 0u; at < sizeof(VkDescriptorImageInfo); at++) writer.write_u8(0u);
  std::uint8_t block[block_bytes] = {};
  WireArena    arena(block, sizeof block);
  WireReader   reader(frame, writer.offset());
  std::uint8_t reply_bytes[64] = {};
  WireWriter   reply(reply_bytes, sizeof reply_bytes);
  CHECK(!backend.run(static_cast<std::uint32_t>(av::vkproto::kCmd_vkUpdateDescriptorSetWithTemplate), reader, reply,
                     arena)); }

TEST(vulkan_backend, a_backend_with_no_device_exports_imports_and_releases_nothing)
{ Rig rig;
  VulkanBackend backend(rig.lines);
  std::uint32_t handle_kind = 9u;
  av::OwnedFd   descriptor;
  CHECK(!backend.export_object(AV_SHARED_MEMORY, 0x40u, handle_kind, descriptor));
  CHECK_EQ(handle_kind, 0u);
  CHECK(!descriptor.held());
  CHECK(!backend.export_object(AV_SHARED_MONITORED_FENCE, 0x40u, handle_kind, descriptor));
  CHECK(!backend.export_object(AV_SHARED_MEMORY, 0u, handle_kind, descriptor));

  std::uint64_t made = 9u;
  CHECK(!backend.import_object(AV_SHARED_MEMORY, AV_SHARED_HANDLE_DMA_BUF, 3, 4096u, av::SharedImageShape(), 0x7001u,
                               made, detail));
  CHECK_EQ(made, 0u);
  CHECK(!backend.import_object(AV_SHARED_MONITORED_FENCE, AV_SHARED_HANDLE_OPAQUE_FD, 3, 0u, av::SharedImageShape(),
                               0x7002u, made, detail));
  backend.release_object(AV_SHARED_MEMORY, 0x40u, detail);
  backend.release_object(AV_SHARED_MONITORED_FENCE, 0u, detail); }

namespace {

void never_called(void *) {}

}

TEST(vulkan_backend, a_signal_changes_nothing_it_waits_on)
{ Rig rig;
  VulkanBackend backend(rig.lines);
  backend.bind_signal(&never_called, nullptr);
  CHECK(!backend.poll());
  std::uint64_t marked = 7u, reached = 7u;
  backend.gpu_progress(marked, reached);
  CHECK_EQ(marked, 0u);
  CHECK_EQ(reached, 0u); }

TEST(vulkan_backend, the_guest_s_device_local_heap_is_capped_by_the_reserve_the_source_holds)
{ av::MockPlatform       platform;
  av::MockMemoryExporter exporter;
  av::HostMemorySource   source(exporter, platform);
  constexpr std::uint64_t card = std::uint64_t{16} << 30u;
  const std::uint32_t     heap = exporter.add_heap(VK_MEMORY_HEAP_DEVICE_LOCAL_BIT, card);
  exporter.add_type(av::MockMemoryExporter::device_local, true, heap);
  source.reserve_vram(6144u);
  av::VramLedger ledger;
  ledger.learn(card);
  ledger.reserve_vram(6144u);
  av::HostGpu   gpu;
  Rig rig;
  VulkanBackend backend(rig.lines);
  backend.use_vram(&ledger);
  backend.initialize(gpu, &source, rig.imports);
  VkPhysicalDeviceMemoryProperties guest = {};
  backend.on_vkGetPhysicalDeviceMemoryProperties(VK_NULL_HANDLE, &guest);
  bool seen = false;
  for (std::uint32_t index = 0u; index < guest.memoryHeapCount; index++) {
      if ((guest.memoryHeaps[index].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) == 0u) continue;
      seen = true;
      CHECK_EQ(guest.memoryHeaps[index].size, av::VramPolicy(6144u).cap_for(card));
      CHECK(guest.memoryHeaps[index].size != av::VramPolicy().cap_for(card));
  }
  CHECK(seen);
  backend.shut_down(); }

TEST(vulkan_backend, the_guest_s_types_are_the_source_s_learnt_once_per_context)
{ av::MockPlatform       platform;
  av::MockMemoryExporter exporter;
  av::HostMemorySource   source(exporter, platform);
  constexpr std::uint32_t integrated = av::MockMemoryExporter::device_local | av::MockMemoryExporter::host_visible |
                                       av::MockMemoryExporter::host_coherent;
  exporter.add_type(integrated, true);
  av::HostGpu   gpu;
  Rig rig;
  VulkanBackend backend(rig.lines);
  backend.initialize(gpu, &source, rig.imports);
  av::VulkanBackendProbe probe(backend);
  probe.learn_types(VK_NULL_HANDLE);
  VkPhysicalDeviceMemoryProperties guest = {};
  probe.types().properties(guest);
  CHECK_EQ(guest.memoryTypeCount, 2u);
  CHECK_EQ(guest.memoryTypes[0].propertyFlags, VkMemoryPropertyFlags{VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT});
  CHECK_EQ(probe.types().source_type(0u), av::GuestMemoryTypes::no_type);
  CHECK_EQ(probe.types().source_type(1u), 0u);

  exporter.add_type(av::MockMemoryExporter::host_visible | av::MockMemoryExporter::host_coherent, true);
  probe.learn_types(VK_NULL_HANDLE);
  probe.types().properties(guest);
  CHECK_EQ(guest.memoryTypeCount, 2u);

  backend.shut_down();
  backend.initialize(gpu, &source, rig.imports);
  probe.learn_types(VK_NULL_HANDLE);
  probe.types().properties(guest);
  CHECK_EQ(guest.memoryTypeCount, 3u);
  CHECK_EQ(probe.types().source_type(1u), 1u);
  backend.shut_down(); }
