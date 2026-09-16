/* One guest context's Vulkan on a real device: the part of the backend only a
 * VkDevice can reach - two devices in one context, a non-dispatchable handle
 * through a dispatch, an import onto a device that can take one, and a teardown
 * that destroys what the guest never did.
 *
 * Its own binary because a real VkDevice allocates from a driver module that
 * carries no symbols at all - two frames, no module name - so no suppression
 * can name it, and leak detection is off here for that reason alone. It stays
 * on in test_vulkan_backend.cpp, which makes instances but no devices, and the
 * backend's own teardown is judged here by what it still recognises afterwards
 * rather than by a leak report. A host with no GPU makes no device and these
 * tests return without judging; the imports are judged on every GPU the host's
 * own loaders list, because the two measured disagree about which memory types
 * a host pointer may become. */
#include "core/support/status.hpp"
#include "device/abi/rvk_executor.h"
#include "device/executor/handle_registry.hpp"
#include "device/executor/host_gpu.hpp"
#include "device/executor/imported_blob.hpp"
#include "device/executor/import_table.hpp"
#include "device/shim/loader_host_apis.hpp"
#include "device/executor/vulkan_backend.hpp"
#include "harness/executor/vulkan_backend_probe.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/client_blob_arena.hpp"
#include "proto/client_transport.hpp"
#include "proto/generated/vulkan/vk_dispatch_client.h"
#include "proto/generated/vulkan/vk_protocol_ids.h"
#include "proto/generated/vulkan/vk_serialize.h"
#include "proto/wire/wire_arena.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

#include <cstdint>
#include <cstring>
#include <sys/mman.h>

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

constexpr std::size_t block_bytes = 65536u;
constexpr std::size_t page = ImportedBlob::page_bytes;

template <typename H>
H as_handle(std::uint64_t raw)
{ H handle = VK_NULL_HANDLE;
  std::memcpy(&handle, &raw, sizeof handle);
  return handle; }

