/* What a destroy, a free or a reset ends, as the backend that decides a
 * guest's handles sees it.
 *
 * The driver freed the object, so a handle still known afterwards is a freed
 * pointer the guest can hand the driver again - from the QEMU process, a
 * guest-to-host escape (feature 044, T031). And the driver freed everything
 * made through it, so the same holds for a device's buffers and a pool's
 * command buffers.
 *
 * Decoded through the generated dispatch into a driver that invents its
 * handles, so none of this needs a GPU: what is judged is which numbers the
 * backend still accepts, which is all a guest can reach. */
#include "device/executor/handle_registry.hpp"
#include "device/executor/vulkan_backend.hpp"
#include "harness/executor/vulkan_backend_probe.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/generated/vulkan/vk_default_dispatch.h"
#include "proto/generated/vulkan/vk_dispatch_server.h"
#include "proto/generated/vulkan/vk_protocol_ids.h"
#include "proto/generated/vulkan/vk_serialize.h"
#include "proto/wire/wire_arena.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

using av::VulkanBackend;
using av::VulkanBackendProbe;
using av::WireArena;
using av::WireReader;
using av::WireWriter;
namespace vkproto = av::vkproto;

namespace {

constexpr std::size_t block_bytes = 65536u;
constexpr std::size_t reply_bytes = 256u;
constexpr std::uint32_t most_listed = 4u;

template <typename H>
H as_handle(std::uint64_t raw)
{ H handle = VK_NULL_HANDLE;
  std::memcpy(&handle, &raw, sizeof handle);
  return handle; }

/* A driver with nothing behind it: every create answers a number nobody has
 * used, and the two calls a freed buffer could reach count themselves, so a
 * test can say the driver never saw one. */
class InventingDriver final : public vkproto::DefaultServerDispatch {
public:
    VkResult on_vkCreateInstance(const VkInstanceCreateInfo *, const VkAllocationCallbacks *,
                                 VkInstance *out) override
    { *out = invent<VkInstance>(); return VK_SUCCESS; }
    VkResult on_vkEnumeratePhysicalDevices(VkInstance, uint32_t *count,
                                           VkPhysicalDevice *out) override
    { if (out != nullptr && *count != 0u) out[0] = invent<VkPhysicalDevice>();
      *count = 1u;
      return VK_SUCCESS; }
    VkResult on_vkCreateDevice(VkPhysicalDevice, const VkDeviceCreateInfo *,
                               const VkAllocationCallbacks *, VkDevice *out) override
    { *out = invent<VkDevice>(); return VK_SUCCESS; }
    void on_vkGetDeviceQueue(VkDevice, uint32_t, uint32_t, VkQueue *out) override
    { *out = invent<VkQueue>(); }
    VkResult on_vkCreateBuffer(VkDevice, const VkBufferCreateInfo *, const VkAllocationCallbacks *,
                               VkBuffer *out) override
    { *out = invent<VkBuffer>(); return VK_SUCCESS; }
    VkResult on_vkAllocateMemory(VkDevice, const VkMemoryAllocateInfo *,
                                 const VkAllocationCallbacks *, VkDeviceMemory *out) override
    { *out = invent<VkDeviceMemory>(); return VK_SUCCESS; }
    VkResult on_vkCreateCommandPool(VkDevice, const VkCommandPoolCreateInfo *,
                                    const VkAllocationCallbacks *, VkCommandPool *out) override
    { *out = invent<VkCommandPool>(); return VK_SUCCESS; }
    VkResult on_vkAllocateCommandBuffers(VkDevice, const VkCommandBufferAllocateInfo *info,
                                         VkCommandBuffer *out) override
    { for (uint32_t i = 0u; i < info->commandBufferCount; i++) out[i] = invent<VkCommandBuffer>();
      return VK_SUCCESS; }
    VkResult on_vkCreateDescriptorPool(VkDevice, const VkDescriptorPoolCreateInfo *,
                                       const VkAllocationCallbacks *, VkDescriptorPool *out) override
    { *out = invent<VkDescriptorPool>(); return VK_SUCCESS; }
    VkResult on_vkAllocateDescriptorSets(VkDevice, const VkDescriptorSetAllocateInfo *info,
                                         VkDescriptorSet *out) override
    { for (uint32_t i = 0u; i < info->descriptorSetCount; i++) out[i] = invent<VkDescriptorSet>();
      return VK_SUCCESS; }
    void on_vkDestroyBuffer(VkDevice, VkBuffer, const VkAllocationCallbacks *) override
    { destroyed_buffers_++; }
    void on_vkGetBufferMemoryRequirements(VkDevice, VkBuffer, VkMemoryRequirements *) override
    { asked_requirements_++; }

