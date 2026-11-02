#include "core/support/status.hpp"
#include "device/abi/av_executor.h"
#include "executor/vulkan/extension_list.hpp"
#include "executor/vulkan/guest_memory_types.hpp"
#include "executor/vulkan/handle_registry.hpp"
#include "executor/vulkan/host_exported_fds.hpp"
#include "executor/host_gpu.hpp"
#include "executor/vulkan/host_template_table.hpp"
#include "executor/exported_memory.hpp"
#include "executor/host_memory_source.hpp"
#include "executor/imported_blob.hpp"
#include "executor/source_memory.hpp"
#include "executor/import_table.hpp"
#include "executor/shim/loader_host_apis.hpp"
#include "executor/vulkan/vulkan_backend.hpp"
#include "harness/executor/mock_memory_exporter.hpp"
#include "harness/executor/vulkan_backend_probe.hpp"
#include "harness/platform/mock_platform.hpp"
#include "executor/sharing/shared_image_shape.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/client/client_blob_arena.hpp"
#include "proto/resources/host_memory_request.hpp"
#include "proto/client/client_transport.hpp"
#include "proto/generated/vulkan/vk_dispatch_client.h"
#include "proto/generated/vulkan/vk_protocol_ids.h"
#include "proto/generated/vulkan/vk_serialize.h"
#include "proto/wire/wire_arena.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

#include <cstdint>
#include <cstring>
#include <cstdio>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

extern "C" int __lsan_is_turned_off(void);
extern "C" int __lsan_is_turned_off(void) { return 1; }

using av::ImportedBlob;
using av::VulkanBackend;
using av::VulkanBackendProbe;
using av::WireArena;
using av::WireReader;
using av::WireWriter;
namespace vkproto = av::vkproto;

namespace {
std::uint64_t detail = 0u;

constexpr std::size_t block_bytes = 65536u;
constexpr std::size_t page = 4096u;
constexpr std::uint32_t domain = 1u;

template <typename H>
H as_handle(std::uint64_t raw)
{ H handle = VK_NULL_HANDLE;
  std::memcpy(&handle, &raw, sizeof handle);
  return handle; }

template <typename H>
std::uint64_t as_wire(H handle)
{ std::uint64_t raw = 0u;
  std::memcpy(&raw, &handle, sizeof handle);
  return raw; }

class GuestSequence {
public:
    explicit GuestSequence(VulkanBackend &backend) : backend_(backend), block_{} {}

    bool send(std::uint32_t command, const std::uint8_t *frame, std::size_t length,
              std::uint8_t *reply, std::size_t capacity, std::size_t &answered)
    {
        WireArena  arena(block_, sizeof block_);
        WireReader payload(frame, length);
        WireWriter answer(reply, capacity);
        const bool ran = backend_.run(command, payload, answer, arena);
        answered = answer.offset();
        return ran;
    }

    std::uint64_t create_instance()
    {
        std::uint8_t frame[1024];
        WireWriter   writer(frame, sizeof frame);
        VkApplicationInfo application = {};
        application.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        application.pApplicationName = "test";
        application.apiVersion = (1u << 22) | (1u << 12);
        VkInstanceCreateInfo create = {};
        create.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        create.pApplicationInfo = &application;
        vkproto::serialize_VkInstanceCreateInfo(writer, create);
        std::uint8_t reply[64] = {};
        std::size_t  answered = 0u;
        if (!send(vkproto::kCmd_vkCreateInstance, frame, writer.offset(), reply, sizeof reply,
                  answered))
            return 0u;
        return handle_after_result(reply, answered);
    }

    std::uint64_t first_physical_device(std::uint64_t instance)
    {
        std::uint8_t frame[32];
        WireWriter   writer(frame, sizeof frame);
        writer.write_pod(instance);
        writer.write_pod<std::uint8_t>(1u);
        writer.write_pod<std::uint32_t>(1u);
        std::uint8_t reply[64] = {};
        std::size_t  answered = 0u;
        if (!send(vkproto::kCmd_vkEnumeratePhysicalDevices, frame, writer.offset(), reply,
                  sizeof reply, answered))
            return 0u;
        WireReader    read(reply, answered);
        std::int32_t  returned = 0;
        std::uint32_t count = 0u;
        std::uint64_t device = 0u;
        (void)read.read_pod(returned);
        (void)read.read_pod(count);
        (void)read.read_pod(device);
        return count != 0u ? device : 0u;
    }

    std::uint64_t create_device(std::uint64_t physical, std::uint32_t count = 0u,
                                const char *const *extensions = nullptr)
    {
        std::uint8_t frame[16384];
        WireWriter   writer(frame, sizeof frame);
        writer.write_pod(physical);
        VkDeviceCreateInfo create = {};
        create.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        float priority = 1.0f;
        VkDeviceQueueCreateInfo queue = {};
        queue.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue.queueCount = 1u;
        queue.pQueuePriorities = &priority;
        create.queueCreateInfoCount = 1u;
        create.pQueueCreateInfos = &queue;
        create.enabledExtensionCount = count;
        create.ppEnabledExtensionNames = extensions;
        vkproto::serialize_VkDeviceCreateInfo(writer, create);
        std::uint8_t reply[64] = {};
        std::size_t  answered = 0u;
        if (!send(vkproto::kCmd_vkCreateDevice, frame, writer.offset(), reply, sizeof reply,
                  answered))
            return 0u;
        return handle_after_result(reply, answered);
    }

    VkResult create_template(std::uint64_t device, const VkDescriptorUpdateTemplateEntry *entries,
                             std::uint32_t count)
    {
        std::uint8_t frame[1024];
        WireWriter   writer(frame, sizeof frame);
        writer.write_pod(device);
        VkDescriptorUpdateTemplateCreateInfo create = {};
        create.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_UPDATE_TEMPLATE_CREATE_INFO;
        create.descriptorUpdateEntryCount = count;
        create.pDescriptorUpdateEntries = entries;
        create.templateType = VK_DESCRIPTOR_UPDATE_TEMPLATE_TYPE_DESCRIPTOR_SET;
        vkproto::serialize_VkDescriptorUpdateTemplateCreateInfo(writer, create);
        std::uint8_t reply[64] = {};
        std::size_t  answered = 0u;
        if (!send(vkproto::kCmd_vkCreateDescriptorUpdateTemplate, frame, writer.offset(), reply, sizeof reply,
                  answered))
            return VK_ERROR_UNKNOWN;
        WireReader   read(reply, answered);
        std::int32_t returned = 0;
        (void)read.read_pod(returned);
        return static_cast<VkResult>(returned);
    }

    bool destroy(std::uint32_t command, std::uint64_t handle)
    {
        std::uint8_t frame[32];
        WireWriter   writer(frame, sizeof frame);
        writer.write_pod(handle);
        std::uint8_t reply[8] = {};
        std::size_t  answered = 0u;
        return send(command, frame, writer.offset(), reply, sizeof reply, answered);
    }

private:
    std::uint64_t handle_after_result(const std::uint8_t *reply, std::size_t answered) const
    {
        WireReader    read(reply, answered);
        std::int32_t  returned = 0;
        std::uint64_t handle = 0u;
        (void)read.read_pod(returned);
        (void)read.read_pod(handle);
        return returned == 0 ? handle : 0u;
    }

    VulkanBackend &backend_;
    std::uint8_t   block_[block_bytes];
};

class BackendTransport final : public av::ClientTransport {
public:
    explicit BackendTransport(VulkanBackend &backend)
        : backend_(backend), request_{}, reply_{}, block_{}, arena_(nullptr, 0u), blobs_(), ran_(false)
    {}

    WireWriter request_writer() override { return WireWriter(request_, sizeof request_); }
    [[nodiscard]] av::TransportResult send_async(std::uint32_t command, WireWriter &writer) override
    { std::size_t answered = 0u;
      return deliver(command, writer, answered); }
    [[nodiscard]] av::TransportResult send_sync(std::uint32_t command, WireWriter &writer,
                                                WireReader &reply) override
    { std::size_t answered = 0u;
      const av::TransportResult delivered = deliver(command, writer, answered);
      reply = WireReader(reply_, answered);
      return delivered; }
    void                 end_call() override {}
    WireArena           &decode_arena() override { return arena_; }
    av::ClientBlobArena &blobs() override { return blobs_; }

    bool ran() const { return ran_; }

private:
    av::TransportResult deliver(std::uint32_t command, WireWriter &writer, std::size_t &answered)
    { if (writer.status() != av::Status::ok) return av::TransportResult::failed;
      WireArena  arena(block_, sizeof block_);
      WireReader payload(request_, writer.offset());
      WireWriter answer(reply_, sizeof reply_);
      ran_ = backend_.run(command, payload, answer, arena);
      answered = answer.offset();
      return ran_ ? av::TransportResult::ok : av::TransportResult::failed; }

    VulkanBackend      &backend_;
    std::uint8_t        request_[4096];
    std::uint8_t        reply_[4096];
    std::uint8_t        block_[block_bytes];
    WireArena           arena_;
    av::ClientBlobArena blobs_;
    bool                ran_;
};

std::uint32_t guest_type_of(av::HostMemorySource *source, std::uint32_t type)
{ av::GuestMemoryTypes view;
  if (source == nullptr || !source->vulkan_types(view)) return av::GuestMemoryTypes::no_type;
  for (std::uint32_t guest = 0u; guest < VK_MAX_MEMORY_TYPES; guest++)
      if (view.source_type(guest) == type) return guest;
  return av::GuestMemoryTypes::no_type; }

class Allocation {
public:
    Allocation(av::HostMemorySource *source, std::uint64_t key, std::uint32_t type, std::uint64_t size = page)
        : source_(source), key_(key), answered_{-1, 0u, 0u}, made_(false)
    { made_ = source_ != nullptr &&
              source_->allocate(domain, key_, size,
                                av::HostMemoryRequest(av::HostMemoryRequest::Kind::vulkan,
                                                      guest_type_of(source_, type)),
                                answered_) == av::Status::ok; }
    ~Allocation()
    { if (made_) source_->free(domain, key_);
      if (answered_.fd >= 0) ::close(answered_.fd); }
    Allocation(const Allocation &) = delete;
    Allocation &operator=(const Allocation &) = delete;

    bool made() const { return made_; }
    int  descriptor() const { return answered_.fd; }
    bool take(ImportedBlob &blob, std::uint64_t id)
    { av::SourceMemory *const memory = made_ ? source_->acquire(domain, key_) : nullptr;
      if (memory == nullptr) return false;
      if (blob.take(id, *memory) == av::Status::ok) return true;
      source_->release(*memory);
      return false; }
    void give_back(ImportedBlob &blob)
    { av::SourceMemory *const memory = blob.give_back();
      if (memory != nullptr) source_->release(*memory); }
    std::uint8_t *bytes() const
    { if (!made_) return nullptr;
      av::SourceMemory *const memory = source_->acquire(domain, key_);
      std::uint8_t *const     pointer = memory->pointer();
      source_->release(*memory);
      return pointer; }

private:
    av::HostMemorySource *source_;
    std::uint64_t         key_;
    AvHostMemory          answered_;
    bool                  made_;
};

class GuestDevice {
public:
    GuestDevice(av::LoaderHostApis &host, std::uint32_t index, std::uint32_t count = 0u,
                const char *const *extensions = nullptr)
        : gpu_(), source_(host.memory_source(index)), backend_(), probe_(backend_), guest_(backend_), instance_(0u),
          physical_(0u), device_(0u)
    { gpu_.describe(host, index);
      backend_.initialize(gpu_, source_);
      instance_ = guest_.create_instance();
      physical_ = instance_ != 0u ? guest_.first_physical_device(instance_) : 0u;
      device_ = physical_ != 0u ? guest_.create_device(physical_, count, extensions) : 0u; }
    ~GuestDevice() { backend_.shut_down(); }
    GuestDevice(const GuestDevice &) = delete;
    GuestDevice &operator=(const GuestDevice &) = delete;