/* A guest's own sequence, sent through run() as frames rather than called
 * directly, because run() is what sets the backend deciding - and every handle
 * in these replies is one the backend remembers only because it went out
 * through the wire. */
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

    /* A device on queue family 0 with the extensions named - none, by
     * default, which is what a guest that knows nothing of host pointers
     * sends. */
    std::uint64_t create_device(std::uint64_t physical, std::uint32_t count = 0u,
                                const char *const *extensions = nullptr)
    {
        std::uint8_t frame[1024];
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

/* The generated client's own encoding, delivered straight into run(): what a
 * guest's ICD sends, without a guest. Whether the backend ran the last frame
 * is kept, because a command with no answer says nothing else. */
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

/* Page-aligned memory this process owns, as the device's contiguous view of a
 * guest's pages is. Declared before any backend that imports it, so it is
 * unmapped only after the driver has let it go. */
class Pages {
public:
    explicit Pages(std::size_t count)
        : count_(count),
          base_(static_cast<std::uint8_t *>(
              mmap(nullptr, count * page, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0)))
    {}
    ~Pages() { (void)munmap(base_, count_ * page); }
    Pages(const Pages &) = delete;
    Pages &operator=(const Pages &) = delete;

    std::uint8_t *at(std::size_t index) const { return base_ + index * page; }

private:
    std::size_t   count_;
    std::uint8_t *base_;
};

/* A backend bound, as the root binds one, to one GPU this host's own loaders
 * list, with the instance, physical device and device a guest makes first. */
class GuestDevice {
public:
    GuestDevice(av::LoaderHostApis &host, std::uint32_t index, std::uint32_t count = 0u,
                const char *const *extensions = nullptr)
        : gpu_(), backend_(), probe_(backend_), guest_(backend_), instance_(0u), physical_(0u), device_(0u)
    { gpu_.describe(host, index);
      backend_.initialize(gpu_);
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

    /* The types the driver says `pointer` may become on this device - asked
     * of the driver directly, so the backend's own answer is judged against
     * it rather than against itself. */
    std::uint32_t importable(void *pointer) const
    { const VkDevice device = as_handle<VkDevice>(device_);
      auto *const properties = reinterpret_cast<PFN_vkGetMemoryHostPointerPropertiesEXT>(
          ::vkGetDeviceProcAddr(device, "vkGetMemoryHostPointerPropertiesEXT"));
      if (properties == nullptr) return 0u;
      VkMemoryHostPointerPropertiesEXT host = {};
      host.sType = VK_STRUCTURE_TYPE_MEMORY_HOST_POINTER_PROPERTIES_EXT;
      if (properties(device, VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT, pointer, &host) !=
          VK_SUCCESS)
          return 0u;
      return host.memoryTypeBits; }

    std::uint32_t memory_types() const
    { VkPhysicalDeviceMemoryProperties memory = {};
      ::vkGetPhysicalDeviceMemoryProperties(as_handle<VkPhysicalDevice>(physical_), &memory);
      return memory.memoryTypeCount; }

private:
    av::HostGpu        gpu_;
    VulkanBackend      backend_;
    VulkanBackendProbe probe_;
    GuestSequence      guest_;
    std::uint64_t      instance_;
    std::uint64_t      physical_;
    std::uint64_t      device_;
};

/* The host's loaders, opened once for the binary: opening them lists every
 * OpenCL and EGL device on both GPUs, seconds a test would spend each time. */
av::LoaderHostApis &opened_host()
{ static av::LoaderHostApis host;
  host.open();
  return host; }

RvkBlobHandle host_pointer(void *pointer, std::uint32_t memory_type, std::uint64_t size = page)
{ RvkBlobHandle handle = {};
  handle.kind = RVK_BLOB_HOST_POINTER;
  handle.reserved = memory_type;
  handle.pointer = pointer;
  handle.fd = -1;
  handle.size = size;
  return handle; }

std::uint32_t lowest(std::uint32_t mask)
{ return mask != 0u ? static_cast<std::uint32_t>(__builtin_ctz(mask)) : 0u; }

/* vkFreeMemory of `memory` through the wire; whether the backend ran it. */
bool free_memory(BackendTransport &wire, std::uint64_t device, std::uint64_t memory)
{ vkproto::rvk_vkFreeMemory(wire, as_handle<VkDevice>(device), as_handle<VkDeviceMemory>(memory), nullptr);
  return wire.ran(); }

}  // namespace

/* The whole of what a guest's context does with a device, on this host's own
 * Vulkan: make an instance and a device, import a page the host owns, give it
 * back, and destroy both. */
TEST(vulkan_backend_device, a_guest_s_device_imports_a_host_page_and_tears_down)
{ av::LoaderHostApis &host = opened_host();
  Pages pages(1u);
  for (std::uint32_t i = 0u; i < host.gpu_count(); i++) {
      GuestDevice made(host, i);
      VulkanBackend &backend = made.backend();
      if (made.device() == 0u) continue;
      CHECK(backend.knows_handle(made.device()));

      ImportedBlob blob;
      CHECK_STATUS(blob.take(1u, host_pointer(pages.at(0u), lowest(made.importable(pages.at(0u))))),
                   av::Status::ok);
      /* A page this process owns, imported as device memory - the zero-copy
       * claim the whole memory channel exists to make. */
      CHECK(backend.adopt(blob));
      CHECK(blob.object() != 0u);
      backend.abandon(blob);
      CHECK_EQ(blob.object(), 0u);
      /* Abandoning what holds nothing is nothing. */
      backend.abandon(blob);
      CHECK_EQ(made.probe().imports_held(), 0u);

      /* Destroying a device this context did not make leaves its own in place. */
      CHECK(!made.guest().destroy(vkproto::kCmd_vkDestroyDevice, made.device() ^ 0x1000u));
      CHECK(made.guest().destroy(vkproto::kCmd_vkDestroyDevice, made.device()));
      CHECK(!backend.knows_handle(made.device()));
      /* And with no device an import has nowhere to go. */
      CHECK(!backend.adopt(blob));
  }
}

/* The type an import becomes is the one the guest asked for, on each GPU: every
 * type the driver says this pointer may become is imported and made a handle
 * the guest may name, every type it may not - the NVIDIA BAR type among them -
 * is refused, and so is a type past the device's own and past any device's.
 * A device made without naming the extensions imports all the same, because
 * the backend enabled them. */
TEST(vulkan_backend_device, an_import_becomes_the_type_asked_for_and_only_a_type_the_pointer_may_become)
{ av::LoaderHostApis &host = opened_host();
  Pages pages(1u);
  for (std::uint32_t i = 0u; i < host.gpu_count(); i++) {
      GuestDevice made(host, i);
      VulkanBackend &backend = made.backend();
      if (made.device() == 0u) continue;
      const std::uint32_t allowed = made.importable(pages.at(0u));
      const std::uint32_t types = made.memory_types();
      /* A GPU whose driver takes no host pointer at all would make everything
       * below pass by refusing; neither GPU here is one. */
      CHECK(allowed != 0u);

      for (std::uint32_t type = 0u; type < types; type++) {
          const bool may = ((allowed >> type) & 1u) != 0u;
          ImportedBlob blob;
          CHECK_STATUS(blob.take(1u, host_pointer(pages.at(0u), type)), av::Status::ok);
          CHECK_EQ(backend.adopt(blob), may);
          CHECK(blob.invariants_hold());
          if (!may) {
              CHECK_EQ(blob.object(), 0u);
              CHECK_EQ(blob.wire_handle(), 0u);
              continue;
          }
          CHECK(blob.object() != 0u);
          CHECK_EQ(blob.wire_handle(), blob.object());
          CHECK(backend.knows_handle(blob.wire_handle()));
          CHECK_EQ(made.probe().imports_held(), 1u);
          const std::uint64_t memory = blob.wire_handle();
          backend.abandon(blob);
          CHECK_EQ(blob.wire_handle(), 0u);
          CHECK(!backend.knows_handle(memory));
          CHECK_EQ(made.probe().imports_held(), 0u);
      }

      constexpr std::uint32_t never = 0xFFFFFFFFu;
      const std::uint32_t past[] = {types, VK_MAX_MEMORY_TYPES - 1u, VK_MAX_MEMORY_TYPES, never};
      for (const std::uint32_t type : past) {
          if (type < types) continue;
          ImportedBlob blob;
          CHECK_STATUS(blob.take(1u, host_pointer(pages.at(0u), type)), av::Status::ok);
          CHECK(!backend.adopt(blob));
          CHECK_EQ(blob.object(), 0u);
      }
      CHECK_EQ(made.probe().imports_held(), 0u);
  }
}

/* A pointer the driver would import, into a type it allows, with a size that
 * runs past the end of the process's address space: the driver refuses it -
 * both measured, where a range merely running into an inaccessible page was
 * pinned lazily by one of them - and the blob holds nothing, with no handle
 * named and no import to release. */
TEST(vulkan_backend_device, an_import_the_driver_refuses_holds_nothing)
{ av::LoaderHostApis &host = opened_host();
  constexpr std::uint64_t past_user_space = std::uint64_t{1u} << 47u;
  for (std::uint32_t i = 0u; i < host.gpu_count(); i++) {
      Pages pages(1u);
      GuestDevice made(host, i);
      if (made.device() == 0u) continue;
      const std::uint32_t type = lowest(made.importable(pages.at(0u)));
      const std::uint32_t known = made.probe().handles().count();
      ImportedBlob blob;
      CHECK_STATUS(blob.take(1u, host_pointer(pages.at(0u), type, past_user_space)), av::Status::ok);
      CHECK(!made.backend().adopt(blob));
      CHECK_EQ(blob.object(), 0u);
      CHECK_EQ(blob.wire_handle(), 0u);
      CHECK_EQ(made.probe().imports_held(), 0u);
      CHECK_EQ(made.probe().handles().count(), known);
  }
}

/* A guest that names the extensions itself gets them once; one naming an
 * extension no driver has is refused - with the host's added or without - and
 * no device is kept. A list counted and not sent is left for the loader to
 * judge, and the loader here makes that device - without the extensions, so
 * nothing imports into it and the guest is copied through instead. */
TEST(vulkan_backend_device, a_device_naming_the_extensions_imports_and_a_refused_one_is_not_kept)
{ av::LoaderHostApis &host = opened_host();
  Pages pages(1u);
  constexpr const char *named[] = {"VK_KHR_external_memory", "VK_EXT_external_memory_host"};
  constexpr const char *bogus[] = {"VK_accel_virt_no_such_extension"};
  constexpr const char *bogus_and_named[] = {"VK_accel_virt_no_such_extension", "VK_KHR_external_memory",
                                             "VK_EXT_external_memory_host"};
  for (std::uint32_t i = 0u; i < host.gpu_count(); i++) {
      GuestDevice made(host, i, 2u, named);
      if (made.device() == 0u) continue;
      VulkanBackend &backend = made.backend();
      ImportedBlob blob;
      CHECK_STATUS(blob.take(1u, host_pointer(pages.at(0u), lowest(made.importable(pages.at(0u))))),
                   av::Status::ok);
      CHECK(backend.adopt(blob));
      backend.abandon(blob);

      CHECK_EQ(made.guest().create_device(made.physical(), 1u, bogus), 0u);
      CHECK_EQ(made.guest().create_device(made.physical(), 3u, bogus_and_named), 0u);
      /* The device the refusals did not replace is still the one imports go
       * to. */
      ImportedBlob again;
      CHECK_STATUS(again.take(2u, host_pointer(pages.at(0u), lowest(made.importable(pages.at(0u))))),
                   av::Status::ok);
      CHECK(backend.adopt(again));
      backend.abandon(again);

      const std::uint64_t unsent = made.guest().create_device(made.physical(), 3u, nullptr);
      CHECK(unsent != 0u);
      ImportedBlob none;
      CHECK_STATUS(none.take(3u, host_pointer(pages.at(0u), lowest(made.importable(pages.at(0u))))),
                   av::Status::ok);
      CHECK(!backend.adopt(none));
      CHECK_EQ(none.object(), 0u);
  }
}

/* The proof that an import is live GPU memory rather than a handle that merely
 * exists: on each GPU, two imported pages bound to two buffers through the
 * wire, the first filled by this process, and the GPU told - by a command
 * buffer the guest records and submits, all as frames - to copy it into the
 * second. The second page then holds the pattern, written by the GPU. Freeing
 * each memory through the wire - on_vkFreeMemory - releases its import once:
 * the driver's count drops, a second free is refused as a handle nobody has,
 * the pages stay this process's, and the device's own release of the blob
 * afterwards frees nothing. */
TEST(vulkan_backend_device, the_gpu_copies_between_two_imported_pages_bound_through_the_wire)
{ av::LoaderHostApis &host = opened_host();
  for (std::uint32_t i = 0u; i < host.gpu_count(); i++) {
      Pages pages(2u);
      GuestDevice made(host, i);
      if (made.device() == 0u) continue;
      VulkanBackend &backend = made.backend();
      BackendTransport wire(backend);
      const VkDevice device = as_handle<VkDevice>(made.device());
      for (std::size_t b = 0u; b < page; b++) pages.at(0u)[b] = static_cast<std::uint8_t>(b * 7u + 1u);

      VkExternalMemoryBufferCreateInfo external = {};
      external.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO;
      external.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT;
      VkBufferCreateInfo create = {};
      create.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
      create.pNext = &external;
      create.size = page;
      create.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
      VkBuffer source = VK_NULL_HANDLE;
      CHECK_EQ(vkproto::rvk_vkCreateBuffer(wire, device, &create, nullptr, &source), VK_SUCCESS);
      create.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
      VkBuffer target = VK_NULL_HANDLE;
      CHECK_EQ(vkproto::rvk_vkCreateBuffer(wire, device, &create, nullptr, &target), VK_SUCCESS);
      VkMemoryRequirements requirements = {};
      vkproto::rvk_vkGetBufferMemoryRequirements(wire, device, source, &requirements);
      CHECK(requirements.size <= page);
      const std::uint32_t type = lowest(made.importable(pages.at(0u)) & requirements.memoryTypeBits);

      ImportedBlob blobs[2];
      for (std::uint32_t b = 0u; b < 2u; b++) {
          CHECK_STATUS(blobs[b].take(b + 1u, host_pointer(pages.at(b), type)), av::Status::ok);
          CHECK(backend.adopt(blobs[b]));
      }
      CHECK_EQ(made.probe().imports_held(), 2u);
      CHECK_EQ(vkproto::rvk_vkBindBufferMemory(wire, device, source,
                                               as_handle<VkDeviceMemory>(blobs[0].wire_handle()), 0u),
               VK_SUCCESS);
      CHECK_EQ(vkproto::rvk_vkBindBufferMemory(wire, device, target,
                                               as_handle<VkDeviceMemory>(blobs[1].wire_handle()), 0u),
               VK_SUCCESS);

      std::uint64_t queue = 0u;
      vkproto::rvk_vkGetDeviceQueue(wire, device, 0u, 0u, &queue);
      CHECK(queue != 0u);
      VkCommandPoolCreateInfo pool_create = {};
      pool_create.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
      VkCommandPool pool = VK_NULL_HANDLE;
      CHECK_EQ(vkproto::rvk_vkCreateCommandPool(wire, device, &pool_create, nullptr, &pool), VK_SUCCESS);
      VkCommandBufferAllocateInfo allocate = {};
      allocate.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
      allocate.commandPool = pool;
      allocate.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
      allocate.commandBufferCount = 1u;
      std::uint64_t commands_raw = 0u;
      CHECK_EQ(vkproto::rvk_vkAllocateCommandBuffers(wire, device, &allocate, &commands_raw), VK_SUCCESS);
      const VkCommandBuffer commands = as_handle<VkCommandBuffer>(commands_raw);
      VkCommandBufferBeginInfo begin = {};
      begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
      CHECK_EQ(vkproto::rvk_vkBeginCommandBuffer(wire, commands, &begin), VK_SUCCESS);
      const VkBufferCopy region = {0u, 0u, page};
      vkproto::rvk_vkCmdCopyBuffer(wire, commands, source, target, 1u, &region);
      CHECK(wire.ran());
      CHECK_EQ(vkproto::rvk_vkEndCommandBuffer(wire, commands), VK_SUCCESS);
      VkSubmitInfo submit = {};
      submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
      submit.commandBufferCount = 1u;
      submit.pCommandBuffers = &commands;
      CHECK_EQ(vkproto::rvk_vkQueueSubmit(wire, as_handle<VkQueue>(queue), 1u, &submit, VK_NULL_HANDLE),
               VK_SUCCESS);
      CHECK_EQ(vkproto::rvk_vkQueueWaitIdle(wire, as_handle<VkQueue>(queue)), VK_SUCCESS);

      CHECK(std::memcmp(pages.at(1u), pages.at(0u), page) == 0);
      CHECK_EQ(pages.at(1u)[page - 1u], static_cast<std::uint8_t>((page - 1u) * 7u + 1u));

      vkproto::rvk_vkDestroyCommandPool(wire, device, pool, nullptr);
      vkproto::rvk_vkDestroyBuffer(wire, device, source, nullptr);
      vkproto::rvk_vkDestroyBuffer(wire, device, target, nullptr);

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
      }
      /* Still this process's memory, and still what the GPU wrote. */
      pages.at(1u)[0] ^= 0xFFu;
      CHECK_EQ(pages.at(1u)[0], static_cast<std::uint8_t>(0xFEu));
  }
}