    std::uint32_t destroyed_buffers() const { return destroyed_buffers_; }
    std::uint32_t asked_requirements() const { return asked_requirements_; }

private:
    template <typename H>
    H invent()
    { next_ += 0x100u;
      return as_handle<H>(next_); }

    std::uint64_t next_ = 0x10000u;
    std::uint32_t destroyed_buffers_ = 0u;
    std::uint32_t asked_requirements_ = 0u;
};

/* One guest context: frames in through the generated decoder with the backend
 * deciding, and the registry's invariants judged after every one. */
class GuestContext {
public:
    GuestContext() : backend_(), probe_(backend_), driver_(), block_{}, reply_{}, answered_(0u) {}

    VulkanBackend        &backend() { return backend_; }
    const InventingDriver &driver() const { return driver_; }
    bool                  knows(std::uint64_t handle) const { return backend_.knows_handle(handle); }

    bool send(std::uint32_t command, const WireWriter &frame)
    { WireArena  arena(block_, sizeof block_);
      WireReader payload(frame_, frame.offset());
      WireWriter answer(reply_, sizeof reply_);
      const bool ran = probe_.run_through(driver_, command, payload, answer, arena);
      answered_ = answer.offset();
      CHECK(probe_.handles().invariants_hold());
      return ran; }

    /* The handle at `index` after a reply's leading result and `skip` bytes. */
    std::uint64_t answered(std::size_t index, std::size_t skip = 0u) const
    { std::uint64_t handle = 0u;
      const std::size_t at = sizeof(std::int32_t) + skip + index * sizeof handle;
      if (at + sizeof handle > answered_) return 0u;
      std::memcpy(&handle, reply_ + at, sizeof handle);
      return handle; }

    WireWriter frame() { return WireWriter(frame_, sizeof frame_); }

    std::uint64_t instance()
    { WireWriter writer = frame();
      VkInstanceCreateInfo create = {};
      create.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
      vkproto::serialize_VkInstanceCreateInfo(writer, create);
      return send(vkproto::kCmd_vkCreateInstance, writer) ? answered(0u) : 0u; }

    std::uint64_t physical_device(std::uint64_t instance)
    { WireWriter writer = frame();
      writer.write_pod(instance);
      writer.write_pod<std::uint8_t>(1u);
      writer.write_pod<std::uint32_t>(1u);
      return send(vkproto::kCmd_vkEnumeratePhysicalDevices, writer)
                 ? answered(0u, sizeof(std::uint32_t)) : 0u; }

    std::uint64_t device(std::uint64_t physical)
    { WireWriter writer = frame();
      writer.write_pod(physical);
      VkDeviceCreateInfo create = {};
      create.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
      vkproto::serialize_VkDeviceCreateInfo(writer, create);
      return send(vkproto::kCmd_vkCreateDevice, writer) ? answered(0u) : 0u; }

    std::uint64_t queue(std::uint64_t device)
    { WireWriter writer = frame();
      writer.write_pod(device);
      writer.write_pod<std::uint32_t>(0u);
      writer.write_pod<std::uint32_t>(0u);
      return send(vkproto::kCmd_vkGetDeviceQueue, writer) ? answered(0u) : 0u; }

    std::uint64_t buffer(std::uint64_t device)
    { WireWriter writer = frame();
      writer.write_pod(device);
      VkBufferCreateInfo create = {};
      create.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
      create.size = 4096u;
      vkproto::serialize_VkBufferCreateInfo(writer, create);
      return send(vkproto::kCmd_vkCreateBuffer, writer) ? answered(0u) : 0u; }

    std::uint64_t memory(std::uint64_t device)
    { WireWriter writer = frame();
      writer.write_pod(device);
      VkMemoryAllocateInfo allocate = {};
      allocate.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
      allocate.allocationSize = 4096u;
      vkproto::serialize_VkMemoryAllocateInfo(writer, allocate);
      return send(vkproto::kCmd_vkAllocateMemory, writer) ? answered(0u) : 0u; }