    VulkanBackend      &backend() { return backend_; }
    VulkanBackendProbe &probe() { return probe_; }
    GuestSequence      &guest() { return guest_; }
    std::uint64_t       instance() const { return instance_; }
    std::uint64_t       physical() const { return physical_; }
    std::uint64_t       device() const { return device_; }
    av::HostMemorySource *source() const { return source_; }
    std::uint32_t         mappable() const { return source_ != nullptr ? source_->mappable_types() : 0u; }

    std::uint32_t importable_dmabuf(int fd) const
    { const VkDevice device = as_handle<VkDevice>(device_);
      auto *const properties = reinterpret_cast<PFN_vkGetMemoryFdPropertiesKHR>(
          ::vkGetDeviceProcAddr(device, "vkGetMemoryFdPropertiesKHR"));
      if (properties == nullptr || fd < 0) return 0u;
      VkMemoryFdPropertiesKHR answered = {};
      answered.sType = VK_STRUCTURE_TYPE_MEMORY_FD_PROPERTIES_KHR;
      if (properties(device, VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT, fd, &answered) != VK_SUCCESS)
          return 0u;
      return answered.memoryTypeBits; }

    std::uint32_t memory_types() const
    { VkPhysicalDeviceMemoryProperties memory = {};
      ::vkGetPhysicalDeviceMemoryProperties(as_handle<VkPhysicalDevice>(physical_), &memory);
      return memory.memoryTypeCount; }

private:
    av::HostGpu        gpu_;
    av::HostMemorySource *source_;
    VulkanBackend      backend_;
    VulkanBackendProbe probe_;
    GuestSequence      guest_;
    std::uint64_t      instance_;
    std::uint64_t      physical_;
    std::uint64_t      device_;
};

av::LoaderHostApis &opened_host()
{ static av::LoaderHostApis host;
  host.open();
  return host; }

std::uint32_t lowest(std::uint32_t mask)
{ return mask != 0u ? static_cast<std::uint32_t>(__builtin_ctz(mask)) : 0u; }

bool same_kind(std::uint64_t physical, std::uint32_t mask, std::uint32_t type)
{ VkPhysicalDeviceMemoryProperties memory = {};
  ::vkGetPhysicalDeviceMemoryProperties(as_handle<VkPhysicalDevice>(physical), &memory);
  for (std::uint32_t named = 0u; named < memory.memoryTypeCount; named++)
      if ((mask >> named & 1u) != 0u &&
          memory.memoryTypes[named].propertyFlags == memory.memoryTypes[type].propertyFlags)
          return true;
  return false; }

bool free_memory(BackendTransport &wire, std::uint64_t device, std::uint64_t memory)
{ vkproto::av_vkFreeMemory(wire, as_handle<VkDevice>(device), as_handle<VkDeviceMemory>(memory), nullptr);
  return wire.ran(); }

}

TEST(vulkan_backend_device, a_guest_s_device_imports_source_memory_and_tears_down)
{ av::LoaderHostApis &host = opened_host();
  for (std::uint32_t i = 0u; i < host.gpu_count(); i++) {
      GuestDevice made(host, i);
      VulkanBackend &backend = made.backend();
      if (made.device() == 0u || made.mappable() == 0u) continue;
      CHECK(backend.knows_handle(made.device()));
      CHECK(made.probe().memory_import());

      Allocation   allocation(made.source(), 1u, lowest(made.mappable()));
      ImportedBlob blob;
      CHECK(allocation.take(blob, 1u));
      CHECK(backend.adopt(blob));
      CHECK(blob.object() != 0u);
      CHECK_EQ(made.probe().imports_held(), 1u);
      backend.abandon(blob);
      CHECK_EQ(blob.object(), 0u);
      backend.abandon(blob);
      CHECK_EQ(made.probe().imports_held(), 0u);

      CHECK(!made.guest().destroy(vkproto::kCmd_vkDestroyDevice, made.device() ^ 0x1000u));
      CHECK(made.guest().destroy(vkproto::kCmd_vkDestroyDevice, made.device()));
      CHECK(!backend.knows_handle(made.device()));
      CHECK(!backend.adopt(blob));
      allocation.give_back(blob);
  }
}

TEST(vulkan_backend_device, every_mappable_type_imports_as_itself_and_no_other_is_allocated)
{ av::LoaderHostApis &host = opened_host();
  for (std::uint32_t i = 0u; i < host.gpu_count(); i++) {
      GuestDevice made(host, i);
      VulkanBackend &backend = made.backend();
      if (made.device() == 0u || made.source() == nullptr) continue;
      const std::uint32_t mappable = made.mappable();
      CHECK(mappable != 0u);
      for (std::uint32_t type = 0u; type < made.memory_types(); type++) {
          Allocation allocation(made.source(), 100u + type, type);
          const bool may = ((mappable >> type) & 1u) != 0u;
          CHECK_EQ(allocation.made(), may);
          if (!may) continue;
          CHECK(same_kind(made.physical(), made.importable_dmabuf(allocation.descriptor()), type));
          ImportedBlob blob;
          CHECK(allocation.take(blob, 1u));
          CHECK(backend.adopt(blob));
          CHECK(blob.invariants_hold());
          CHECK(blob.object() != 0u);
          CHECK_EQ(blob.wire_handle(), blob.object());
          CHECK(backend.knows_handle(blob.wire_handle()));
          CHECK_EQ(made.probe().imports_held(), 1u);
          const std::uint64_t memory = blob.wire_handle();
          backend.abandon(blob);
          CHECK_EQ(blob.wire_handle(), 0u);
          CHECK(!backend.knows_handle(memory));
          CHECK_EQ(made.probe().imports_held(), 0u);
          allocation.give_back(blob);
      }
  }
}

TEST(vulkan_backend_device, memory_with_no_descriptor_to_share_is_not_adopted)
{ av::LoaderHostApis &host = opened_host();
  for (std::uint32_t i = 0u; i < host.gpu_count(); i++) {
      GuestDevice made(host, i);
      if (made.device() == 0u || made.mappable() == 0u) continue;
      alignas(4096) static std::uint8_t backing[page];
      av::SourceMemory memory(domain, 9u, lowest(made.mappable()), AV_MEMORY_WRITE_BACK,
                              av::ExportedMemory(0x1000u, -1, backing, page));
      const std::uint32_t known = made.probe().handles().count();
      ImportedBlob blob;
      CHECK_STATUS(blob.take(1u, memory), av::Status::ok);
      CHECK(!made.backend().adopt(blob));
      CHECK_EQ(blob.object(), 0u);
      CHECK_EQ(blob.wire_handle(), 0u);
      CHECK_EQ(made.probe().imports_held(), 0u);
      CHECK_EQ(made.probe().handles().count(), known);
      static_cast<void>(blob.give_back());
  }
}

TEST(vulkan_backend_device, a_device_naming_the_extensions_imports_and_a_refused_one_is_not_kept)
{ av::LoaderHostApis &host = opened_host();
  constexpr const char *named[] = {"VK_KHR_external_memory", "VK_KHR_external_memory_fd",
                                   "VK_EXT_external_memory_dma_buf"};
  constexpr const char *bogus[] = {"VK_accel_virt_no_such_extension"};
  constexpr const char *bogus_and_named[] = {"VK_accel_virt_no_such_extension", "VK_KHR_external_memory",
                                             "VK_EXT_external_memory_dma_buf"};
  for (std::uint32_t i = 0u; i < host.gpu_count(); i++) {
      GuestDevice made(host, i, 3u, named);
      if (made.device() == 0u || made.mappable() == 0u) continue;
      VulkanBackend &backend = made.backend();
      Allocation     allocation(made.source(), 1u, lowest(made.mappable()));
      ImportedBlob   blob;
      CHECK(allocation.take(blob, 1u));
      CHECK(backend.adopt(blob));
      backend.abandon(blob);
      allocation.give_back(blob);

      CHECK_EQ(made.guest().create_device(made.physical(), 1u, bogus), 0u);
      CHECK_EQ(made.guest().create_device(made.physical(), 3u, bogus_and_named), 0u);
      ImportedBlob again;
      CHECK(allocation.take(again, 2u));
      CHECK(backend.adopt(again));
      backend.abandon(again);
      allocation.give_back(again);

      const char *full[av::ExtensionList::capacity];
      for (const char *&name : full) name = "VK_KHR_maintenance1";
      const std::uint64_t unsent = made.guest().create_device(made.physical(), av::ExtensionList::capacity, full);
      CHECK(unsent != 0u);
      CHECK(!made.probe().memory_import());
      ImportedBlob none;
      CHECK(allocation.take(none, 3u));
      CHECK(!backend.adopt(none));
      CHECK_EQ(none.object(), 0u);
      allocation.give_back(none);
  }
}

TEST(vulkan_backend_device, the_gpu_copies_between_two_imported_pages_bound_through_the_wire)
{ av::LoaderHostApis &host = opened_host();
  for (std::uint32_t i = 0u; i < host.gpu_count(); i++) {
      GuestDevice made(host, i);
      if (made.device() == 0u || made.mappable() == 0u) continue;
      VulkanBackend &backend = made.backend();
      BackendTransport wire(backend);
      const VkDevice device = as_handle<VkDevice>(made.device());

      VkExternalMemoryBufferCreateInfo external = {};
      external.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO;
      external.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT;
      VkBufferCreateInfo create = {};
      create.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
      create.pNext = &external;
      create.size = page;
      create.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
      VkBuffer source = VK_NULL_HANDLE;
      CHECK_EQ(vkproto::av_vkCreateBuffer(wire, device, &create, nullptr, &source), VK_SUCCESS);
      create.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
      VkBuffer target = VK_NULL_HANDLE;
      CHECK_EQ(vkproto::av_vkCreateBuffer(wire, device, &create, nullptr, &target), VK_SUCCESS);
      VkMemoryRequirements requirements = {};
      vkproto::av_vkGetBufferMemoryRequirements(wire, device, source, &requirements);
      CHECK(requirements.size <= page);
      const av::GuestMemoryTypes &view = made.probe().types();
      std::uint32_t               type = av::GuestMemoryTypes::no_type;
      for (std::uint32_t guest = VK_MAX_MEMORY_TYPES; guest-- > 0u;)
          if ((requirements.memoryTypeBits >> guest & 1u) != 0u && view.source_type(guest) != av::GuestMemoryTypes::no_type)
              type = view.source_type(guest);
      CHECK(type != av::GuestMemoryTypes::no_type);

      Allocation   first(made.source(), 1u, type);
      Allocation   second(made.source(), 2u, type);
      Allocation  *allocations[2] = {&first, &second};
      std::uint8_t *const written = first.bytes();
      CHECK(written != nullptr);
      if (written == nullptr) continue;
      for (std::size_t b = 0u; b < page; b++) written[b] = static_cast<std::uint8_t>(b * 7u + 1u);

      ImportedBlob blobs[2];
      for (std::uint32_t b = 0u; b < 2u; b++) {
          CHECK(allocations[b]->take(blobs[b], b + 1u));
          CHECK(backend.adopt(blobs[b]));
      }
      CHECK_EQ(made.probe().imports_held(), 2u);
      CHECK_EQ(vkproto::av_vkBindBufferMemory(wire, device, source,
                                               as_handle<VkDeviceMemory>(blobs[0].wire_handle()), 0u),
               VK_SUCCESS);
      CHECK_EQ(vkproto::av_vkBindBufferMemory(wire, device, target,
                                               as_handle<VkDeviceMemory>(blobs[1].wire_handle()), 0u),
               VK_SUCCESS);

      std::uint64_t queue = 0u;
      vkproto::av_vkGetDeviceQueue(wire, device, 0u, 0u, &queue);
      CHECK(queue != 0u);
      VkCommandPoolCreateInfo pool_create = {};
      pool_create.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
      VkCommandPool pool = VK_NULL_HANDLE;
      CHECK_EQ(vkproto::av_vkCreateCommandPool(wire, device, &pool_create, nullptr, &pool), VK_SUCCESS);
      VkCommandBufferAllocateInfo allocate = {};
      allocate.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
      allocate.commandPool = pool;
      allocate.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
      allocate.commandBufferCount = 1u;
      std::uint64_t commands_raw = 0u;
      CHECK_EQ(vkproto::av_vkAllocateCommandBuffers(wire, device, &allocate, &commands_raw), VK_SUCCESS);
      const VkCommandBuffer commands = as_handle<VkCommandBuffer>(commands_raw);
      VkCommandBufferBeginInfo begin = {};
      begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
      CHECK_EQ(vkproto::av_vkBeginCommandBuffer(wire, commands, &begin), VK_SUCCESS);
      const VkBufferCopy region = {0u, 0u, page};
      vkproto::av_vkCmdCopyBuffer(wire, commands, source, target, 1u, &region);
      CHECK(wire.ran());
      CHECK_EQ(vkproto::av_vkEndCommandBuffer(wire, commands), VK_SUCCESS);
      VkSubmitInfo submit = {};
      submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
      submit.commandBufferCount = 1u;
      submit.pCommandBuffers = &commands;
      CHECK_EQ(vkproto::av_vkQueueSubmit(wire, as_handle<VkQueue>(queue), 1u, &submit, VK_NULL_HANDLE),
               VK_SUCCESS);
      CHECK_EQ(vkproto::av_vkQueueWaitIdle(wire, as_handle<VkQueue>(queue)), VK_SUCCESS);

      void *const window = ::mmap(nullptr, page, PROT_READ, MAP_SHARED, second.descriptor(), 0);
      CHECK(window != MAP_FAILED);
      if (window != MAP_FAILED) {
          CHECK(std::memcmp(window, written, page) == 0);
          CHECK_EQ(static_cast<const std::uint8_t *>(window)[page - 1u],
                   static_cast<std::uint8_t>((page - 1u) * 7u + 1u));
          ::munmap(window, page);
      }

      vkproto::av_vkDestroyCommandPool(wire, device, pool, nullptr);
      vkproto::av_vkDestroyBuffer(wire, device, source, nullptr);
      vkproto::av_vkDestroyBuffer(wire, device, target, nullptr);

      for (std::uint32_t b = 0u; b < 2u; b++) {
          const std::uint64_t memory = blobs[b].wire_handle();
          CHECK(free_memory(wire, made.device(), memory));
          CHECK_EQ(made.probe().imports_held(), 1u - b);
          CHECK(!backend.knows_handle(memory));
          CHECK_EQ(blobs[b].object(), 0u);
          CHECK(blobs[b].live());
          CHECK(blobs[b].invariants_hold());
          CHECK(!free_memory(wire, made.device(), memory));
          backend.abandon(blobs[b]);
          CHECK_EQ(made.probe().imports_held(), 1u - b);
          allocations[b]->give_back(blobs[b]);
      }
  }
}