/* Memory the guest allocated itself is not an import, and its free goes to the
 * driver as any other - after which it is a handle nobody has. */
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
      CHECK_EQ(vkproto::rvk_vkAllocateMemory(wire, as_handle<VkDevice>(made.device()), &allocate, nullptr, &memory),
               VK_SUCCESS);
      std::uint64_t raw = 0u;
      std::memcpy(&raw, &memory, sizeof memory);
      CHECK(made.backend().knows_handle(raw));
      CHECK(free_memory(wire, made.device(), raw));
      CHECK(!made.backend().knows_handle(raw));
      CHECK(!free_memory(wire, made.device(), raw));
  }
}

/* An import the backend cannot make a handle of is refused and given back
 * whole, rather than answered with a handle the guest would be refused - both
 * when the handle registry is full and when the backend already holds a
 * session's worth of imports - and the device is not lost for it. Room made
 * again is an import accepted again. */
TEST(vulkan_backend_device, an_import_with_no_room_to_be_named_is_refused_and_given_back)
{ av::LoaderHostApis &host = opened_host();
  if (host.gpu_count() == 0u) return;
  Pages pages(av::ImportTable::max_imports + 1u);
  {
      GuestDevice made(host, 0u);
      VulkanBackend &backend = made.backend();
      const std::uint32_t type = lowest(made.importable(pages.at(0u)));
      for (std::uint64_t raw = 1u; made.probe().handles().count() < av::HandleRegistry::max_handles; raw++)
          backend.made(raw);
      CHECK(!backend.lost());
      ImportedBlob blob;
      CHECK_STATUS(blob.take(1u, host_pointer(pages.at(0u), type)), av::Status::ok);
      CHECK(!backend.adopt(blob));
      CHECK_EQ(blob.object(), 0u);
      CHECK_EQ(made.probe().imports_held(), 0u);
      CHECK(!backend.lost());
  }
  {
      GuestDevice made(host, 0u);
      VulkanBackend &backend = made.backend();
      const std::uint32_t type = lowest(made.importable(pages.at(0u)));
      static ImportedBlob blobs[av::ImportTable::max_imports + 1u];
      for (std::uint32_t b = 0u; b < av::ImportTable::max_imports; b++) {
          CHECK_STATUS(blobs[b].take(b + 1u, host_pointer(pages.at(b), type)), av::Status::ok);
          CHECK(backend.adopt(blobs[b]));
      }
      ImportedBlob &extra = blobs[av::ImportTable::max_imports];
      CHECK_STATUS(extra.take(0xFFFFu, host_pointer(pages.at(av::ImportTable::max_imports), type)),
                   av::Status::ok);
      CHECK(!backend.adopt(extra));
      CHECK_EQ(extra.object(), 0u);
      CHECK_EQ(made.probe().imports_held(), av::ImportTable::max_imports);

      backend.abandon(blobs[0]);
      CHECK(backend.adopt(extra));
      for (ImportedBlob &blob : blobs) {
          backend.abandon(blob);
          blob.give_back();
      }
      CHECK_EQ(made.probe().imports_held(), 0u);
  }
}