    std::uint64_t command_pool(std::uint64_t device)
    { WireWriter writer = frame();
      writer.write_pod(device);
      VkCommandPoolCreateInfo create = {};
      create.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
      vkproto::serialize_VkCommandPoolCreateInfo(writer, create);
      return send(vkproto::kCmd_vkCreateCommandPool, writer) ? answered(0u) : 0u; }

    bool command_buffers(std::uint64_t device, std::uint64_t pool, std::uint32_t count,
                         std::uint64_t *out)
    { WireWriter writer = frame();
      writer.write_pod(device);
      VkCommandBufferAllocateInfo allocate = {};
      allocate.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
      allocate.commandPool = as_handle<VkCommandPool>(pool);
      allocate.commandBufferCount = count;
      vkproto::serialize_VkCommandBufferAllocateInfo(writer, allocate);
      if (!send(vkproto::kCmd_vkAllocateCommandBuffers, writer)) return false;
      for (std::uint32_t i = 0u; i < count; i++) out[i] = answered(i);
      return true; }

    std::uint64_t descriptor_pool(std::uint64_t device)
    { WireWriter writer = frame();
      writer.write_pod(device);
      VkDescriptorPoolCreateInfo create = {};
      create.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
      create.maxSets = most_listed;
      vkproto::serialize_VkDescriptorPoolCreateInfo(writer, create);
      return send(vkproto::kCmd_vkCreateDescriptorPool, writer) ? answered(0u) : 0u; }

    bool descriptor_sets(std::uint64_t device, std::uint64_t pool, std::uint32_t count,
                         std::uint64_t *out)
    { WireWriter writer = frame();
      writer.write_pod(device);
      const VkDescriptorSetLayout layouts[most_listed] = {};
      VkDescriptorSetAllocateInfo allocate = {};
      allocate.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
      allocate.descriptorPool = as_handle<VkDescriptorPool>(pool);
      allocate.descriptorSetCount = count;
      allocate.pSetLayouts = layouts;
      vkproto::serialize_VkDescriptorSetAllocateInfo(writer, allocate);
      if (!send(vkproto::kCmd_vkAllocateDescriptorSets, writer)) return false;
      for (std::uint32_t i = 0u; i < count; i++) out[i] = answered(i);
      return true; }

    /* The destroy every dispatchable root takes: the handle and nothing else. */
    bool end(std::uint32_t command, std::uint64_t handle)
    { WireWriter writer = frame();
      writer.write_pod(handle);
      return send(command, writer); }

    /* The destroy and free of everything a device owns. */
    bool end(std::uint32_t command, std::uint64_t device, std::uint64_t handle)
    { WireWriter writer = frame();
      writer.write_pod(device);
      writer.write_pod(handle);
      return send(command, writer); }

    bool end_listed(std::uint32_t command, std::uint64_t device, std::uint64_t pool,
                    std::uint32_t count, const std::uint64_t *handles)
    { WireWriter writer = frame();
      writer.write_pod(device);
      writer.write_pod(pool);
      writer.write_pod(count);
      for (std::uint32_t i = 0u; i < count; i++) writer.write_pod(handles[i]);
      return send(command, writer); }

    bool reset_descriptor_pool(std::uint64_t device, std::uint64_t pool)
    { WireWriter writer = frame();
      writer.write_pod(device);
      writer.write_pod(pool);
      writer.write_pod<std::uint32_t>(0u);
      return send(vkproto::kCmd_vkResetDescriptorPool, writer); }

private:
    VulkanBackend      backend_;
    VulkanBackendProbe probe_;
    InventingDriver    driver_;
    std::uint8_t       block_[block_bytes];
    std::uint8_t       frame_[reply_bytes];
    std::uint8_t       reply_[reply_bytes];
    std::size_t        answered_;
};

/* An instance, its physical device and a device on it, which every other
 * object is made through. */
struct Chain {
    explicit Chain(GuestContext &guest)
        : instance(guest.instance()), physical(guest.physical_device(instance)),
          device(guest.device(physical))
    {}

    std::uint64_t instance;
    std::uint64_t physical;
    std::uint64_t device;
};

}  // namespace

/* The simplest shape of the hole: a buffer the guest destroyed and names
 * again. The driver freed it, so the host refuses the frame before the driver
 * is called at all. */