TEST(vulkan_backend_device, freeing_memory_that_is_not_an_import_frees_it)
{ av::LoaderHostApis &host = opened_host();
  for (std::uint32_t i = 0u; i < host.gpu_count(); i++) {
      GuestDevice made(host, i);
      if (made.device() == 0u) continue;
      BackendTransport wire(made.backend());
      VkMemoryAllocateInfo allocate = {};
      allocate.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
      allocate.allocationSize = page;
      allocate.memoryTypeIndex = 0u;
      VkDeviceMemory memory = VK_NULL_HANDLE;
      CHECK_EQ(vkproto::av_vkAllocateMemory(wire, as_handle<VkDevice>(made.device()), &allocate, nullptr, &memory),
               VK_SUCCESS);
      std::uint64_t raw = 0u;
      std::memcpy(&raw, &memory, sizeof memory);
      CHECK(made.backend().knows_handle(raw));
      CHECK(free_memory(wire, made.device(), raw));
      CHECK(!made.backend().knows_handle(raw));
      CHECK(!free_memory(wire, made.device(), raw));
  }
}

TEST(vulkan_backend_device, an_import_with_no_room_to_be_named_is_refused_and_given_back)
{ av::LoaderHostApis &host = opened_host();
  if (host.gpu_count() == 0u) return;
  {
      GuestDevice made(host, 0u);
      if (made.device() == 0u || made.mappable() == 0u) return;
      VulkanBackend &backend = made.backend();
      Allocation     allocation(made.source(), 1u, lowest(made.mappable()));
      for (std::uint64_t raw = 1u; made.probe().handles().count() < av::HandleRegistry::max_handles; raw++)
          backend.made(raw);
      CHECK(!backend.lost());
      ImportedBlob blob;
      CHECK(allocation.take(blob, 1u));
      CHECK(!backend.adopt(blob));
      CHECK_EQ(blob.object(), 0u);
      CHECK_EQ(made.probe().imports_held(), 0u);
      CHECK(!backend.lost());
      allocation.give_back(blob);
  }
  {
      GuestDevice made(host, 0u);
      VulkanBackend &backend = made.backend();
      Allocation     allocation(made.source(), 2u, lowest(made.mappable()));
      static ImportedBlob blobs[av::ImportTable::max_imports + 1u];
      for (std::uint32_t b = 0u; b < av::ImportTable::max_imports; b++) {
          CHECK(allocation.take(blobs[b], b + 1u));
          CHECK(backend.adopt(blobs[b]));
      }
      ImportedBlob &extra = blobs[av::ImportTable::max_imports];
      CHECK(allocation.take(extra, 0xFFFFu));
      CHECK(!backend.adopt(extra));
      CHECK_EQ(extra.object(), 0u);
      CHECK_EQ(made.probe().imports_held(), av::ImportTable::max_imports);

      backend.abandon(blobs[0]);
      CHECK(backend.adopt(extra));
      for (ImportedBlob &blob : blobs) {
          backend.abandon(blob);
          allocation.give_back(blob);
      }
      CHECK_EQ(made.probe().imports_held(), 0u);
  }
}

TEST(vulkan_backend_device, shutting_down_destroys_a_device_the_guest_never_did)
{ av::LoaderHostApis &host = opened_host();
  for (std::uint32_t i = 0u; i < host.gpu_count(); i++) {
      GuestDevice made(host, i);
      if (made.device() == 0u) continue;
      made.backend().shut_down();
      CHECK(!made.backend().knows_handle(made.device()));
      CHECK(!made.backend().knows_handle(made.physical()));
  }
}

TEST(vulkan_backend_device, a_second_instance_and_device_are_the_ones_kept)
{ av::LoaderHostApis &host = opened_host();
  if (host.gpu_count() == 0u) return;
  av::HostGpu gpu;
  gpu.describe(host, 0u);
  VulkanBackend backend;
  backend.initialize(gpu, host.memory_source(0u));
  GuestSequence guest(backend);
  const std::uint64_t first = guest.create_instance();
  if (first == 0u) return;
  const std::uint64_t second = guest.create_instance();
  CHECK(second != 0u);
  CHECK(guest.destroy(vkproto::kCmd_vkDestroyInstance, first));
  CHECK(backend.knows_handle(second));

  const std::uint64_t physical = guest.first_physical_device(second);
  if (physical == 0u) { backend.shut_down(); return; }
  const std::uint64_t device_one = guest.create_device(physical);
  const std::uint64_t device_two = guest.create_device(physical);
  if (device_one == 0u || device_two == 0u) { backend.shut_down(); return; }
  CHECK(guest.destroy(vkproto::kCmd_vkDestroyDevice, device_one));
  CHECK(backend.knows_handle(device_two));

  std::uint8_t frame[32];
  WireWriter invented(frame, sizeof frame);
  invented.write_pod(device_two);
  invented.write_pod<std::uint64_t>(0xBEEFu);
  std::uint8_t reply[8] = {};
  std::size_t answered = 0u;
  CHECK(!guest.send(vkproto::kCmd_vkDestroyBuffer, frame, invented.offset(), reply,
                    sizeof reply, answered));
  WireWriter null_buffer(frame, sizeof frame);
  null_buffer.write_pod(device_two);
  null_buffer.write_pod<std::uint64_t>(0u);
  CHECK(guest.send(vkproto::kCmd_vkDestroyBuffer, frame, null_buffer.offset(), reply,
                   sizeof reply, answered));
  backend.shut_down();
}

TEST(vulkan_backend_device, memory_of_a_device_that_is_gone_is_not_freed_through_it)
{ av::LoaderHostApis &host = opened_host();
  for (std::uint32_t i = 0u; i < host.gpu_count(); i++) {
      GuestDevice made(host, i);
      if (made.device() == 0u || made.mappable() == 0u) continue;
      VulkanBackend &backend = made.backend();
      Allocation     allocation(made.source(), 1u, lowest(made.mappable()));
      ImportedBlob   blob;
      CHECK(allocation.take(blob, 1u));
      CHECK(backend.adopt(blob));
      const std::uint64_t memory = blob.object();
      CHECK(made.guest().destroy(vkproto::kCmd_vkDestroyDevice, made.device()));
      CHECK(!backend.knows_handle(memory));
      backend.abandon(blob);
      CHECK_EQ(blob.object(), memory);
      CHECK_EQ(made.probe().imports_held(), 1u);
      allocation.give_back(blob);
  }
}

TEST(vulkan_backend_device, a_buffer_this_context_made_is_accepted_back)
{ av::LoaderHostApis &host = opened_host();
  for (std::uint32_t i = 0u; i < host.gpu_count(); i++) {
      GuestDevice made(host, i);
      if (made.device() == 0u) continue;
      VulkanBackend &backend = made.backend();
      GuestSequence &guest = made.guest();
      std::uint8_t frame[256];
      WireWriter writer(frame, sizeof frame);
      writer.write_pod(made.device());
      VkBufferCreateInfo create = {};
      create.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
      create.size = page;
      create.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
      vkproto::serialize_VkBufferCreateInfo(writer, create);
      std::uint8_t reply[64] = {};
      std::size_t answered = 0u;
      CHECK(guest.send(vkproto::kCmd_vkCreateBuffer, frame, writer.offset(), reply, sizeof reply,
                       answered));
      WireReader read(reply, answered);
      std::int32_t returned = 0;
      std::uint64_t buffer = 0u;
      CHECK(read.read_pod(returned));
      CHECK(read.read_pod(buffer));
      CHECK_EQ(returned, 0);
      CHECK(backend.knows_handle(buffer));
      WireWriter destroy(frame, sizeof frame);
      destroy.write_pod(made.device());
      destroy.write_pod(buffer);
      CHECK(guest.send(vkproto::kCmd_vkDestroyBuffer, frame, destroy.offset(), reply,
                       sizeof reply, answered));
      CHECK(!backend.knows_handle(buffer));
      CHECK(!guest.send(vkproto::kCmd_vkDestroyBuffer, frame, destroy.offset(), reply,
                        sizeof reply, answered));
  }
}

TEST(vulkan_backend_device, a_buffer_of_a_destroyed_device_is_refused)
{ av::LoaderHostApis &host = opened_host();
  for (std::uint32_t i = 0u; i < host.gpu_count(); i++) {
      GuestDevice made(host, i);
      if (made.device() == 0u) continue;
      BackendTransport wire(made.backend());
      VkBufferCreateInfo create = {};
      create.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
      create.size = page;
      create.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
      VkBuffer buffer = VK_NULL_HANDLE;
      CHECK_EQ(vkproto::av_vkCreateBuffer(wire, as_handle<VkDevice>(made.device()), &create, nullptr, &buffer),
               VK_SUCCESS);
      std::uint64_t raw = 0u;
      std::memcpy(&raw, &buffer, sizeof buffer);
      CHECK(made.guest().destroy(vkproto::kCmd_vkDestroyDevice, made.device()));
      CHECK(!made.backend().knows_handle(raw));
      CHECK(made.backend().knows_handle(made.physical()));
      CHECK(made.guest().destroy(vkproto::kCmd_vkDestroyInstance, made.instance()));
      CHECK(!made.backend().knows_handle(made.physical()));
  }
}