/* A context that dies owning a device as well as an instance gives both back,
 * device first, in the order the API requires. */
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

/* Two instances and two devices in one context: destroying the first of each
 * leaves the second as the one kept. A non-dispatchable handle goes through the
 * same dispatch - one the backend never made is refused, the null one is the
 * API's own and is not. */
TEST(vulkan_backend_device, a_second_instance_and_device_are_the_ones_kept)
{ av::LoaderHostApis &host = opened_host();
  if (host.gpu_count() == 0u) return;
  av::HostGpu gpu;
  gpu.describe(host, 0u);
  VulkanBackend backend;
  backend.initialize(gpu);
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

/* A blob holding memory whose device has since gone is not freed through a
 * device that no longer exists, and the memory went with the device as a
 * handle the guest can name. */
TEST(vulkan_backend_device, memory_of_a_device_that_is_gone_is_not_freed_through_it)
{ av::LoaderHostApis &host = opened_host();
  Pages pages(1u);
  for (std::uint32_t i = 0u; i < host.gpu_count(); i++) {
      GuestDevice made(host, i);
      if (made.device() == 0u) continue;
      VulkanBackend &backend = made.backend();
      ImportedBlob blob;
      CHECK_STATUS(blob.take(1u, host_pointer(pages.at(0u), lowest(made.importable(pages.at(0u))))),
                   av::Status::ok);
      CHECK(backend.adopt(blob));
      const std::uint64_t memory = blob.object();
      CHECK(made.guest().destroy(vkproto::kCmd_vkDestroyDevice, made.device()));
      CHECK(!backend.knows_handle(memory));
      backend.abandon(blob);
      CHECK_EQ(blob.object(), memory);
      CHECK_EQ(made.probe().imports_held(), 1u);
  }
}

/* A non-dispatchable handle this context really made comes back through the
 * dispatch and is accepted, which is the half of the check that says yes. */
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
      /* The driver freed it, so naming it again is refused before the driver
       * is handed a freed pointer (T031). */
      CHECK(!backend.knows_handle(buffer));
      CHECK(!guest.send(vkproto::kCmd_vkDestroyBuffer, frame, destroy.offset(), reply,
                        sizeof reply, answered));
  }
}

