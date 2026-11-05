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

constexpr std::size_t   block_bytes = 65536u;
constexpr std::size_t   frame_bytes = 256u;
constexpr std::uint64_t stale = 0xA5A5A5A5A5A5A5A5u;
constexpr std::uint64_t first_invented = 0x10100u;

template <typename H>
H as_handle(std::uint64_t raw)
{ H handle = VK_NULL_HANDLE;
  std::memcpy(&handle, &raw, sizeof handle);
  return handle; }

class CarelessDriver final : public vkproto::DefaultServerDispatch {
public:
    explicit CarelessDriver(VulkanBackend &backend) : backend_(backend) {}

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
    { *out = as_handle<VkInstance>(first_invented); return VK_SUCCESS; }
    VkResult on_vkCreateDevice(VkPhysicalDevice, const VkDeviceCreateInfo *,
                               const VkAllocationCallbacks *, VkDevice *out) override
    { *out = as_handle<VkDevice>(first_invented + 0x200u); return VK_SUCCESS; }
    VkResult on_vkEnumeratePhysicalDevices(VkInstance, uint32_t *count, VkPhysicalDevice *out) override
    { if (out != nullptr) {
          for (uint32_t i = 0u; i < *count; i++) out[i] = as_handle<VkPhysicalDevice>(stale);
          for (uint32_t i = 0u; result_ >= 0 && i < listed_ && i < *count; i++)
              out[i] = as_handle<VkPhysicalDevice>(first_invented + 0x100u + i);
      }
      *count = claimed_;
      return result_; }
    VkResult on_vkCreateBuffer(VkDevice, const VkBufferCreateInfo *, const VkAllocationCallbacks *,
                               VkBuffer *out) override
    { *out = as_handle<VkBuffer>(stale); return VK_ERROR_OUT_OF_DEVICE_MEMORY; }

    void enumerate(VkResult result, std::uint32_t listed, std::uint32_t claimed)
    { result_ = result;
      listed_ = listed;
      claimed_ = claimed; }

private:
    VulkanBackend &backend_;
    VkResult      result_ = VK_SUCCESS;
    std::uint32_t listed_ = 1u;
    std::uint32_t claimed_ = 1u;
};

class GuestContext {
public:
    GuestContext() : sink_(), lines_(sink_, "vulkan: "), backend_(lines_), probe_(backend_), driver_(backend_), block_{}, frame_{}, reply_{}, answered_(0u) {}

    CarelessDriver &driver() { return driver_; }
    vkproto::HandleTranslation &identity() { return identity_; }
    bool            knows(std::uint64_t handle) const { return backend_.knows_handle(handle); }
    WireWriter      frame() { return WireWriter(frame_, sizeof frame_); }
    WireReader      answer() const { return WireReader(reply_, answered_); }

    bool send(std::uint32_t command, const WireWriter &frame)
    { WireArena  arena(block_, sizeof block_);
      WireReader payload(frame_, frame.offset());
      WireWriter answer(reply_, sizeof reply_);
      const bool ran = probe_.run_through(driver_, command, payload, answer, arena);
      answered_ = answer.offset();
      CHECK(probe_.handles().invariants_hold());
      return ran; }

    std::uint64_t created() const
    { WireReader reply = answer();
      std::int32_t  result = 0;
      std::uint64_t handle = stale;
      CHECK(reply.read_pod(result));
      CHECK(reply.read_pod(handle));
      return handle; }

    std::uint64_t instance()
    { WireWriter writer = frame();
      VkInstanceCreateInfo create = {};
      create.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
      vkproto::serialize_VkInstanceCreateInfo(writer, identity_, create);
      CHECK(send(vkproto::kCmd_vkCreateInstance, writer));
      return created(); }

    bool enumerate(std::uint64_t instance, std::uint32_t room)
    { WireWriter writer = frame();
      writer.write_pod(instance);
      writer.write_pod<std::uint8_t>(1u);
      writer.write_pod(room);
      return send(vkproto::kCmd_vkEnumeratePhysicalDevices, writer); }