namespace {

VkExternalMemoryBufferCreateInfo host_buffer()
{ VkExternalMemoryBufferCreateInfo external = {};
  external.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO;
  external.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT;
  return external; }

VkBufferCreateInfo storage_buffer()
{ VkBufferCreateInfo create = {};
  create.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  create.size = page;
  create.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
  return create; }

VkImageCreateInfo colour_image()
{ VkImageCreateInfo create = {};
  create.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  create.imageType = VK_IMAGE_TYPE_2D;
  create.format = VK_FORMAT_R8G8B8A8_UNORM;
  create.extent = {64u, 64u, 1u};
  create.mipLevels = 1u;
  create.arrayLayers = 1u;
  create.samples = VK_SAMPLE_COUNT_1_BIT;
  create.tiling = VK_IMAGE_TILING_OPTIMAL;
  create.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
  return create; }

std::uint32_t driver_buffer_types(std::uint64_t device, const VkBufferCreateInfo &create)
{ const VkDevice host = as_handle<VkDevice>(device);
  VkBuffer buffer = VK_NULL_HANDLE;
  if (::vkCreateBuffer(host, &create, nullptr, &buffer) != VK_SUCCESS) return 0u;
  VkMemoryRequirements requirements = {};
  ::vkGetBufferMemoryRequirements(host, buffer, &requirements);
  ::vkDestroyBuffer(host, buffer, nullptr);
  return requirements.memoryTypeBits; }

std::uint32_t driver_image_types(std::uint64_t device, const VkImageCreateInfo &create)
{ const VkDevice host = as_handle<VkDevice>(device);
  VkImage image = VK_NULL_HANDLE;
  if (::vkCreateImage(host, &create, nullptr, &image) != VK_SUCCESS) return 0u;
  VkMemoryRequirements requirements = {};
  ::vkGetImageMemoryRequirements(host, image, &requirements);
  ::vkDestroyImage(host, image, nullptr);
  return requirements.memoryTypeBits; }

bool driver_imports_image(std::uint64_t physical, const VkImageCreateInfo &create)
{ VkPhysicalDeviceExternalImageFormatInfo external = {};
  external.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_IMAGE_FORMAT_INFO;
  external.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT;
  VkPhysicalDeviceImageFormatInfo2 format = {};
  format.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2;
  format.pNext = &external;
  format.format = create.format;
  format.type = create.imageType;
  format.tiling = create.tiling;
  format.usage = create.usage;
  VkExternalImageFormatProperties answer = {};
  answer.sType = VK_STRUCTURE_TYPE_EXTERNAL_IMAGE_FORMAT_PROPERTIES;
  VkImageFormatProperties2 properties = {};
  properties.sType = VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2;
  properties.pNext = &answer;
  if (::vkGetPhysicalDeviceImageFormatProperties2(as_handle<VkPhysicalDevice>(physical), &format, &properties) !=
      VK_SUCCESS)
      return false;
  return (answer.externalMemoryProperties.externalMemoryFeatures & VK_EXTERNAL_MEMORY_FEATURE_IMPORTABLE_BIT) != 0u; }

std::uint32_t wire_buffer_types(BackendTransport &wire, std::uint64_t device, const VkBufferCreateInfo &create)
{ const VkDevice host = as_handle<VkDevice>(device);
  VkBuffer buffer = VK_NULL_HANDLE;
  if (vkproto::av_vkCreateBuffer(wire, host, &create, nullptr, &buffer) != VK_SUCCESS) return 0u;
  VkMemoryRequirements requirements = {};
  vkproto::av_vkGetBufferMemoryRequirements(wire, host, buffer, &requirements);
  vkproto::av_vkDestroyBuffer(wire, host, buffer, nullptr);
  return requirements.memoryTypeBits; }

std::uint32_t wire_image_types(BackendTransport &wire, std::uint64_t device, const VkImageCreateInfo &create)
{ const VkDevice host = as_handle<VkDevice>(device);
  VkImage image = VK_NULL_HANDLE;
  if (vkproto::av_vkCreateImage(wire, host, &create, nullptr, &image) != VK_SUCCESS) return 0u;
  VkMemoryRequirements requirements = {};
  vkproto::av_vkGetImageMemoryRequirements(wire, host, image, &requirements);
  vkproto::av_vkDestroyImage(wire, host, image, nullptr);
  return requirements.memoryTypeBits; }

}

TEST(vulkan_backend_device, buffers_and_images_are_made_for_host_allocation_where_the_driver_imports_them)
{ av::LoaderHostApis &host = opened_host();
  for (std::uint32_t i = 0u; i < host.gpu_count(); i++) {
      GuestDevice made(host, i);
      if (made.device() == 0u) continue;
      BackendTransport wire(made.backend());
      VkExternalMemoryBufferCreateInfo external = host_buffer();
      VkBufferCreateInfo asked = storage_buffer();
      VkBufferCreateInfo for_host = storage_buffer();
      for_host.pNext = &external;
      const std::uint32_t host_types = driver_buffer_types(made.device(), for_host);
      CHECK(host_types != 0u);
      CHECK_EQ(wire_buffer_types(wire, made.device(), asked), host_types);
      CHECK_EQ(wire_buffer_types(wire, made.device(), for_host), host_types);

      VkExternalMemoryImageCreateInfo image_external = {};
      image_external.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO;
      image_external.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT;
      const VkImageCreateInfo image = colour_image();
      VkImageCreateInfo image_for_host = colour_image();
      image_for_host.pNext = &image_external;
      const bool          imports = driver_imports_image(made.physical(), image);
      const std::uint32_t driver = imports ? driver_image_types(made.device(), image_for_host)
                                           : driver_image_types(made.device(), image);
      CHECK(driver != 0u);
      std::uint32_t native = 0u;
      VkPhysicalDeviceMemoryProperties guest = {};
      vkproto::av_vkGetPhysicalDeviceMemoryProperties(wire, as_handle<VkPhysicalDevice>(made.physical()), &guest);
      for (std::uint32_t type = 0u; type < guest.memoryTypeCount; type++)
          if ((guest.memoryTypes[type].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) == 0u) native |= 1u << type;
      CHECK(imports || (driver & native) != 0u);
      CHECK_EQ(wire_image_types(wire, made.device(), image), imports ? driver : driver & native);
  }
}

TEST(vulkan_backend_device, a_device_that_imports_nothing_makes_buffers_as_asked)
{ av::LoaderHostApis &host = opened_host();
  for (std::uint32_t i = 0u; i < host.gpu_count(); i++) {
      GuestDevice made(host, i);
      if (made.device() == 0u) continue;
      BackendTransport wire(made.backend());
      const VkBufferCreateInfo asked = storage_buffer();

      const std::uint64_t replaced = made.device();
      const char *full[av::ExtensionList::capacity];
      for (const char *&name : full) name = "VK_KHR_maintenance1";
      const std::uint64_t unsent = made.guest().create_device(made.physical(), av::ExtensionList::capacity, full);
      CHECK(!made.probe().memory_import());
      CHECK(unsent != 0u);
      CHECK_EQ(wire_buffer_types(wire, unsent, asked), driver_buffer_types(unsent, asked));
      CHECK_EQ(wire_buffer_types(wire, replaced, asked), driver_buffer_types(replaced, asked));
  }
}

TEST(vulkan_backend_device, a_replaced_device_makes_buffers_as_asked_while_its_replacement_imports)
{ av::LoaderHostApis &host = opened_host();
  for (std::uint32_t i = 0u; i < host.gpu_count(); i++) {
      GuestDevice made(host, i);
      if (made.device() == 0u) continue;
      BackendTransport wire(made.backend());
      const VkBufferCreateInfo asked = storage_buffer();
      VkExternalMemoryBufferCreateInfo external = host_buffer();
      VkBufferCreateInfo for_host = storage_buffer();
      for_host.pNext = &external;

      const std::uint64_t replaced = made.device();
      const std::uint64_t kept = made.guest().create_device(made.physical());
      CHECK(kept != 0u);
      CHECK_EQ(wire_buffer_types(wire, kept, asked), driver_buffer_types(kept, for_host));
      CHECK_EQ(wire_buffer_types(wire, replaced, asked), driver_buffer_types(replaced, asked));
  }
}

TEST(vulkan_backend_device, a_buffer_with_its_own_chain_is_still_made_for_host_allocation)
{ av::LoaderHostApis &host = opened_host();
  for (std::uint32_t i = 0u; i < host.gpu_count(); i++) {
      GuestDevice made(host, i);
      if (made.device() == 0u) continue;
      BackendTransport wire(made.backend());
      VkBufferOpaqueCaptureAddressCreateInfo capture = {};
      capture.sType = VK_STRUCTURE_TYPE_BUFFER_OPAQUE_CAPTURE_ADDRESS_CREATE_INFO;
      VkBufferCreateInfo asked = storage_buffer();
      asked.pNext = &capture;
      VkExternalMemoryBufferCreateInfo external = host_buffer();
      external.pNext = &capture;
      VkBufferCreateInfo for_host = storage_buffer();
      for_host.pNext = &external;
      const std::uint32_t host_types = driver_buffer_types(made.device(), for_host);
      CHECK(host_types != 0u);
      CHECK_EQ(wire_buffer_types(wire, made.device(), asked), host_types);
  }
}

TEST(vulkan_backend_device, an_image_laid_out_by_a_drm_modifier_is_made_without_asking_the_driver_about_it)
{ av::LoaderHostApis &host = opened_host();
  for (std::uint32_t i = 0u; i < host.gpu_count(); i++) {
      const char *const extensions[] = {"VK_EXT_image_drm_format_modifier"};
      GuestDevice made(host, i, 1u, extensions);
      if (made.device() == 0u) continue;
      BackendTransport wire(made.backend());
      const std::uint64_t linear = 0u;
      VkImageDrmFormatModifierListCreateInfoEXT modifiers = {};
      modifiers.sType = VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_LIST_CREATE_INFO_EXT;
      modifiers.drmFormatModifierCount = 1u;
      modifiers.pDrmFormatModifiers = &linear;
      VkImageCreateInfo create = colour_image();
      create.tiling = VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT;
      create.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
      create.pNext = &modifiers;
      if (driver_image_types(made.device(), create) == 0u) continue;
      const VkDevice device = as_handle<VkDevice>(made.device());
      VkImage image = VK_NULL_HANDLE;
      CHECK_EQ(vkproto::av_vkCreateImage(wire, device, &create, nullptr, &image), VK_SUCCESS);
      CHECK(made.backend().knows_handle(as_wire(image)));
      vkproto::av_vkDestroyImage(wire, device, image, nullptr);
  }
}