/* A driver destroying a device frees the buffers made on it, so after the
 * guest destroys the device a buffer it never destroyed is refused too. */
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
      CHECK_EQ(vkproto::rvk_vkCreateBuffer(wire, as_handle<VkDevice>(made.device()), &create, nullptr, &buffer),
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
  external.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT;
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

/* The memory types the driver itself answers for `create` on `device`, made
 * directly rather than through the backend: the reference a guest's buffer is
 * judged against. */
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

/* Whether the driver says an image like `create` may live in host memory it
 * imports: Intel's does not for an optimal colour image, NVIDIA's does. */
bool driver_imports_image(std::uint64_t physical, const VkImageCreateInfo &create)
{ VkPhysicalDeviceExternalImageFormatInfo external = {};
  external.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_IMAGE_FORMAT_INFO;
  external.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT;
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
  if (vkproto::rvk_vkCreateBuffer(wire, host, &create, nullptr, &buffer) != VK_SUCCESS) return 0u;
  VkMemoryRequirements requirements = {};
  vkproto::rvk_vkGetBufferMemoryRequirements(wire, host, buffer, &requirements);
  vkproto::rvk_vkDestroyBuffer(wire, host, buffer, nullptr);
  return requirements.memoryTypeBits; }

std::uint32_t wire_image_types(BackendTransport &wire, std::uint64_t device, const VkImageCreateInfo &create)
{ const VkDevice host = as_handle<VkDevice>(device);
  VkImage image = VK_NULL_HANDLE;
  if (vkproto::rvk_vkCreateImage(wire, host, &create, nullptr, &image) != VK_SUCCESS) return 0u;
  VkMemoryRequirements requirements = {};
  vkproto::rvk_vkGetImageMemoryRequirements(wire, host, image, &requirements);
  vkproto::rvk_vkDestroyImage(wire, host, image, nullptr);
  return requirements.memoryTypeBits; }

}  // namespace