TEST(vulkan_backend_lifetime, a_destroyed_buffer_is_refused_before_the_driver_sees_it)
{ GuestContext guest;
  const Chain chain(guest);
  const std::uint64_t buffer = guest.buffer(chain.device);
  CHECK(buffer != 0u);
  CHECK(guest.knows(buffer));

  CHECK(guest.end(vkproto::kCmd_vkDestroyBuffer, chain.device, buffer));
  CHECK_EQ(guest.driver().destroyed_buffers(), 1u);
  CHECK(!guest.knows(buffer));
  CHECK(guest.knows(chain.device));

  CHECK(!guest.end(vkproto::kCmd_vkGetBufferMemoryRequirements, chain.device, buffer));
  CHECK_EQ(guest.driver().asked_requirements(), 0u);
  CHECK(!guest.end(vkproto::kCmd_vkDestroyBuffer, chain.device, buffer));
  CHECK_EQ(guest.driver().destroyed_buffers(), 1u); }

/* Every destroy and free the profile carries of a handle a device owns, each
 * of which the generator was told to end: the handle is gone, the device it
 * belongs to is not, and naming it again is refused. */
TEST(vulkan_backend_lifetime, every_destroy_and_free_of_a_device_s_object_ends_it)
{ constexpr std::uint32_t commands[] = {
      vkproto::kCmd_vkFreeMemory,
      vkproto::kCmd_vkDestroyBuffer,
      vkproto::kCmd_vkDestroyImage,
      vkproto::kCmd_vkDestroyImageView,
      vkproto::kCmd_vkDestroySampler,
      vkproto::kCmd_vkDestroySemaphore,
      vkproto::kCmd_vkDestroyFence,
      vkproto::kCmd_vkDestroyCommandPool,
      vkproto::kCmd_vkDestroyShaderModule,
      vkproto::kCmd_vkDestroyPipelineLayout,
      vkproto::kCmd_vkDestroyPipeline,
      vkproto::kCmd_vkDestroyPipelineCache,
      vkproto::kCmd_vkDestroyRenderPass,
      vkproto::kCmd_vkDestroyFramebuffer,
      vkproto::kCmd_vkDestroyDescriptorSetLayout,
      vkproto::kCmd_vkDestroyDescriptorPool,
      vkproto::kCmd_vkDestroyEvent,
      vkproto::kCmd_vkDestroyBufferView,
      vkproto::kCmd_vkDestroyDescriptorUpdateTemplate,
      vkproto::kCmd_vkDestroyPrivateDataSlot,
      vkproto::kCmd_vkDestroyQueryPool,
      vkproto::kCmd_vkDestroySamplerYcbcrConversion,
      vkproto::kCmd_vkDestroyDescriptorUpdateTemplateKHR,
      vkproto::kCmd_vkDestroySamplerYcbcrConversionKHR,
      vkproto::kCmd_vkDestroyAccelerationStructureKHR,
      vkproto::kCmd_vkDestroyDeferredOperationKHR,
      vkproto::kCmd_vkDestroyShaderEXT,
      vkproto::kCmd_vkDestroyPipelineBinaryKHR,
  };
  constexpr std::uint64_t device = 0x1000u, object = 0x2000u, neighbour = 0x3000u;
  for (const std::uint32_t command : commands) {
      GuestContext guest;
      guest.backend().made(device, av::HandleKind::device);
      guest.backend().made(object);
      guest.backend().made(neighbour);
      CHECK(guest.end(command, device, object));
      CHECK(!guest.knows(object));
      CHECK(guest.knows(device));
      CHECK(guest.knows(neighbour));
      CHECK(!guest.end(command, device, object));
  } }

/* The two roots, which take only themselves. */
TEST(vulkan_backend_lifetime, destroying_an_instance_or_a_device_ends_it)
{ constexpr std::uint32_t commands[] = {vkproto::kCmd_vkDestroyInstance,
                                        vkproto::kCmd_vkDestroyDevice};
  constexpr std::uint64_t root = 0x1000u, neighbour = 0x3000u;
  for (const std::uint32_t command : commands) {
      GuestContext guest;
      guest.backend().made(root, command == vkproto::kCmd_vkDestroyInstance ? av::HandleKind::instance
                                                                            : av::HandleKind::device);
      guest.backend().made(neighbour);
      CHECK(guest.end(command, root));
      CHECK(!guest.knows(root));
      CHECK(guest.knows(neighbour));
      CHECK(!guest.end(command, root));
  } }

/* A device's destruction frees everything made through it, whatever made it -
 * a queue it answered, a buffer, memory, a pool and the pool's own command
 * buffers - and nothing of the device beside it. */