namespace {

VkPhysicalDeviceMemoryProperties driver_memory(std::uint64_t physical)
{ VkPhysicalDeviceMemoryProperties memory = {};
  ::vkGetPhysicalDeviceMemoryProperties(as_handle<VkPhysicalDevice>(physical), &memory);
  return memory; }

void check_view(const VkPhysicalDeviceMemoryProperties &guest, const VkPhysicalDeviceMemoryProperties &driver,
                const av::GuestMemoryTypes &types, std::uint32_t mappable)
{ constexpr VkMemoryPropertyFlags host_only = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                              VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
                                              VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
  CHECK_EQ(guest.memoryTypeCount, driver.memoryTypeCount);
  CHECK_EQ(guest.memoryHeapCount, driver.memoryHeapCount);
  std::uint32_t shown = 0u;
  for (std::uint32_t type = 0u; type < guest.memoryTypeCount; type++) {
      const std::uint32_t host = types.host_type(type);
      CHECK(host < driver.memoryTypeCount);
      if (host >= driver.memoryTypeCount) continue;
      shown |= 1u << host;
      const VkMemoryPropertyFlags flags = driver.memoryTypes[host].propertyFlags;
      const bool                  sourced = types.source_type(type) == host;
      CHECK(!sourced || (mappable >> host & 1u) != 0u);
      CHECK_EQ(guest.memoryTypes[type].propertyFlags, sourced ? flags : flags & ~host_only);
      CHECK_EQ(guest.memoryTypes[type].heapIndex, driver.memoryTypes[host].heapIndex);
      for (std::uint32_t earlier = 0u; earlier < type; earlier++) {
          const VkMemoryPropertyFlags before = guest.memoryTypes[earlier].propertyFlags;
          CHECK(!((before & guest.memoryTypes[type].propertyFlags) == guest.memoryTypes[type].propertyFlags &&
                  before != guest.memoryTypes[type].propertyFlags));
      }
  }
  CHECK_EQ(shown, (1u << driver.memoryTypeCount) - 1u);
  for (std::uint32_t heap = 0u; heap < driver.memoryHeapCount; heap++)
      CHECK_EQ(guest.memoryHeaps[heap].size, driver.memoryHeaps[heap].size); }

}

TEST(vulkan_backend_device, memory_is_host_visible_to_the_guest_only_where_the_source_maps_it)
{ av::LoaderHostApis &host = opened_host();
  av::MockPlatform       platform;
  av::MockMemoryExporter empty;
  av::HostMemorySource   typeless(empty, platform);
  for (std::uint32_t i = 0u; i < host.gpu_count(); i++) {
      if ((host.sub_apis(i) & AV_EXECUTOR_SUB_API_VULKAN) == 0u) continue;
      av::HostMemorySource *const source = host.memory_source(i);
      av::HostMemorySource *const sources[] = {source, nullptr, &typeless};
      for (av::HostMemorySource *const used : sources) {
          const std::uint32_t mappable = used != nullptr ? used->mappable_types() : 0u;
          av::HostGpu gpu;
          gpu.describe(host, i);
          VulkanBackend fresh;
          fresh.initialize(gpu, used);
          VulkanBackendProbe probe(fresh);
          GuestSequence  guest(fresh);
          BackendTransport wire(fresh);
          const std::uint64_t instance = guest.create_instance();
          const std::uint64_t physical = guest.first_physical_device(instance);
          CHECK(physical != 0u);
          if (physical == 0u) { fresh.shut_down(); continue; }
          const VkPhysicalDeviceMemoryProperties driver = driver_memory(physical);
          VkPhysicalDeviceMemoryProperties before = {};
          vkproto::av_vkGetPhysicalDeviceMemoryProperties(wire, as_handle<VkPhysicalDevice>(physical), &before);
          check_view(before, driver, probe.types(), mappable);
          av::GuestMemoryTypes sourced;
          if (used == source && source != nullptr && source->vulkan_types(sourced)) {
              VkPhysicalDeviceMemoryProperties expected = {};
              sourced.properties(expected);
              CHECK(std::memcmp(&expected, &before, sizeof before) == 0);
          }

          VkPhysicalDeviceMemoryProperties2 after = {};
          after.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2;
          CHECK(guest.create_device(physical) != 0u);
          vkproto::av_vkGetPhysicalDeviceMemoryProperties2(wire, as_handle<VkPhysicalDevice>(physical), &after);
          CHECK(std::memcmp(&after.memoryProperties, &before, sizeof before) == 0);
          VkPhysicalDeviceMemoryProperties2 khr = {};
          khr.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2;
          vkproto::av_vkGetPhysicalDeviceMemoryProperties2KHR(wire, as_handle<VkPhysicalDevice>(physical), &khr);
          CHECK(std::memcmp(&khr.memoryProperties, &before, sizeof before) == 0);
          fresh.shut_down();
      }
  }
}

TEST(vulkan_backend_device, a_template_the_device_cannot_track_is_refused_before_the_driver)
{ av::LoaderHostApis &host = opened_host();
  for (std::uint32_t i = 0u; i < host.gpu_count(); i++) {
      if ((host.sub_apis(i) & AV_EXECUTOR_SUB_API_VULKAN) == 0u) continue;
      GuestDevice made(host, i);
      if (made.device() == 0u) continue;

      VkDescriptorUpdateTemplateEntry hostile = {};
      hostile.descriptorCount = 2u;
      hostile.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
      hostile.offset = ~static_cast<std::size_t>(0);
      hostile.stride = sizeof(VkDescriptorImageInfo);
      CHECK_EQ(made.guest().create_template(made.device(), &hostile, 1u), VK_ERROR_INITIALIZATION_FAILED);

      VkDescriptorUpdateTemplateEntry one = {};
      one.descriptorCount = 1u;
      one.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
      one.stride = sizeof(VkDescriptorImageInfo);
      for (std::uint32_t taken = 0u; taken < av::HostTemplateTable::max_templates; taken++) {
          CHECK_EQ(made.probe().reserve_template(as_handle<VkDevice>(made.device()), &one), av::Status::ok);
          VulkanBackendProbe::name_template(made.backend(), as_handle<VkDescriptorUpdateTemplate>(0x5000u + taken));
      }
      CHECK_EQ(made.guest().create_template(made.device(), &one, 1u), VK_ERROR_OUT_OF_HOST_MEMORY);
  }
}

TEST(vulkan_backend_device, an_import_names_an_fd_this_context_exported)
{ av::LoaderHostApis &host = opened_host();
  for (std::uint32_t i = 0u; i < host.gpu_count(); i++) {
      if ((host.sub_apis(i) & AV_EXECUTOR_SUB_API_VULKAN) == 0u) continue;
      const char *const extensions[] = {"VK_KHR_external_memory_fd"};
      GuestDevice made(host, i, 1u, extensions);
      if (made.device() == 0u) continue;
      BackendTransport wire(made.backend());
      const VkDevice   device = as_handle<VkDevice>(made.device());

      VkExportMemoryAllocateInfo exportable = {};
      exportable.sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO;
      exportable.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;
      VkMemoryAllocateInfo allocate = {};
      allocate.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
      allocate.pNext = &exportable;
      allocate.allocationSize = page;
      allocate.memoryTypeIndex = 0u;
      VkDeviceMemory shared = VK_NULL_HANDLE;
      CHECK_EQ(vkproto::av_vkAllocateMemory(wire, device, &allocate, nullptr, &shared), VK_SUCCESS);

      VkMemoryGetFdInfoKHR get = {};
      get.sType = VK_STRUCTURE_TYPE_MEMORY_GET_FD_INFO_KHR;
      get.memory = shared;
      get.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;
      int fd = -1;
      if (vkproto::av_vkGetMemoryFdKHR(wire, device, &get, &fd) != VK_SUCCESS) continue;
      CHECK(fd >= 0);
      CHECK(made.probe().holds_fd(fd));

      VkImportMemoryFdInfoKHR forged = {};
      forged.sType = VK_STRUCTURE_TYPE_IMPORT_MEMORY_FD_INFO_KHR;
      forged.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;
      forged.fd = fd + 4096;
      VkMemoryAllocateInfo importing = allocate;
      importing.pNext = &forged;
      VkDeviceMemory imported = VK_NULL_HANDLE;
      CHECK_EQ(vkproto::av_vkAllocateMemory(wire, device, &importing, nullptr, &imported),
               VK_ERROR_INVALID_EXTERNAL_HANDLE);
      CHECK(imported == VK_NULL_HANDLE);

      VkImportMemoryFdInfoKHR import = forged;
      import.fd = fd;
      importing.pNext = &import;
      CHECK_EQ(vkproto::av_vkAllocateMemory(wire, device, &importing, nullptr, &imported), VK_SUCCESS);
      CHECK(!made.probe().holds_fd(fd));
      VkDeviceMemory again = VK_NULL_HANDLE;
      CHECK_EQ(vkproto::av_vkAllocateMemory(wire, device, &importing, nullptr, &again),
               VK_ERROR_INVALID_EXTERNAL_HANDLE);

      VkDeviceMemory plain = VK_NULL_HANDLE;
      VkMemoryAllocateInfo alone = {};
      alone.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
      alone.allocationSize = page;
      CHECK_EQ(vkproto::av_vkAllocateMemory(wire, device, &alone, nullptr, &plain), VK_SUCCESS);
      VkMemoryAllocateInfo absurd = alone;
      absurd.allocationSize = ~static_cast<VkDeviceSize>(0) / 2u;
      VkDeviceMemory none = VK_NULL_HANDLE;
      CHECK(vkproto::av_vkAllocateMemory(wire, device, &absurd, nullptr, &none) != VK_SUCCESS);
  }
}

TEST(vulkan_backend_device, shutting_down_closes_every_fd_the_context_exported)
{ av::LoaderHostApis &host = opened_host();
  for (std::uint32_t i = 0u; i < host.gpu_count(); i++) {
      if ((host.sub_apis(i) & AV_EXECUTOR_SUB_API_VULKAN) == 0u) continue;
      const char *const extensions[] = {"VK_KHR_external_memory_fd"};
      GuestDevice made(host, i, 1u, extensions);
      if (made.device() == 0u) continue;
      BackendTransport wire(made.backend());
      const VkDevice   device = as_handle<VkDevice>(made.device());
      VkExportMemoryAllocateInfo exportable = {};
      exportable.sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO;
      exportable.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;
      VkMemoryAllocateInfo allocate = {};
      allocate.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
      allocate.pNext = &exportable;
      allocate.allocationSize = page;
      VkDeviceMemory shared = VK_NULL_HANDLE;
      CHECK_EQ(vkproto::av_vkAllocateMemory(wire, device, &allocate, nullptr, &shared), VK_SUCCESS);
      VkMemoryGetFdInfoKHR get = {};
      get.sType = VK_STRUCTURE_TYPE_MEMORY_GET_FD_INFO_KHR;
      get.memory = shared;
      get.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;
      int fd = -1;
      if (vkproto::av_vkGetMemoryFdKHR(wire, device, &get, &fd) != VK_SUCCESS) continue;
      CHECK(made.probe().holds_fd(fd));
      made.backend().shut_down();
      CHECK(!made.probe().holds_fd(fd));
      CHECK(::fcntl(fd, F_GETFD) == -1);
  }
}

TEST(vulkan_backend_device, a_semaphore_import_names_an_fd_this_context_exported)
{ av::LoaderHostApis &host = opened_host();
  for (std::uint32_t i = 0u; i < host.gpu_count(); i++) {
      if ((host.sub_apis(i) & AV_EXECUTOR_SUB_API_VULKAN) == 0u) continue;
      const char *const extensions[] = {"VK_KHR_external_semaphore_fd"};
      GuestDevice made(host, i, 1u, extensions);
      if (made.device() == 0u) continue;
      BackendTransport wire(made.backend());
      const VkDevice   device = as_handle<VkDevice>(made.device());

      VkSemaphoreCreateInfo create = {};
      create.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
      VkSemaphore semaphore = VK_NULL_HANDLE;
      CHECK_EQ(vkproto::av_vkCreateSemaphore(wire, device, &create, nullptr, &semaphore), VK_SUCCESS);
      VkImportSemaphoreFdInfoKHR importing = {};
      importing.sType = VK_STRUCTURE_TYPE_IMPORT_SEMAPHORE_FD_INFO_KHR;
      importing.semaphore = semaphore;
      importing.handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT;
      importing.fd = 4096;
      CHECK_EQ(vkproto::av_vkImportSemaphoreFdKHR(wire, device, &importing), VK_ERROR_INVALID_EXTERNAL_HANDLE);
      vkproto::av_vkDestroySemaphore(wire, device, semaphore, nullptr);

      int nothing = -1;
      CHECK_EQ(made.probe().export_answered(VK_ERROR_INVALID_EXTERNAL_HANDLE, nothing),
               VK_ERROR_INVALID_EXTERNAL_HANDLE);
      CHECK(!made.probe().holds_fd(nothing));
  }
}