/* A guest's ICD cannot know, when an application makes a buffer or an image,
 * whether its memory will be a host-visible chunk the host imports, so the
 * backend makes every one it can for host allocation (feature 045, T035). What
 * the guest is answered is then the requirements of such a buffer: on NVIDIA its
 * BAR type drops out of memoryTypeBits (0x3b becomes 0x1b), which is the type
 * the ICD no longer calls host-visible. An image the driver will not import -
 * Intel's optimal colour image - is made as the guest asked. A buffer that
 * already names its external handle types is the guest's to decide. */
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
      image_external.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT;
      const VkImageCreateInfo image = colour_image();
      VkImageCreateInfo image_for_host = colour_image();
      image_for_host.pNext = &image_external;
      const std::uint32_t expected = driver_imports_image(made.physical(), image)
                                         ? driver_image_types(made.device(), image_for_host)
                                         : driver_image_types(made.device(), image);
      CHECK(expected != 0u);
      CHECK_EQ(wire_image_types(wire, made.device(), image), expected);
  }
}

/* A device the driver made without host import - the guest's list could not be
 * widened - and a device the context has since replaced make buffers exactly as
 * asked: nothing will ever be imported into either. */
TEST(vulkan_backend_device, a_device_that_imports_nothing_makes_buffers_as_asked)
{ av::LoaderHostApis &host = opened_host();
  for (std::uint32_t i = 0u; i < host.gpu_count(); i++) {
      GuestDevice made(host, i);
      if (made.device() == 0u) continue;
      BackendTransport wire(made.backend());
      const VkBufferCreateInfo asked = storage_buffer();

      const std::uint64_t replaced = made.device();
      const std::uint64_t unsent = made.guest().create_device(made.physical(), 3u, nullptr);
      CHECK(unsent != 0u);
      CHECK_EQ(wire_buffer_types(wire, unsent, asked), driver_buffer_types(unsent, asked));
      CHECK_EQ(wire_buffer_types(wire, replaced, asked), driver_buffer_types(replaced, asked));
  }
}