    std::uint64_t device(std::uint64_t physical)
    { WireWriter writer = frame();
      writer.write_pod(physical);
      VkDeviceCreateInfo create = {};
      create.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
      vkproto::serialize_VkDeviceCreateInfo(writer, identity_, create);
      CHECK(send(vkproto::kCmd_vkCreateDevice, writer));
      return created(); }

private:
    av::RecordingLog   sink_;
    av::DriverLog      lines_;
    VulkanBackend      backend_;
    VulkanBackendProbe probe_;
    CarelessDriver     driver_;
    vkproto::HandleIdentity identity_;
    std::uint8_t       block_[block_bytes];
    std::uint8_t       frame_[frame_bytes];
    std::uint8_t       reply_[frame_bytes];
    std::size_t        answered_;
};

std::uint32_t read_enumeration(const GuestContext &guest, std::int32_t &result, std::uint32_t &count,
                               std::uint64_t *listed, std::uint32_t room)
{ WireReader reply = guest.answer();
  CHECK(reply.read_pod(result));
  CHECK(reply.read_pod(count));
  std::uint32_t followed = 0u;
  while (followed < room && reply.remaining() >= sizeof(std::uint64_t)) {
      CHECK(reply.read_pod(listed[followed]));
      followed++;
  }
  CHECK_EQ(reply.remaining(), 0u);
  return followed; }

}

TEST(vulkan_dispatch, a_failed_create_answers_a_null_handle_the_host_never_learns)
{ GuestContext guest;
  const std::uint64_t instance = guest.instance();
  CHECK(guest.enumerate(instance, 1u));
  std::int32_t  result = 0;
  std::uint32_t count = 0u;
  std::uint64_t physical = 0u;
  CHECK_EQ(read_enumeration(guest, result, count, &physical, 1u), 1u);
  const std::uint64_t device = guest.device(physical);
  CHECK(guest.knows(device));

  WireWriter writer = guest.frame();
  writer.write_pod(device);
  VkBufferCreateInfo create = {};
  create.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  create.size = 4096u;
  vkproto::serialize_VkBufferCreateInfo(writer, guest.identity(), create);
  CHECK(guest.send(vkproto::kCmd_vkCreateBuffer, writer));
  WireReader reply = guest.answer();
  std::int32_t  failed = 0;
  std::uint64_t buffer = stale;
  CHECK(reply.read_pod(failed));
  CHECK(reply.read_pod(buffer));
  CHECK_EQ(failed, VK_ERROR_OUT_OF_DEVICE_MEMORY);
  CHECK_EQ(buffer, 0u);
  CHECK(!guest.knows(stale)); }

TEST(vulkan_dispatch, a_failed_enumeration_answers_no_devices_whatever_the_driver_claims)
{ GuestContext guest;
  const std::uint64_t instance = guest.instance();
  for (std::uint32_t room = 0u; room < 3u; room += 2u) {
      guest.driver().enumerate(VK_ERROR_INITIALIZATION_FAILED, 0u, 7u);
      CHECK(guest.enumerate(instance, room));
      std::int32_t  result = 0;
      std::uint32_t count = ~std::uint32_t{0};
      std::uint64_t listed[4] = {};
      CHECK_EQ(read_enumeration(guest, result, count, listed, 4u), 0u);
      CHECK_EQ(result, VK_ERROR_INITIALIZATION_FAILED);
      CHECK_EQ(count, 0u);
  }
  CHECK(!guest.knows(stale)); }

TEST(vulkan_dispatch, an_enumeration_answers_no_more_devices_than_the_guest_had_room_for)
{ GuestContext guest;
  const std::uint64_t instance = guest.instance();
  guest.driver().enumerate(VK_INCOMPLETE, 2u, 5u);
  CHECK(guest.enumerate(instance, 2u));
  std::int32_t  result = 0;
  std::uint32_t count = 0u;
  std::uint64_t listed[4] = {};
  CHECK_EQ(read_enumeration(guest, result, count, listed, 4u), 2u);
  CHECK_EQ(result, VK_INCOMPLETE);
  CHECK_EQ(count, 2u);
  CHECK(guest.knows(listed[0]));
  CHECK(guest.knows(listed[1]));
  CHECK(!guest.knows(stale)); }
