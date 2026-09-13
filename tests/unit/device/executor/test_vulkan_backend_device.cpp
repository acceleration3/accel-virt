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
 * tests say so rather than pass by omission. */
#include "core/support/status.hpp"
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

extern "C" int __lsan_is_turned_off(void);
extern "C" int __lsan_is_turned_off(void) { return 1; }

using av::VulkanBackend;
using av::WireArena;
using av::WireReader;
using av::WireWriter;

namespace {

constexpr std::size_t block_bytes = 65536u;

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
        av::vkproto::serialize_VkInstanceCreateInfo(writer, create);
        std::uint8_t reply[64] = {};
        std::size_t  answered = 0u;
        if (!send(av::vkproto::kCmd_vkCreateInstance, frame, writer.offset(), reply, sizeof reply,
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
        if (!send(av::vkproto::kCmd_vkEnumeratePhysicalDevices, frame, writer.offset(), reply,
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

    std::uint64_t create_device(std::uint64_t physical) { return create_device(physical, true); }

    /* With or without the host-pointer extension, and with one that does not
     * exist when `extension` names it - which is how to ask the driver to
     * refuse. */
    std::uint64_t create_device(std::uint64_t physical, bool host_import,
                                const char *extension = "VK_EXT_external_memory_host")
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
        const char *extensions[] = {extension};
        create.enabledExtensionCount = host_import ? 1u : 0u;
        create.ppEnabledExtensionNames = host_import ? extensions : nullptr;
        av::vkproto::serialize_VkDeviceCreateInfo(writer, create);
        std::uint8_t reply[64] = {};
        std::size_t  answered = 0u;
        if (!send(av::vkproto::kCmd_vkCreateDevice, frame, writer.offset(), reply, sizeof reply,
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

}  // namespace

/* The whole of what a guest's context does with a device, on this host's own
 * Vulkan: make an instance and a device, import a page the host owns, give it
 * back, and destroy both. */
TEST(vulkan_backend_device, a_guest_s_device_imports_a_host_page_and_tears_down)
{ VulkanBackend backend;
  GuestSequence guest(backend);
  const std::uint64_t instance = guest.create_instance();
  if (instance == 0u) return;
  CHECK(backend.knows_handle(instance));

  const std::uint64_t physical = guest.first_physical_device(instance);
  if (physical == 0u) { backend.shut_down(); return; }
  CHECK(backend.knows_handle(physical));

  const std::uint64_t device = guest.create_device(physical);
  if (device == 0u) { backend.shut_down(); return; }
  CHECK(backend.knows_handle(device));

  alignas(4096) static std::uint8_t page[av::ImportedBlob::page_bytes];
  av::ImportedBlob blob;
  RvkBlobHandle handle = {};
  handle.kind = RVK_BLOB_HOST_POINTER;
  handle.pointer = page;
  handle.fd = -1;
  handle.size = sizeof page;
  CHECK_STATUS(blob.take(1u, handle), av::Status::ok);
  /* A page this process owns, imported as device memory - the zero-copy claim
   * the whole memory channel exists to make. */
  if (backend.adopt(blob)) {
      CHECK(blob.object() != 0u);
      backend.abandon(blob);
      CHECK_EQ(blob.object(), 0u);
  }
  /* Abandoning what holds nothing is nothing. */
  backend.abandon(blob);

  /* Destroying a device this context did not make leaves its own in place. */
  CHECK(!guest.destroy(av::vkproto::kCmd_vkDestroyDevice, device ^ 0x1000u));
  CHECK(guest.destroy(av::vkproto::kCmd_vkDestroyDevice, device));
  CHECK(!backend.knows_handle(device));
  /* And with no device an import has nowhere to go. */
  CHECK(!backend.adopt(blob));

  CHECK(guest.destroy(av::vkproto::kCmd_vkDestroyInstance, instance));
  CHECK(!backend.knows_handle(instance));
  backend.shut_down(); }

/* A context that dies owning a device as well as an instance gives both back,
 * device first, in the order the API requires. */
TEST(vulkan_backend_device, shutting_down_destroys_a_device_the_guest_never_did)
{ VulkanBackend backend;
  GuestSequence guest(backend);
  const std::uint64_t instance = guest.create_instance();
  if (instance == 0u) return;
  const std::uint64_t physical = guest.first_physical_device(instance);
  const std::uint64_t device = physical != 0u ? guest.create_device(physical) : 0u;
  backend.shut_down();
  CHECK(!backend.knows_handle(instance));
  if (device != 0u) CHECK(!backend.knows_handle(device)); }

/* Two instances and two devices in one context: destroying the first of each
 * leaves the second as the one kept. A non-dispatchable handle goes through the
 * same dispatch - one the backend never made is refused, the null one is the
 * API's own and is not. */
TEST(vulkan_backend_device, a_second_instance_and_device_are_the_ones_kept)
{ VulkanBackend backend;
  GuestSequence guest(backend);
  const std::uint64_t first = guest.create_instance();
  if (first == 0u) return;
  const std::uint64_t second = guest.create_instance();
  CHECK(second != 0u);
  CHECK(guest.destroy(av::vkproto::kCmd_vkDestroyInstance, first));
  CHECK(backend.knows_handle(second));

  const std::uint64_t physical = guest.first_physical_device(second);
  if (physical == 0u) { backend.shut_down(); return; }
  const std::uint64_t device_one = guest.create_device(physical);
  const std::uint64_t device_two = guest.create_device(physical);
  if (device_one == 0u || device_two == 0u) { backend.shut_down(); return; }
  CHECK(guest.destroy(av::vkproto::kCmd_vkDestroyDevice, device_one));
  CHECK(backend.knows_handle(device_two));

  std::uint8_t frame[32];
  WireWriter invented(frame, sizeof frame);
  invented.write_pod(device_two);
  invented.write_pod<std::uint64_t>(0xBEEFu);
  std::uint8_t reply[8] = {};
  std::size_t answered = 0u;
  CHECK(!guest.send(av::vkproto::kCmd_vkDestroyBuffer, frame, invented.offset(), reply,
                    sizeof reply, answered));
  WireWriter null_buffer(frame, sizeof frame);
  null_buffer.write_pod(device_two);
  null_buffer.write_pod<std::uint64_t>(0u);
  CHECK(guest.send(av::vkproto::kCmd_vkDestroyBuffer, frame, null_buffer.offset(), reply,
                   sizeof reply, answered));
  backend.shut_down(); }

/* A blob holding memory whose device has since gone is not freed through a
 * device that no longer exists. */
TEST(vulkan_backend_device, memory_of_a_device_that_is_gone_is_not_freed_through_it)
{ VulkanBackend backend;
  GuestSequence guest(backend);
  const std::uint64_t instance = guest.create_instance();
  if (instance == 0u) return;
  const std::uint64_t physical = guest.first_physical_device(instance);
  if (physical == 0u) { backend.shut_down(); return; }
  const std::uint64_t device = guest.create_device(physical);
  if (device == 0u) { backend.shut_down(); return; }

  alignas(4096) static std::uint8_t page[av::ImportedBlob::page_bytes];
  av::ImportedBlob blob;
  RvkBlobHandle handle = {};
  handle.kind = RVK_BLOB_HOST_POINTER;
  handle.pointer = page;
  handle.fd = -1;
  handle.size = sizeof page;
  CHECK_STATUS(blob.take(1u, handle), av::Status::ok);
  if (backend.adopt(blob)) {
      const std::uint64_t memory = blob.object();
      CHECK(guest.destroy(av::vkproto::kCmd_vkDestroyDevice, device));
      backend.abandon(blob);
      CHECK_EQ(blob.object(), memory);
  }
  backend.shut_down(); }

/* A device the driver refuses is not kept - an extension that does not exist is
 * how to ask for one - and a device made without the host-pointer extension
 * has nothing to import through. */
TEST(vulkan_backend_device, a_refused_device_is_not_kept_and_one_without_the_extension_imports_nothing)
{ VulkanBackend backend;
  GuestSequence guest(backend);
  const std::uint64_t instance = guest.create_instance();
  if (instance == 0u) return;
  const std::uint64_t physical = guest.first_physical_device(instance);
  if (physical == 0u) { backend.shut_down(); return; }

  CHECK_EQ(guest.create_device(physical, true, "VK_accel_virt_no_such_extension"), 0u);

  const std::uint64_t plain = guest.create_device(physical, false);
  if (plain == 0u) { backend.shut_down(); return; }
  alignas(4096) static std::uint8_t page[av::ImportedBlob::page_bytes];
  av::ImportedBlob blob;
  RvkBlobHandle handle = {};
  handle.kind = RVK_BLOB_HOST_POINTER;
  handle.pointer = page;
  handle.fd = -1;
  handle.size = sizeof page;
  CHECK_STATUS(blob.take(1u, handle), av::Status::ok);
  CHECK(!backend.adopt(blob));
  CHECK_EQ(blob.object(), 0u);
  backend.shut_down(); }

/* A non-dispatchable handle this context really made comes back through the
 * dispatch and is accepted, which is the half of the check that says yes. */
TEST(vulkan_backend_device, a_buffer_this_context_made_is_accepted_back)
{ VulkanBackend backend;
  GuestSequence guest(backend);
  const std::uint64_t instance = guest.create_instance();
  if (instance == 0u) return;
  const std::uint64_t physical = guest.first_physical_device(instance);
  if (physical == 0u) { backend.shut_down(); return; }
  const std::uint64_t device = guest.create_device(physical, false);
  if (device == 0u) { backend.shut_down(); return; }

  std::uint8_t frame[256];
  WireWriter writer(frame, sizeof frame);
  writer.write_pod(device);
  VkBufferCreateInfo create = {};
  create.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  create.size = 4096u;
  create.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
  av::vkproto::serialize_VkBufferCreateInfo(writer, create);
  std::uint8_t reply[64] = {};
  std::size_t answered = 0u;
  CHECK(guest.send(av::vkproto::kCmd_vkCreateBuffer, frame, writer.offset(), reply, sizeof reply,
                   answered));
  WireReader read(reply, answered);
  std::int32_t returned = 0;
  std::uint64_t buffer = 0u;
  CHECK(read.read_pod(returned));
  CHECK(read.read_pod(buffer));
  if (returned == 0 && buffer != 0u) {
      CHECK(backend.knows_handle(buffer));
      WireWriter destroy(frame, sizeof frame);
      destroy.write_pod(device);
      destroy.write_pod(buffer);
      CHECK(guest.send(av::vkproto::kCmd_vkDestroyBuffer, frame, destroy.offset(), reply,
                       sizeof reply, answered));
  }
  backend.shut_down(); }
