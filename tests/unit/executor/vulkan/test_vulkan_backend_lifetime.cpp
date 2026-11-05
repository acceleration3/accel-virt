#include "core/log/driver_log.hpp"
#include "executor/vulkan/vulkan_backend.hpp"
#include "harness/executor/vulkan_backend_probe.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/icd/recording_log.hpp"
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

class InventingDriver final : public vkproto::DefaultServerDispatch {
public:
    explicit InventingDriver(VulkanBackend &backend) : backend_(backend) {}

    bool handle_for(vkproto::HandleKind kind, std::uint64_t wire, std::uint64_t &handle) const override
    { return backend_.handle_for(kind, wire, handle); }
    std::uint64_t template_bytes(VkDescriptorUpdateTemplate made) const override
    { return backend_.template_bytes(made); }
    void made(vkproto::HandleKind kind, std::uint64_t handle) override { backend_.made(kind, handle); }
    bool room_for(std::uint64_t handles) override { return backend_.room_for(handles); }
    void made_through(std::uint64_t parent) override { backend_.made_through(parent); }
    void ended(std::uint64_t handle) override { backend_.ended(handle); }
    void children_ended(std::uint64_t handle) override { backend_.children_ended(handle); }

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

    VulkanBackend &backend_;
    std::uint64_t next_ = 0x10000u;
    std::uint32_t destroyed_buffers_ = 0u;
    std::uint32_t asked_requirements_ = 0u;
};

class GuestContext {
public:
    GuestContext() : sink_(), lines_(sink_, "vulkan: "), backend_(lines_), probe_(backend_), driver_(backend_), block_{}, reply_{}, answered_(0u) {}

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
      vkproto::serialize_VkInstanceCreateInfo(writer, identity_, create);
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
      vkproto::serialize_VkDeviceCreateInfo(writer, identity_, create);
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
      vkproto::serialize_VkBufferCreateInfo(writer, identity_, create);
      return send(vkproto::kCmd_vkCreateBuffer, writer) ? answered(0u) : 0u; }

    std::uint64_t memory(std::uint64_t device)
    { WireWriter writer = frame();
      writer.write_pod(device);
      VkMemoryAllocateInfo allocate = {};
      allocate.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
      allocate.allocationSize = 4096u;
      vkproto::serialize_VkMemoryAllocateInfo(writer, identity_, allocate);
      return send(vkproto::kCmd_vkAllocateMemory, writer) ? answered(0u) : 0u; }

    std::uint64_t command_pool(std::uint64_t device)
    { WireWriter writer = frame();
      writer.write_pod(device);
      VkCommandPoolCreateInfo create = {};
      create.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
      vkproto::serialize_VkCommandPoolCreateInfo(writer, identity_, create);
      return send(vkproto::kCmd_vkCreateCommandPool, writer) ? answered(0u) : 0u; }

    bool command_buffers(std::uint64_t device, std::uint64_t pool, std::uint32_t count,
                         std::uint64_t *out)
    { WireWriter writer = frame();
      writer.write_pod(device);
      VkCommandBufferAllocateInfo allocate = {};
      allocate.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
      allocate.commandPool = as_handle<VkCommandPool>(pool);
      allocate.commandBufferCount = count;
      vkproto::serialize_VkCommandBufferAllocateInfo(writer, identity_, allocate);
      if (!send(vkproto::kCmd_vkAllocateCommandBuffers, writer)) return false;
      for (std::uint32_t i = 0u; i < count; i++) out[i] = answered(i);
      return true; }

    std::uint64_t descriptor_pool(std::uint64_t device)
    { WireWriter writer = frame();
      writer.write_pod(device);
      VkDescriptorPoolCreateInfo create = {};
      create.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
      create.maxSets = most_listed;
      vkproto::serialize_VkDescriptorPoolCreateInfo(writer, identity_, create);
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
      vkproto::serialize_VkDescriptorSetAllocateInfo(writer, identity_, allocate);
      if (!send(vkproto::kCmd_vkAllocateDescriptorSets, writer)) return false;
      for (std::uint32_t i = 0u; i < count; i++) out[i] = answered(i);
      return true; }

    bool end(std::uint32_t command, std::uint64_t handle)
    { WireWriter writer = frame();
      writer.write_pod(handle);
      return send(command, writer); }

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
    av::RecordingLog   sink_;
    av::DriverLog      lines_;
    VulkanBackend      backend_;
    VulkanBackendProbe probe_;
    InventingDriver    driver_;
    vkproto::HandleIdentity identity_;
    std::uint8_t       block_[block_bytes];
    std::uint8_t       frame_[reply_bytes];
    std::uint8_t       reply_[reply_bytes];
    std::size_t        answered_;
};