TEST(vulkan_backend_device, exports_past_what_the_table_holds_are_refused)
{ av::LoaderHostApis &host = opened_host();
  for (std::uint32_t i = 0u; i < host.gpu_count(); i++) {
      if ((host.sub_apis(i) & AV_EXECUTOR_SUB_API_VULKAN) == 0u) continue;
      const char *const extensions[] = {"VK_KHR_external_memory_fd"};
      GuestDevice made(host, i, 1u, extensions);
      if (made.device() == 0u) continue;
      BackendTransport wire(made.backend());
      const VkDevice   device = as_handle<VkDevice>(made.device());

      VkExportMemoryAllocateInfo exportable = {};
      exportable.sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO;
      exportable.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;
      VkMemoryAllocateInfo allocate = {};
      allocate.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
      allocate.pNext = &exportable;
      allocate.allocationSize = page;
      VkDeviceMemory shared = VK_NULL_HANDLE;
      CHECK_EQ(vkproto::av_vkAllocateMemory(wire, device, &allocate, nullptr, &shared), VK_SUCCESS);

      VkMemoryGetFdInfoKHR get = {};
      get.sType = VK_STRUCTURE_TYPE_MEMORY_GET_FD_INFO_KHR;
      get.memory = shared;
      get.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;
      int first = -1;
      if (vkproto::av_vkGetMemoryFdKHR(wire, device, &get, &first) != VK_SUCCESS) continue;
      for (std::uint32_t taken = 1u; taken < av::HostExportedFds::max_fds; taken++) {
          int more = -1;
          CHECK_EQ(vkproto::av_vkGetMemoryFdKHR(wire, device, &get, &more), VK_SUCCESS);
      }
      int past = -1;
      CHECK_EQ(vkproto::av_vkGetMemoryFdKHR(wire, device, &get, &past), VK_ERROR_TOO_MANY_OBJECTS);
      CHECK(!made.probe().holds_fd(past));
      VkDeviceMemory plain = VK_NULL_HANDLE;
      VkMemoryAllocateInfo alone = {};
      alone.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
      alone.allocationSize = page;
      CHECK_EQ(vkproto::av_vkAllocateMemory(wire, device, &alone, nullptr, &plain), VK_SUCCESS);
      VkMemoryGetFdInfoKHR unshared = get;
      unshared.memory = plain;
      int refused = -1;
      CHECK(vkproto::av_vkGetMemoryFdKHR(wire, device, &unshared, &refused) != VK_SUCCESS);
  }
}

TEST(vulkan_backend_device, a_guest_list_with_no_room_left_is_sent_as_the_guest_wrote_it)
{ av::LoaderHostApis &host = opened_host();
  static char        spelled[av::ExtensionList::capacity][32] = {};
  const char        *names[av::ExtensionList::capacity] = {};
  for (std::uint32_t index = 0u; index < av::ExtensionList::capacity; index++) {
      std::snprintf(spelled[index], sizeof spelled[index], "VK_AV_no_such_extension_%u", index);
      names[index] = spelled[index];
  }
  for (std::uint32_t i = 0u; i < host.gpu_count(); i++) {
      GuestDevice made(host, i, av::ExtensionList::capacity, names);
      CHECK(made.instance() != 0u);
      CHECK(made.physical() != 0u);
      CHECK_EQ(made.device(), 0u);
  }
}


TEST(vulkan_backend_device, shares_memory_as_a_dma_buf)
{ av::LoaderHostApis &host = opened_host();
  for (std::uint32_t i = 0u; i < host.gpu_count(); i++) {
      if ((host.sub_apis(i) & AV_EXECUTOR_SUB_API_VULKAN) == 0u) continue;
      const char *const extensions[] = {"VK_KHR_external_memory_fd", "VK_EXT_external_memory_dma_buf"};
      GuestDevice made(host, i, 2u, extensions);
      if (made.device() == 0u) continue;
      BackendTransport wire(made.backend());
      const VkDevice   device = as_handle<VkDevice>(made.device());

      VkExportMemoryAllocateInfo exportable = {};
      exportable.sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO;
      exportable.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT;
      VkMemoryAllocateInfo allocate = {};
      allocate.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
      allocate.pNext = &exportable;
      allocate.allocationSize = page;
      allocate.memoryTypeIndex = 0u;
      VkDeviceMemory shared = VK_NULL_HANDLE;
      CHECK_EQ(vkproto::av_vkAllocateMemory(wire, device, &allocate, nullptr, &shared), VK_SUCCESS);
      const std::uint64_t handle = as_wire(shared);

      std::uint32_t handle_kind = 9u;
      int           fd = 9;
      CHECK(!made.backend().export_object(AV_SHARED_MEMORY, handle + 0x1000u, handle_kind, fd));
      CHECK_EQ(handle_kind, 0u);
      CHECK_EQ(fd, -1);
      CHECK(!made.backend().export_object(AV_SHARED_MEMORY, 0u, handle_kind, fd));
      CHECK(!made.backend().export_object(AV_SHARED_NATIVE_FENCE + 1u, handle, handle_kind, fd));

      if (!made.backend().export_object(AV_SHARED_MEMORY, handle, handle_kind, fd)) continue;
      CHECK_EQ(handle_kind, AV_SHARED_HANDLE_DMA_BUF);
      CHECK(fd >= 0);

      std::uint64_t imported = 9u;
      CHECK(!made.backend().import_object(AV_SHARED_MEMORY, handle_kind, -1, page, av::SharedImageShape(), imported, detail));
      CHECK_EQ(imported, 0u);
      CHECK(!made.backend().import_object(AV_SHARED_MEMORY, AV_SHARED_HANDLE_OPAQUE_FD, fd, page, av::SharedImageShape(),
                                          imported, detail));
      CHECK(!made.backend().import_object(AV_SHARED_MEMORY, handle_kind, fd, 0u, av::SharedImageShape(), imported, detail));
      CHECK(!made.backend().import_object(AV_SHARED_NATIVE_FENCE + 1u, handle_kind, fd, page, av::SharedImageShape(), imported, detail));

      CHECK(made.backend().import_object(AV_SHARED_MEMORY, handle_kind, fd, page, av::SharedImageShape(), imported, detail));
      CHECK(imported != 0u);
      CHECK(imported != handle);
      CHECK(made.probe().handles().knows(imported));
      made.backend().release_object(AV_SHARED_MEMORY, imported, detail);
      CHECK(!made.probe().handles().knows(imported));
      CHECK(::fcntl(fd, F_GETFD) != -1);
      made.backend().release_object(AV_SHARED_MEMORY, handle + 0x1000u, detail);
      made.backend().release_object(AV_SHARED_MEMORY, 0u, detail);
      ::close(fd);
  }
}

TEST(vulkan_backend_device, a_refused_export_and_an_unimportable_descriptor_are_answered_not_invented)
{ av::LoaderHostApis &host = opened_host();
  for (std::uint32_t i = 0u; i < host.gpu_count(); i++) {
      if ((host.sub_apis(i) & AV_EXECUTOR_SUB_API_VULKAN) == 0u) continue;
      const char *const extensions[] = {"VK_KHR_external_memory_fd", "VK_EXT_external_memory_dma_buf"};
      GuestDevice made(host, i, 2u, extensions);
      if (made.device() == 0u) continue;
      BackendTransport wire(made.backend());
      const VkDevice   device = as_handle<VkDevice>(made.device());

      VkMemoryAllocateInfo allocate = {};
      allocate.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
      allocate.allocationSize = page;
      allocate.memoryTypeIndex = 0u;
      VkDeviceMemory plain = VK_NULL_HANDLE;
      CHECK_EQ(vkproto::av_vkAllocateMemory(wire, device, &allocate, nullptr, &plain), VK_SUCCESS);

      int ends[2] = {-1, -1};
      CHECK_EQ(::pipe(ends), 0);
      ::close(ends[1]);
      std::uint64_t imported = 9u;
      CHECK(!made.backend().import_object(AV_SHARED_MEMORY, AV_SHARED_HANDLE_DMA_BUF, ends[0], page, av::SharedImageShape(),
                                          imported, detail));
      CHECK_EQ(imported, 0u);
      ::close(ends[0]);

      CHECK(made.probe().handles().knows(as_wire(plain)));
      made.backend().release_object(AV_SHARED_MEMORY, as_wire(plain), detail);
      CHECK(!made.probe().handles().knows(as_wire(plain)));
  }
}

TEST(vulkan_backend_device, exports_and_imports_a_fence_as_an_opaque_fd_only)
{ av::LoaderHostApis &host = opened_host();
  for (std::uint32_t i = 0u; i < host.gpu_count(); i++) {
      if ((host.sub_apis(i) & AV_EXECUTOR_SUB_API_VULKAN) == 0u) continue;
      const char *const extensions[] = {"VK_KHR_external_semaphore_fd"};
      GuestDevice made(host, i, 1u, extensions);
      if (made.device() == 0u) continue;
      BackendTransport wire(made.backend());
      const VkDevice   device = as_handle<VkDevice>(made.device());

      VkSemaphoreCreateInfo plain_create = {};
      plain_create.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
      VkSemaphore plain = VK_NULL_HANDLE;
      CHECK_EQ(vkproto::av_vkCreateSemaphore(wire, device, &plain_create, nullptr, &plain), VK_SUCCESS);
      made.backend().release_object(AV_SHARED_MONITORED_FENCE, as_wire(plain), detail);
      CHECK(!made.probe().handles().knows(as_wire(plain)));

      std::uint32_t handle_kind = 9u;
      int           fd = 9;
      VkExportSemaphoreCreateInfo exportable = {};
      exportable.sType = VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO;
      exportable.handleTypes = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT;
      VkSemaphoreTypeCreateInfo timeline = {};
      timeline.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
      timeline.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
      timeline.pNext = &exportable;
      VkSemaphoreCreateInfo create = {};
      create.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
      create.pNext = &timeline;
      VkSemaphore semaphore = VK_NULL_HANDLE;
      CHECK_EQ(vkproto::av_vkCreateSemaphore(wire, device, &create, nullptr, &semaphore), VK_SUCCESS);

      if (!made.backend().export_object(AV_SHARED_NATIVE_FENCE, as_wire(semaphore), handle_kind, fd))
          continue;
      CHECK_EQ(handle_kind, AV_SHARED_HANDLE_OPAQUE_FD);
      CHECK(fd >= 0);

      std::uint64_t imported = 9u;
      CHECK(!made.backend().import_object(AV_SHARED_MONITORED_FENCE, AV_SHARED_HANDLE_DMA_BUF, fd, 0u, av::SharedImageShape(),
                                          imported, detail));
      CHECK_EQ(imported, 0u);
      int ends[2] = {-1, -1};
      CHECK_EQ(::pipe(ends), 0);
      ::close(ends[1]);
      CHECK(!made.backend().import_object(AV_SHARED_MONITORED_FENCE, AV_SHARED_HANDLE_OPAQUE_FD, ends[0],
                                          0u, av::SharedImageShape(), imported, detail));
      CHECK_EQ(imported, 0u);
      ::close(ends[0]);

      CHECK(made.backend().import_object(AV_SHARED_NATIVE_FENCE, handle_kind, fd, 0u, av::SharedImageShape(), imported, detail));
      CHECK(imported != 0u);
      CHECK(made.probe().handles().knows(imported));
      made.backend().release_object(AV_SHARED_NATIVE_FENCE, imported, detail);
      CHECK(!made.probe().handles().knows(imported));
      CHECK(::fcntl(fd, F_GETFD) != -1);
      ::close(fd);
  }
}