/* Imports go to the device the context made last, so a device it made earlier
 * is never backed by host memory even while its replacement imports: its
 * buffers are made as asked. */
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

/* A chain naming other structures is still the guest's chain: the host
 * allocation is added in front of it and what the guest named still applies. */
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

namespace {

VkPhysicalDeviceMemoryProperties driver_memory(std::uint64_t physical)
{ VkPhysicalDeviceMemoryProperties memory = {};
  ::vkGetPhysicalDeviceMemoryProperties(as_handle<VkPhysicalDevice>(physical), &memory);
  return memory; }

/* What the guest's view must be: the driver's own properties, with a type host
 * visible only where the driver would import `page` into it. */
void check_restricted(const VkPhysicalDeviceMemoryProperties &guest, const VkPhysicalDeviceMemoryProperties &driver,
                      std::uint32_t importable)
{ constexpr VkMemoryPropertyFlags host_only = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                              VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
                                              VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
  CHECK_EQ(guest.memoryTypeCount, driver.memoryTypeCount);
  CHECK_EQ(guest.memoryHeapCount, driver.memoryHeapCount);
  for (std::uint32_t type = 0u; type < driver.memoryTypeCount; type++) {
      const VkMemoryPropertyFlags flags = driver.memoryTypes[type].propertyFlags;
      const bool                  kept = (importable & (1u << type)) != 0u;
      CHECK_EQ(guest.memoryTypes[type].propertyFlags, kept ? flags : flags & ~host_only);
      CHECK_EQ(guest.memoryTypes[type].heapIndex, driver.memoryTypes[type].heapIndex);
  }
  for (std::uint32_t heap = 0u; heap < driver.memoryHeapCount; heap++)
      CHECK_EQ(guest.memoryHeaps[heap].size, driver.memoryHeaps[heap].size); }

}  // namespace