struct Chain {
    explicit Chain(GuestContext &guest)
        : instance(guest.instance()), physical(guest.physical_device(instance)),
          device(guest.device(physical))
    {}

    std::uint64_t instance;
    std::uint64_t physical;
    std::uint64_t device;
};

}

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
      guest.backend().made(vkproto::HandleKind::device, device);
      guest.backend().made_through(device);
      guest.backend().made(vkproto::HandleKind::buffer, object);
      guest.backend().made(vkproto::HandleKind::buffer, neighbour);
      guest.backend().made_through(0u);
      CHECK(guest.end(command, device, object));
      CHECK(!guest.knows(object));
      CHECK(guest.knows(device));
      CHECK(guest.knows(neighbour));
      CHECK(!guest.end(command, device, object));
  } }

TEST(vulkan_backend_lifetime, destroying_an_instance_or_a_device_ends_it)
{ constexpr std::uint32_t commands[] = {vkproto::kCmd_vkDestroyInstance,
                                        vkproto::kCmd_vkDestroyDevice};
  constexpr std::uint64_t root = 0x1000u, neighbour = 0x3000u;
  for (const std::uint32_t command : commands) {
      GuestContext guest;
      guest.backend().made(command == vkproto::kCmd_vkDestroyInstance ? vkproto::HandleKind::instance
                                                                       : vkproto::HandleKind::device,
                           root);
      guest.backend().made(vkproto::HandleKind::buffer, neighbour);
      CHECK(guest.end(command, root));
      CHECK(!guest.knows(root));
      CHECK(guest.knows(neighbour));
      CHECK(!guest.end(command, root));
  } }

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

TEST(vulkan_backend_lifetime, a_command_s_parent_does_not_outlive_the_command)
{ GuestContext guest;
  const Chain chain(guest);
  guest.backend().made(vkproto::HandleKind::buffer, 0x7000u);
  const std::uint64_t second = guest.instance();
  CHECK(guest.end(vkproto::kCmd_vkDestroyInstance, chain.instance));
  CHECK(!guest.knows(chain.device));
  CHECK(guest.knows(0x7000u));
  CHECK(guest.knows(second)); }

TEST(vulkan_backend_lifetime, made_through_ended_and_children_ended_follow_the_parent)
{ av::RecordingLog sink;
  av::DriverLog    lines(sink, "vulkan: ");
  VulkanBackend    backend(lines);
  backend.made(vkproto::HandleKind::physical_device, 0x1000u);
  backend.made_through(0x1000u);
  backend.made(vkproto::HandleKind::queue, 0x2000u);
  backend.made_through(0x2000u);
  backend.made(vkproto::HandleKind::command_buffer, 0x3000u);
  backend.children_ended(0x1000u);
  CHECK(backend.knows_handle(0x1000u));
  CHECK(!backend.knows_handle(0x2000u));
  CHECK(!backend.knows_handle(0x3000u));
  backend.made_through(0u);
  backend.made(vkproto::HandleKind::buffer, 0x4000u);
  backend.ended(0x1000u);
  CHECK(!backend.knows_handle(0x1000u));
  CHECK(backend.knows_handle(0x4000u));
  backend.shut_down(); }