TEST(vulkan_backend_lifetime, destroying_a_device_ends_everything_made_through_it)
{ GuestContext guest;
  const Chain chain(guest);
  const std::uint64_t other = guest.device(chain.physical);
  const std::uint64_t others_buffer = guest.buffer(other);
  const std::uint64_t queue = guest.queue(chain.device);
  const std::uint64_t buffer = guest.buffer(chain.device);
  const std::uint64_t memory = guest.memory(chain.device);
  const std::uint64_t pool = guest.command_pool(chain.device);
  std::uint64_t command_buffers[2] = {};
  CHECK(guest.command_buffers(chain.device, pool, 2u, command_buffers));
  const std::uint64_t descriptors = guest.descriptor_pool(chain.device);
  std::uint64_t sets[2] = {};
  CHECK(guest.descriptor_sets(chain.device, descriptors, 2u, sets));
  const std::uint64_t made[] = {queue, buffer, memory, pool, command_buffers[0],
                                command_buffers[1], descriptors, sets[0], sets[1]};
  for (const std::uint64_t handle : made) {
      CHECK(handle != 0u);
      CHECK(guest.knows(handle));
  }

  CHECK(guest.end(vkproto::kCmd_vkDestroyDevice, chain.device));
  CHECK(!guest.knows(chain.device));
  for (const std::uint64_t handle : made) CHECK(!guest.knows(handle));
  CHECK(guest.knows(other));
  CHECK(guest.knows(others_buffer));
  CHECK(guest.knows(chain.physical));
  CHECK(guest.knows(chain.instance)); }

/* An instance's destruction takes its physical devices, their devices, and
 * what those made - and leaves a second instance's alone. */
TEST(vulkan_backend_lifetime, destroying_an_instance_ends_its_physical_devices_and_their_devices)
{ GuestContext guest;
  const Chain chain(guest);
  const std::uint64_t buffer = guest.buffer(chain.device);
  const Chain second(guest);
  CHECK(buffer != 0u);
  CHECK(second.device != 0u);

  CHECK(guest.end(vkproto::kCmd_vkDestroyInstance, chain.instance));
  CHECK(!guest.knows(chain.instance));
  CHECK(!guest.knows(chain.physical));
  CHECK(!guest.knows(chain.device));
  CHECK(!guest.knows(buffer));
  CHECK(guest.knows(second.instance));
  CHECK(guest.knows(second.physical));
  CHECK(guest.knows(second.device)); }

/* A command buffer is made through its pool, not its device: destroying the
 * pool ends the pool's and no other pool's. */
TEST(vulkan_backend_lifetime, destroying_a_command_pool_ends_its_command_buffers)
{ GuestContext guest;
  const Chain chain(guest);
  const std::uint64_t pool = guest.command_pool(chain.device);
  const std::uint64_t other = guest.command_pool(chain.device);
  std::uint64_t mine[2] = {};
  std::uint64_t others[1] = {};
  CHECK(guest.command_buffers(chain.device, pool, 2u, mine));
  CHECK(guest.command_buffers(chain.device, other, 1u, others));

  CHECK(guest.end(vkproto::kCmd_vkDestroyCommandPool, chain.device, pool));
  CHECK(!guest.knows(pool));
  CHECK(!guest.knows(mine[0]));
  CHECK(!guest.knows(mine[1]));
  CHECK(guest.knows(others[0]));
  CHECK(guest.knows(other));
  CHECK(guest.knows(chain.device)); }

/* Freeing a list ends each one listed and not the one left out. */
TEST(vulkan_backend_lifetime, freeing_command_buffers_ends_each_listed_one)
{ GuestContext guest;
  const Chain chain(guest);
  const std::uint64_t pool = guest.command_pool(chain.device);
  std::uint64_t buffers[3] = {};
  CHECK(guest.command_buffers(chain.device, pool, 3u, buffers));
  const std::uint64_t listed[] = {buffers[2], buffers[0]};

  CHECK(guest.end_listed(vkproto::kCmd_vkFreeCommandBuffers, chain.device, pool, 2u, listed));
  CHECK(!guest.knows(buffers[0]));
  CHECK(!guest.knows(buffers[2]));
  CHECK(guest.knows(buffers[1]));
  CHECK(guest.knows(pool));
  /* Freed twice is naming a freed one. */
  CHECK(!guest.end_listed(vkproto::kCmd_vkFreeCommandBuffers, chain.device, pool, 1u, listed)); }