/* The guest's only mappable memory is its own RAM, imported by the host GPU, so
 * the guest sees a type host visible only where the driver imports a host
 * pointer: every type on Intel, and on NVIDIA not the BAR type. The answer is
 * the same before the guest has a device - an application chooses its types
 * before it makes one - and through either spelling of the query. */
TEST(vulkan_backend_device, memory_is_host_visible_to_the_guest_only_where_a_host_pointer_imports)
{ av::LoaderHostApis &host = opened_host();
  Pages pages(1u);
  for (std::uint32_t i = 0u; i < host.gpu_count(); i++) {
      GuestDevice made(host, i);
      if (made.device() == 0u) continue;
      const std::uint32_t importable = made.importable(pages.at(0u));
      CHECK(importable != 0u);

      av::HostGpu gpu;
      gpu.describe(host, i);
      VulkanBackend fresh;
      fresh.initialize(gpu);
      GuestSequence  guest(fresh);
      BackendTransport wire(fresh);
      const std::uint64_t instance = guest.create_instance();
      const std::uint64_t physical = guest.first_physical_device(instance);
      CHECK(physical != 0u);
      const VkPhysicalDeviceMemoryProperties driver = driver_memory(physical);
      VkPhysicalDeviceMemoryProperties before = {};
      vkproto::rvk_vkGetPhysicalDeviceMemoryProperties(wire, as_handle<VkPhysicalDevice>(physical), &before);
      check_restricted(before, driver, importable);

      VkPhysicalDeviceMemoryProperties2 after = {};
      after.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2;
      CHECK(guest.create_device(physical) != 0u);
      vkproto::rvk_vkGetPhysicalDeviceMemoryProperties2(wire, as_handle<VkPhysicalDevice>(physical), &after);
      check_restricted(after.memoryProperties, driver, importable);
      VkPhysicalDeviceMemoryProperties2 khr = {};
      khr.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2;
      vkproto::rvk_vkGetPhysicalDeviceMemoryProperties2KHR(wire, as_handle<VkPhysicalDevice>(physical), &khr);
      check_restricted(khr.memoryProperties, driver, importable);
      fresh.shut_down();
  }
}