TEST(vulkan_backend_device, a_device_without_the_fd_extensions_shares_nothing_either_way)
{ av::LoaderHostApis &host = opened_host();
  for (std::uint32_t i = 0u; i < host.gpu_count(); i++) {
      if ((host.sub_apis(i) & AV_EXECUTOR_SUB_API_VULKAN) == 0u) continue;
      GuestDevice made(host, i, 3u, nullptr);
      if (made.device() == 0u) continue;
      CHECK(!made.probe().memory_import());
      BackendTransport wire(made.backend());
      const VkDevice   device = as_handle<VkDevice>(made.device());

      VkMemoryAllocateInfo allocate = {};
      allocate.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
      allocate.allocationSize = page;
      allocate.memoryTypeIndex = 0u;
      VkDeviceMemory memory = VK_NULL_HANDLE;
      CHECK_EQ(vkproto::av_vkAllocateMemory(wire, device, &allocate, nullptr, &memory), VK_SUCCESS);
      VkSemaphoreCreateInfo create = {};
      create.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
      VkSemaphore semaphore = VK_NULL_HANDLE;
      CHECK_EQ(vkproto::av_vkCreateSemaphore(wire, device, &create, nullptr, &semaphore), VK_SUCCESS);

      std::uint32_t handle_kind = 9u;
      int           fd = 9;
      CHECK(!made.backend().export_object(AV_SHARED_MEMORY, as_wire(memory), handle_kind, fd));
      CHECK_EQ(handle_kind, 0u);
      CHECK_EQ(fd, -1);
      CHECK(!made.backend().export_object(AV_SHARED_MONITORED_FENCE, as_wire(semaphore), handle_kind, fd));
      CHECK_EQ(fd, -1);

      int ends[2] = {-1, -1};
      CHECK_EQ(::pipe(ends), 0);
      ::close(ends[1]);
      std::uint64_t imported = 9u;
      CHECK(!made.backend().import_object(AV_SHARED_NATIVE_FENCE, AV_SHARED_HANDLE_OPAQUE_FD, ends[0],
                                          0u, av::SharedImageShape(), imported, detail));
      CHECK_EQ(imported, 0u);
      CHECK(!made.backend().import_object(AV_SHARED_MEMORY, AV_SHARED_HANDLE_DMA_BUF, ends[0], page, av::SharedImageShape(),
                                          imported, detail));
      CHECK_EQ(imported, 0u);
      ::close(ends[0]);
  }
}