TEST(vulkan_backend_lifetime, freeing_descriptor_sets_ends_each_listed_one)
{ GuestContext guest;
  const Chain chain(guest);
  const std::uint64_t pool = guest.descriptor_pool(chain.device);
  std::uint64_t sets[3] = {};
  CHECK(guest.descriptor_sets(chain.device, pool, 3u, sets));
  const std::uint64_t listed[] = {sets[1], sets[2]};

  CHECK(guest.end_listed(vkproto::kCmd_vkFreeDescriptorSets, chain.device, pool, 2u, listed));
  CHECK(!guest.knows(sets[1]));
  CHECK(!guest.knows(sets[2]));
  CHECK(guest.knows(sets[0]));
  CHECK(guest.knows(pool));
  CHECK(!guest.end_listed(vkproto::kCmd_vkFreeDescriptorSets, chain.device, pool, 1u, listed)); }

/* A reset frees every set the pool gave out and keeps the pool; another
 * pool's sets are not its to free. */
TEST(vulkan_backend_lifetime, resetting_a_descriptor_pool_ends_its_sets_and_keeps_it)
{ GuestContext guest;
  const Chain chain(guest);
  const std::uint64_t pool = guest.descriptor_pool(chain.device);
  const std::uint64_t other = guest.descriptor_pool(chain.device);
  std::uint64_t mine[2] = {};
  std::uint64_t others[1] = {};
  CHECK(guest.descriptor_sets(chain.device, pool, 2u, mine));
  CHECK(guest.descriptor_sets(chain.device, other, 1u, others));

  CHECK(guest.reset_descriptor_pool(chain.device, pool));
  CHECK(guest.knows(pool));
  CHECK(!guest.knows(mine[0]));
  CHECK(!guest.knows(mine[1]));
  CHECK(guest.knows(others[0])); }

TEST(vulkan_backend_lifetime, destroying_a_descriptor_pool_ends_its_sets)
{ GuestContext guest;
  const Chain chain(guest);
  const std::uint64_t pool = guest.descriptor_pool(chain.device);
  std::uint64_t sets[2] = {};
  CHECK(guest.descriptor_sets(chain.device, pool, 2u, sets));

  CHECK(guest.end(vkproto::kCmd_vkDestroyDescriptorPool, chain.device, pool));
  CHECK(!guest.knows(pool));
  CHECK(!guest.knows(sets[0]));
  CHECK(!guest.knows(sets[1])); }

/* What a command makes through is the command's own: an instance made after a
 * device is nobody's child, and neither is a handle recorded between runs. */
TEST(vulkan_backend_lifetime, a_command_s_parent_does_not_outlive_the_command)
{ GuestContext guest;
  const Chain chain(guest);
  guest.backend().made(0x7000u);
  const std::uint64_t second = guest.instance();
  CHECK(guest.end(vkproto::kCmd_vkDestroyInstance, chain.instance));
  CHECK(!guest.knows(chain.device));
  CHECK(guest.knows(0x7000u));
  CHECK(guest.knows(second)); }

/* The backend's own surface, which is what the hooks call. */
TEST(vulkan_backend_lifetime, made_through_ended_and_children_ended_follow_the_parent)
{ VulkanBackend backend;
  backend.made(0x1000u);
  backend.made_through(0x1000u);
  backend.made(0x2000u);
  backend.made_through(0x2000u);
  backend.made(0x3000u);
  backend.children_ended(0x1000u);
  CHECK(backend.knows_handle(0x1000u));
  CHECK(!backend.knows_handle(0x2000u));
  CHECK(!backend.knows_handle(0x3000u));
  backend.made_through(0u);
  backend.made(0x4000u);
  backend.ended(0x1000u);
  CHECK(!backend.knows_handle(0x1000u));
  CHECK(backend.knows_handle(0x4000u)); }

/* Outside a dispatch nobody is deciding: the guest side, where nothing was
 * remembered, so an ending there forgets nothing on anybody's behalf. */
TEST(vulkan_backend_lifetime, outside_a_dispatch_an_ending_forgets_nothing)
{ VulkanBackend backend;
  backend.made(0x1000u);
  backend.made(0x2000u);
  vkproto::handles_made_through(0x1000u);
  vkproto::handle_children_ended(0x1000u);
  vkproto::handle_ended(0x2000u);
  CHECK(backend.knows_handle(0x1000u));
  CHECK(backend.knows_handle(0x2000u));
  CHECK(VulkanBackend::deciding() == nullptr); }