namespace {
std::uint32_t show_integrated(GuestDevice &made)
{ VkPhysicalDeviceMemoryProperties layout = driver_memory(made.physical());
  std::uint32_t backed = 0u;
  for (std::uint32_t type = 0u; type < layout.memoryTypeCount; type++) {
      VkMemoryPropertyFlags &flags = layout.memoryTypes[type].propertyFlags;
      if (flags == VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
          flags |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
      if ((flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0u) backed |= 1u << type;
  }
  made.probe().show_types(layout, backed);
  return layout.memoryTypeCount; }

VkPhysicalDeviceMemoryProperties wire_memory(BackendTransport &wire, std::uint64_t physical)
{ VkPhysicalDeviceMemoryProperties memory = {};
  vkproto::av_vkGetPhysicalDeviceMemoryProperties(wire, as_handle<VkPhysicalDevice>(physical), &memory);
  return memory; }

std::uint32_t over(GuestDevice &made, const VkPhysicalDeviceMemoryProperties &guest, std::uint32_t host_bits,
                   bool native_only)
{ std::uint32_t bits = 0u;
  for (std::uint32_t type = 0u; type < guest.memoryTypeCount; type++) {
      if ((host_bits >> made.probe().types().host_type(type) & 1u) == 0u) continue;
      if (native_only && (guest.memoryTypes[type].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0u)
          continue;
      bits |= 1u << type;
  }
  return bits; }

struct ImageAnswers {
    std::uint32_t bits[5];
};

ImageAnswers wire_image_answers(BackendTransport &wire, std::uint64_t device, const VkImageCreateInfo &create)
{ const VkDevice host = as_handle<VkDevice>(device);
  ImageAnswers answers = {};
  VkImage image = VK_NULL_HANDLE;
  if (vkproto::av_vkCreateImage(wire, host, &create, nullptr, &image) != VK_SUCCESS) return answers;
  VkMemoryRequirements one = {};
  vkproto::av_vkGetImageMemoryRequirements(wire, host, image, &one);
  answers.bits[0] = one.memoryTypeBits;
  VkImageMemoryRequirementsInfo2 info = {};
  info.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2;
  info.image = image;
  VkMemoryRequirements2 two = {};
  two.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2;
  vkproto::av_vkGetImageMemoryRequirements2(wire, host, &info, &two);
  answers.bits[1] = two.memoryRequirements.memoryTypeBits;
  two = VkMemoryRequirements2{VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2, nullptr, {}};
  vkproto::av_vkGetImageMemoryRequirements2KHR(wire, host, &info, &two);
  answers.bits[2] = two.memoryRequirements.memoryTypeBits;
  vkproto::av_vkDestroyImage(wire, host, image, nullptr);
  VkDeviceImageMemoryRequirements asked = {};
  asked.sType = VK_STRUCTURE_TYPE_DEVICE_IMAGE_MEMORY_REQUIREMENTS;
  asked.pCreateInfo = &create;
  two = VkMemoryRequirements2{VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2, nullptr, {}};
  vkproto::av_vkGetDeviceImageMemoryRequirements(wire, host, &asked, &two);
  answers.bits[3] = two.memoryRequirements.memoryTypeBits;
  two = VkMemoryRequirements2{VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2, nullptr, {}};
  vkproto::av_vkGetDeviceImageMemoryRequirementsKHR(wire, host, &asked, &two);
  answers.bits[4] = two.memoryRequirements.memoryTypeBits;
  return answers; }

ImageAnswers wire_buffer_answers(BackendTransport &wire, std::uint64_t device, const VkBufferCreateInfo &create)
{ const VkDevice host = as_handle<VkDevice>(device);
  ImageAnswers answers = {};
  VkBuffer buffer = VK_NULL_HANDLE;
  if (vkproto::av_vkCreateBuffer(wire, host, &create, nullptr, &buffer) != VK_SUCCESS) return answers;
  VkMemoryRequirements one = {};
  vkproto::av_vkGetBufferMemoryRequirements(wire, host, buffer, &one);
  answers.bits[0] = one.memoryTypeBits;
  VkBufferMemoryRequirementsInfo2 info = {};
  info.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_REQUIREMENTS_INFO_2;
  info.buffer = buffer;
  VkMemoryRequirements2 two = {};
  two.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2;
  vkproto::av_vkGetBufferMemoryRequirements2(wire, host, &info, &two);
  answers.bits[1] = two.memoryRequirements.memoryTypeBits;
  two = VkMemoryRequirements2{VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2, nullptr, {}};
  vkproto::av_vkGetBufferMemoryRequirements2KHR(wire, host, &info, &two);
  answers.bits[2] = two.memoryRequirements.memoryTypeBits;
  vkproto::av_vkDestroyBuffer(wire, host, buffer, nullptr);
  VkDeviceBufferMemoryRequirements asked = {};
  asked.sType = VK_STRUCTURE_TYPE_DEVICE_BUFFER_MEMORY_REQUIREMENTS;
  asked.pCreateInfo = &create;
  two = VkMemoryRequirements2{VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2, nullptr, {}};
  vkproto::av_vkGetDeviceBufferMemoryRequirements(wire, host, &asked, &two);
  answers.bits[3] = two.memoryRequirements.memoryTypeBits;
  two = VkMemoryRequirements2{VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2, nullptr, {}};
  vkproto::av_vkGetDeviceBufferMemoryRequirementsKHR(wire, host, &asked, &two);
  answers.bits[4] = two.memoryRequirements.memoryTypeBits;
  return answers; }

void check_every_spelling(const ImageAnswers &answers, std::uint32_t expected)
{ for (const std::uint32_t bits : answers.bits) CHECK_EQ(bits, expected); }

}

TEST(vulkan_backend_device, device_local_memory_the_guest_never_maps_is_the_driver_s_own_allocation)
{ av::LoaderHostApis &host = opened_host();
  for (std::uint32_t i = 0u; i < host.gpu_count(); i++) {
      GuestDevice made(host, i);
      if (made.device() == 0u) continue;
      const std::uint32_t driver_types = show_integrated(made);
      BackendTransport wire(made.backend());
      const VkDevice device = as_handle<VkDevice>(made.device());
      const VkPhysicalDeviceMemoryProperties guest = wire_memory(wire, made.physical());
      CHECK_EQ(guest.memoryTypeCount, driver_types + 1u);
      CHECK_EQ(guest.memoryTypes[0].propertyFlags, VkMemoryPropertyFlags{VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT});
      const std::uint32_t live = made.source() != nullptr ? made.source()->live() : 0u;

      VkMemoryAllocateInfo allocate = {};
      allocate.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
      allocate.allocationSize = page;
      allocate.memoryTypeIndex = 0u;
      VkDeviceMemory memory = VK_NULL_HANDLE;
      CHECK_EQ(vkproto::av_vkAllocateMemory(wire, device, &allocate, nullptr, &memory), VK_SUCCESS);
      CHECK(made.backend().knows_handle(as_wire(memory)));
      CHECK_EQ(made.probe().imports_held(), 0u);
      CHECK_EQ(made.source() != nullptr ? made.source()->live() : 0u, live);

      VkBufferCreateInfo create = storage_buffer();
      VkBuffer buffer = VK_NULL_HANDLE;
      CHECK_EQ(vkproto::av_vkCreateBuffer(wire, device, &create, nullptr, &buffer), VK_SUCCESS);
      VkMemoryRequirements needs = {};
      vkproto::av_vkGetBufferMemoryRequirements(wire, device, buffer, &needs);
      CHECK((needs.memoryTypeBits & 1u) != 0u);
      CHECK_EQ(vkproto::av_vkBindBufferMemory(wire, device, buffer, memory, 0u), VK_SUCCESS);
      vkproto::av_vkDestroyBuffer(wire, device, buffer, nullptr);
      vkproto::av_vkFreeMemory(wire, device, memory, nullptr);

      const std::uint32_t outside[] = {guest.memoryTypeCount, VK_MAX_MEMORY_TYPES, 0xFFFFFFFFu};
      for (const std::uint32_t type : outside) {
          allocate.memoryTypeIndex = type;
          VkDeviceMemory refused = VK_NULL_HANDLE;
          CHECK_EQ(vkproto::av_vkAllocateMemory(wire, device, &allocate, nullptr, &refused),
                   VK_ERROR_OUT_OF_DEVICE_MEMORY);
          CHECK(refused == VK_NULL_HANDLE);
      }
  }
}

TEST(vulkan_backend_device, an_image_not_made_for_the_source_is_offered_no_memory_the_guest_maps)
{ av::LoaderHostApis &host = opened_host();
  for (std::uint32_t i = 0u; i < host.gpu_count(); i++) {
      GuestDevice made(host, i);
      if (made.device() == 0u) continue;
      show_integrated(made);
      BackendTransport wire(made.backend());
      const VkPhysicalDeviceMemoryProperties guest = wire_memory(wire, made.physical());

      VkImageFormatListCreateInfo listed = {};
      listed.sType = VK_STRUCTURE_TYPE_IMAGE_FORMAT_LIST_CREATE_INFO;
      const VkFormat formats[] = {VK_FORMAT_R8G8B8A8_UNORM};
      listed.viewFormatCount = 1u;
      listed.pViewFormats = formats;
      VkExternalMemoryImageCreateInfo opaque = {};
      opaque.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO;
      opaque.pNext = &listed;
      opaque.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;
      VkImageCreateInfo images[] = {colour_image(), colour_image(), colour_image()};
      images[1].pNext = &listed;
      images[2].pNext = &opaque;
      for (const VkImageCreateInfo &image : images) {
          if (driver_imports_image(made.physical(), image)) continue;
          const std::uint32_t native = over(made, guest, driver_image_types(made.device(), image), true);
          CHECK((native & 1u) != 0u);
          check_every_spelling(wire_image_answers(wire, made.device(), image), native);
      }

      VkExternalMemoryBufferCreateInfo external = host_buffer();
      VkBufferCreateInfo for_host = storage_buffer();
      for_host.pNext = &external;
      const std::uint32_t every = over(made, guest, driver_buffer_types(made.device(), for_host), false);
      CHECK((every & 1u) != 0u);
      CHECK((every & ~over(made, guest, 0xFFFFFFFFu, true)) != 0u);
      check_every_spelling(wire_buffer_answers(wire, made.device(), storage_buffer()), every);
  }
}

TEST(vulkan_backend_device, an_image_made_for_the_source_keeps_the_types_the_source_backs)
{ av::LoaderHostApis &host = opened_host();
  for (std::uint32_t i = 0u; i < host.gpu_count(); i++) {
      const char *const extensions[] = {"VK_EXT_image_drm_format_modifier"};
      GuestDevice made(host, i, 1u, extensions);
      if (made.device() == 0u) continue;
      show_integrated(made);
      BackendTransport wire(made.backend());
      const VkPhysicalDeviceMemoryProperties guest = wire_memory(wire, made.physical());
      const std::uint64_t linear = 0u;
      VkImageDrmFormatModifierListCreateInfoEXT modifiers = {};
      modifiers.sType = VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_LIST_CREATE_INFO_EXT;
      modifiers.drmFormatModifierCount = 1u;
      modifiers.pDrmFormatModifiers = &linear;
      VkExternalMemoryImageCreateInfo external = {};
      external.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO;
      external.pNext = &modifiers;
      external.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT;
      VkImageCreateInfo image = colour_image();
      image.tiling = VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT;
      image.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
      image.pNext = &external;
      const std::uint32_t driver = driver_image_types(made.device(), image);
      if (driver == 0u) continue;
      const std::uint32_t every = over(made, guest, driver, false);
      CHECK((every & ~over(made, guest, 0xFFFFFFFFu, true)) != 0u);
      check_every_spelling(wire_image_answers(wire, made.device(), image), every);
  }
}

TEST(vulkan_backend_device, a_create_info_query_with_its_create_info_left_out_is_refused)
{ av::LoaderHostApis &host = opened_host();
  for (std::uint32_t i = 0u; i < host.gpu_count(); i++) {
      GuestDevice made(host, i);
      if (made.device() == 0u) continue;
      BackendTransport wire(made.backend());
      const VkDevice device = as_handle<VkDevice>(made.device());
      VkDeviceImageMemoryRequirements image = {};
      image.sType = VK_STRUCTURE_TYPE_DEVICE_IMAGE_MEMORY_REQUIREMENTS;
      VkMemoryRequirements2 answer = {};
      answer.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2;
      vkproto::av_vkGetDeviceImageMemoryRequirements(wire, device, &image, &answer);
      CHECK(!wire.ran());
      vkproto::av_vkGetDeviceImageMemoryRequirementsKHR(wire, device, &image, &answer);
      CHECK(!wire.ran());
      std::uint32_t count = 0u;
      vkproto::av_vkGetDeviceImageSparseMemoryRequirements(wire, device, &image, &count, nullptr);
      CHECK(!wire.ran());
      vkproto::av_vkGetDeviceImageSparseMemoryRequirementsKHR(wire, device, &image, &count, nullptr);
      CHECK(!wire.ran());
      VkDeviceBufferMemoryRequirements buffer = {};
      buffer.sType = VK_STRUCTURE_TYPE_DEVICE_BUFFER_MEMORY_REQUIREMENTS;
      vkproto::av_vkGetDeviceBufferMemoryRequirements(wire, device, &buffer, &answer);
      CHECK(!wire.ran());
      vkproto::av_vkGetDeviceBufferMemoryRequirementsKHR(wire, device, &buffer, &answer);
      CHECK(!wire.ran());

      const VkImageCreateInfo create = colour_image();
      VkImageSubresource2 subresource = {};
      subresource.sType = VK_STRUCTURE_TYPE_IMAGE_SUBRESOURCE_2;
      subresource.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      VkSubresourceLayout2 layout = {};
      layout.sType = VK_STRUCTURE_TYPE_SUBRESOURCE_LAYOUT_2;
      VkDeviceImageSubresourceInfo neither = {};
      neither.sType = VK_STRUCTURE_TYPE_DEVICE_IMAGE_SUBRESOURCE_INFO;
      VkDeviceImageSubresourceInfo no_subresource = neither;
      no_subresource.pCreateInfo = &create;
      VkDeviceImageSubresourceInfo no_create = neither;
      no_create.pSubresource = &subresource;
      const VkDeviceImageSubresourceInfo *const refused[] = {&neither, &no_subresource, &no_create};
      for (const VkDeviceImageSubresourceInfo *info : refused) {
          vkproto::av_vkGetDeviceImageSubresourceLayoutKHR(wire, device, info, &layout);
          CHECK(!wire.ran());
      }
      CHECK(!made.backend().lost());
  }
}

TEST(vulkan_backend_device, an_image_s_create_info_queries_answer_what_the_made_image_would)
{ av::LoaderHostApis &host = opened_host();
  for (std::uint32_t i = 0u; i < host.gpu_count(); i++) {
      const char *const extensions[] = {"VK_KHR_maintenance5"};
      GuestDevice made(host, i, 1u, extensions);
      if (made.device() == 0u) continue;
      BackendTransport wire(made.backend());
      const VkDevice device = as_handle<VkDevice>(made.device());
      VkImageCreateInfo create = colour_image();
      create.tiling = VK_IMAGE_TILING_LINEAR;
      create.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
      VkImage image = VK_NULL_HANDLE;
      CHECK_EQ(vkproto::av_vkCreateImage(wire, device, &create, nullptr, &image), VK_SUCCESS);

      VkImageSubresource whole = {};
      whole.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      VkSubresourceLayout made_layout = {};
      vkproto::av_vkGetImageSubresourceLayout(wire, device, image, &whole, &made_layout);
      VkImageSubresource2 subresource = {};
      subresource.sType = VK_STRUCTURE_TYPE_IMAGE_SUBRESOURCE_2;
      subresource.imageSubresource = whole;
      VkDeviceImageSubresourceInfo asked = {};
      asked.sType = VK_STRUCTURE_TYPE_DEVICE_IMAGE_SUBRESOURCE_INFO;
      asked.pCreateInfo = &create;
      asked.pSubresource = &subresource;
      VkSubresourceLayout2 layout = {};
      layout.sType = VK_STRUCTURE_TYPE_SUBRESOURCE_LAYOUT_2;
      vkproto::av_vkGetDeviceImageSubresourceLayoutKHR(wire, device, &asked, &layout);
      CHECK(wire.ran());
      CHECK_EQ(layout.subresourceLayout.size, made_layout.size);
      CHECK_EQ(layout.subresourceLayout.rowPitch, made_layout.rowPitch);
      CHECK_EQ(layout.subresourceLayout.offset, made_layout.offset);

      std::uint32_t made_count = 9u;
      vkproto::av_vkGetImageSparseMemoryRequirements(wire, device, image, &made_count, nullptr);
      VkDeviceImageMemoryRequirements sparse = {};
      sparse.sType = VK_STRUCTURE_TYPE_DEVICE_IMAGE_MEMORY_REQUIREMENTS;
      sparse.pCreateInfo = &create;
      std::uint32_t count = 9u;
      vkproto::av_vkGetDeviceImageSparseMemoryRequirements(wire, device, &sparse, &count, nullptr);
      CHECK(wire.ran());
      CHECK_EQ(count, made_count);
      CHECK_EQ(count, 0u);
      count = 9u;
      vkproto::av_vkGetDeviceImageSparseMemoryRequirementsKHR(wire, device, &sparse, &count, nullptr);
      CHECK_EQ(count, 0u);
      vkproto::av_vkDestroyImage(wire, device, image, nullptr);
  }
}

TEST(vulkan_backend_device, a_required_handle_named_null_is_refused_before_the_driver)
{ av::LoaderHostApis &host = opened_host();
  for (std::uint32_t i = 0u; i < host.gpu_count(); i++) {
      GuestDevice made(host, i);
      if (made.device() == 0u) continue;
      BackendTransport wire(made.backend());
      const VkDevice device = as_handle<VkDevice>(made.device());
      VkMemoryRequirements needs = {};
      vkproto::av_vkGetImageMemoryRequirements(wire, device, VK_NULL_HANDLE, &needs);
      CHECK(!wire.ran());
      VkImageMemoryRequirementsInfo2 info = {};
      info.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2;
      VkMemoryRequirements2 answer = {};
      answer.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2;
      vkproto::av_vkGetImageMemoryRequirements2(wire, device, &info, &answer);
      CHECK(!wire.ran());
      vkproto::av_vkGetBufferMemoryRequirements(wire, device, VK_NULL_HANDLE, &needs);
      CHECK(!wire.ran());
      CHECK(!made.backend().knows_handle(0u, av::HandleKind::external_image));
      vkproto::av_vkDestroyImage(wire, device, VK_NULL_HANDLE, nullptr);
      CHECK(wire.ran());
  }
}

TEST(vulkan_backend_device, an_exported_fd_s_types_are_the_guest_s_and_another_fd_is_refused)
{ av::LoaderHostApis &host = opened_host();
  for (std::uint32_t i = 0u; i < host.gpu_count(); i++) {
      GuestDevice made(host, i);
      if (made.device() == 0u || !made.probe().memory_import()) continue;
      show_integrated(made);
      BackendTransport wire(made.backend());
      const VkDevice device = as_handle<VkDevice>(made.device());
      const VkPhysicalDeviceMemoryProperties guest = wire_memory(wire, made.physical());
      VkExportMemoryAllocateInfo exportable = {};
      exportable.sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO;
      exportable.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT;
      VkMemoryAllocateInfo allocate = {};
      allocate.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
      allocate.pNext = &exportable;
      allocate.allocationSize = page;
      VkDeviceMemory memory = VK_NULL_HANDLE;
      CHECK_EQ(vkproto::av_vkAllocateMemory(wire, device, &allocate, nullptr, &memory), VK_SUCCESS);
      VkMemoryGetFdInfoKHR get = {};
      get.sType = VK_STRUCTURE_TYPE_MEMORY_GET_FD_INFO_KHR;
      get.memory = memory;
      get.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT;
      int fd = -1;
      if (vkproto::av_vkGetMemoryFdKHR(wire, device, &get, &fd) != VK_SUCCESS) continue;
      VkMemoryFdPropertiesKHR properties = {};
      properties.sType = VK_STRUCTURE_TYPE_MEMORY_FD_PROPERTIES_KHR;
      CHECK_EQ(vkproto::av_vkGetMemoryFdPropertiesKHR(wire, device, VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT, fd,
                                                      &properties),
               VK_SUCCESS);
      const std::uint32_t expected = over(made, guest, made.importable_dmabuf(fd), false);
      CHECK(expected != 0u);
      CHECK_EQ(properties.memoryTypeBits, expected);

      CHECK_EQ(vkproto::av_vkGetMemoryFdPropertiesKHR(wire, device, VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT, fd,
                                                      &properties),
               VK_ERROR_INVALID_EXTERNAL_HANDLE);
      CHECK_EQ(properties.memoryTypeBits, 0u);

      const int stranger = ::memfd_create("not exported", MFD_CLOEXEC);
      CHECK(stranger >= 0);
      properties.memoryTypeBits = 0x5A5Au;
      CHECK_EQ(vkproto::av_vkGetMemoryFdPropertiesKHR(wire, device, VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT,
                                                      stranger, &properties),
               VK_ERROR_INVALID_EXTERNAL_HANDLE);
      ::close(stranger);
  }
}
