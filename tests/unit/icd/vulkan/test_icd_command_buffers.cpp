/* Command buffers and queues as the loader reaches them through the root
 * (research.md §5): a command buffer records into its own batch through the
 * generated vkCmd* wrappers and costs one submission when it ends; begin drops a
 * previous recording, reset drops it and goes out at once, free and device
 * destruction take the handles and their batches. Queues and command buffers
 * are made through their device. The host is played over MockKmt, invariants
 * after every call that changes the root. */
#include "core/support/object_pool.hpp"
#include "core/support/status.hpp"
#include "guest/transport/kmt_transport.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "harness/guest/scripted_host.hpp"
#include "harness/icd/icd_rig.hpp"
#include "harness/icd/shipped_batch.hpp"
#include "icd/vulkan/connection.hpp"
#include "icd/vulkan/icd_root.hpp"
#include "icd/vulkan/loader_handle.hpp"
#include "proto/generated/vulkan/vk_protocol_ids.h"
#include "proto/generated/vulkan/vk_serialize.h"
#include "proto/icd_handle_registry.hpp"
#include "proto/submission/command_header.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

#include <cstdint>

using av::CommandHeader;
using av::IcdRig;
using av::KmtTransport;
using av::MockKmt;
using av::ScriptedHost;
using av::ShippedBatch;
using av::Status;
using av::WireReader;
using av::WireWriter;
using av::icd::Connection;
using av::icd::LoaderHandle;
using Kind = av::icd::LoaderHandle::Kind;
using namespace av::vkproto;

namespace av {

struct KmtTransportProbe {
    static std::uint32_t ring(KmtTransport &t) { return t.allocations_[0]; }
};

}  // namespace av

namespace av::icd {

struct ConnectionProbe {
    static KmtTransport &transport(Connection &c) { return c.transport_.object(); }
};

struct IcdRootProbe {
    static ObjectPool<CommandBatch> &batches(IcdRoot &root) { return root.batches_; }
};

struct CommandBatchProbe {
    static std::size_t &used(CommandBatch &batch) { return batch.used_; }
};

}  // namespace av::icd

namespace av {

struct ObjectPoolProbe {
    template <typename T>
    static void miscount(ObjectPool<T> &pool, std::int32_t by) { pool.live_ += static_cast<std::uint32_t>(by); }
};

}  // namespace av

using av::KmtTransportProbe;
using av::ObjectPoolProbe;
using av::icd::CommandBatchProbe;
using av::icd::ConnectionProbe;
using av::icd::IcdRootProbe;

namespace {

using CreateDevice = VkResult (*)(VkPhysicalDevice, const VkDeviceCreateInfo *, const VkAllocationCallbacks *,
                                  VkDevice *);
using DestroyDevice = void (*)(VkDevice, const VkAllocationCallbacks *);
using GetDeviceQueue = void (*)(VkDevice, std::uint32_t, std::uint32_t, VkQueue *);
using GetDeviceQueue2 = void (*)(VkDevice, const VkDeviceQueueInfo2 *, VkQueue *);
using AllocateCommandBuffers = VkResult (*)(VkDevice, const VkCommandBufferAllocateInfo *, VkCommandBuffer *);
using FreeCommandBuffers = void (*)(VkDevice, VkCommandPool, std::uint32_t, const VkCommandBuffer *);
using BeginCommandBuffer = VkResult (*)(VkCommandBuffer, const VkCommandBufferBeginInfo *);
using EndCommandBuffer = VkResult (*)(VkCommandBuffer);
using ResetCommandBuffer = VkResult (*)(VkCommandBuffer, VkCommandBufferResetFlags);
using CmdDraw = void (*)(VkCommandBuffer, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t);
using GetFenceStatus = VkResult (*)(VkDevice, VkFence);

constexpr std::uint64_t queue_host = 0x6600u;
constexpr std::uint64_t command_buffer_host = 0x7700u;
constexpr std::uint64_t pool_host = 0x7F00u;
constexpr std::int32_t  vk_error_initialization_failed = -3;
/* A vkCmdDraw on the wire, header included. */
constexpr std::uint32_t draw_bytes = 32u;

/* An instance, its physical device and a device, made through the root as the
 * loader makes them. */
class DeviceRig {
public:
    DeviceRig()
    {
        instance_ = rig_.create_instance();
        physical_device_ = rig_.physical_device(instance_);
        before_device_ = rig_.platform().live_allocations();
        VkDeviceCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        rig_.host().vk_handle(0, ScriptedHost::vk_device);
        CHECK_EQ(entry<CreateDevice>("vkCreateDevice")(physical_device_, &info, nullptr, &device_), VK_SUCCESS);
        CHECK(rig_.root().invariants_hold());
    }
    ~DeviceRig() { rig_.destroy_instance(instance_); }

    IcdRig          &rig() { return rig_; }
    MockKmt         &kmt() { return rig_.kmt(); }
    VkInstance       instance() const { return instance_; }
    VkDevice         device() const { return device_; }
    std::uint32_t    before_device() const { return before_device_; }

    template <typename Function>
    Function entry(const char *name) { return rig_.entry<Function>(instance_, name); }

    /* `count` command buffers the host numbers from command_buffer_host. */
    VkResult allocate(std::uint32_t count, VkCommandBuffer *command_buffers)
    {
        std::uint8_t answer[MockKmt::max_answer] = {};
        WireWriter   writer(answer, sizeof answer);
        writer.write_pod<std::int32_t>(VK_SUCCESS);
        for (std::uint32_t index = 0u; index < count; index++) writer.write_pod(command_buffer_host + index);
        rig_.kmt().queue_answer(answer, static_cast<std::uint32_t>(writer.offset()));
        VkCommandBufferAllocateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        info.commandPool = reinterpret_cast<VkCommandPool>(pool_host);
        info.commandBufferCount = count;
        const VkResult result = entry<AllocateCommandBuffers>("vkAllocateCommandBuffers")(device_, &info, command_buffers);
        CHECK(rig_.root().invariants_hold());
        return result;
    }

    VkResult begin(VkCommandBuffer command_buffer)
    {
        VkCommandBufferBeginInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        const VkResult result = entry<BeginCommandBuffer>("vkBeginCommandBuffer")(command_buffer, &info);
        CHECK(rig_.root().invariants_hold());
        return result;
    }

    void draw(VkCommandBuffer command_buffer, std::uint32_t vertices)
    {
        entry<CmdDraw>("vkCmdDraw")(command_buffer, vertices, 1u, 0u, 0u);
        CHECK(rig_.root().invariants_hold());
    }

    VkResult end(VkCommandBuffer command_buffer)
    {
        const VkResult result = entry<EndCommandBuffer>("vkEndCommandBuffer")(command_buffer);
        CHECK(rig_.root().invariants_hold());
        return result;
    }

    std::uint32_t ring()
    {
        return KmtTransportProbe::ring(ConnectionProbe::transport(*av::icd::connection_of(instance_)));
    }

    /* The last batch shipped: Begin, draws of `first` onwards, End. */
    void check_recording(std::uint64_t host, std::uint32_t first, std::uint32_t draws)
    {
        ShippedBatch shipped(rig_.kmt(), ring());
        CHECK_STATUS(shipped.read(), Status::ok);
        CHECK_EQ(shipped.commands(), draws + 2u);
        if (shipped.commands() != draws + 2u) return;
        CHECK_EQ(shipped.header(0u).id(), static_cast<std::uint32_t>(kCmd_vkBeginCommandBuffer));
        CHECK_EQ(WireReader(shipped.body(0u), 8u).read_u64(), host);
        for (std::uint32_t index = 0u; index < draws; index++) {
            CHECK_EQ(shipped.header(index + 1u).id(), static_cast<std::uint32_t>(kCmd_vkCmdDraw));
            CHECK_EQ(shipped.header(index + 1u).size(), draw_bytes);
            WireReader body(shipped.body(index + 1u), draw_bytes - CommandHeader::encoded_size);
            CHECK_EQ(body.read_u64(), host);
            CHECK_EQ(body.read_u32(), first + index);
        }
        CHECK_EQ(shipped.header(draws + 1u).id(), static_cast<std::uint32_t>(kCmd_vkEndCommandBuffer));
        CHECK_EQ(shipped.header(draws + 1u).size(), CommandHeader::encoded_size + 8u);
    }

private:
    IcdRig           rig_;
    VkInstance       instance_ = VK_NULL_HANDLE;
    VkPhysicalDevice physical_device_ = VK_NULL_HANDLE;
    VkDevice         device_ = VK_NULL_HANDLE;
    std::uint32_t    before_device_ = 0u;
};

/* The body of the command journalled last, past its header. */
WireReader last_body(MockKmt &kmt)
{
    return WireReader(kmt.command_bytes(kmt.commands() - 1u) + CommandHeader::encoded_size,
                      MockKmt::journal_bytes - CommandHeader::encoded_size);
}

}  // namespace

TEST(icd_command_buffers, a_thousand_draws_through_the_generated_wrappers_cost_one_submission)
{ DeviceRig rig;
  VkCommandBuffer command_buffer = VK_NULL_HANDLE;
  CHECK_EQ(rig.allocate(1u, &command_buffer), VK_SUCCESS);
  const std::uint32_t submitted = rig.kmt().calls(MockKmt::Call::submit);
  CHECK_EQ(rig.begin(command_buffer), VK_SUCCESS);
  for (std::uint32_t index = 0u; index < 1000u; index++) rig.draw(command_buffer, index);
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::submit), submitted);
  CHECK_EQ(rig.end(command_buffer), VK_SUCCESS);
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::submit), submitted + 1u);
  rig.check_recording(command_buffer_host, 0u, 1000u); }

TEST(icd_command_buffers, begin_drops_a_recording_never_ended)
{ DeviceRig rig;
  VkCommandBuffer command_buffer = VK_NULL_HANDLE;
  CHECK_EQ(rig.allocate(1u, &command_buffer), VK_SUCCESS);
  CHECK_EQ(rig.begin(command_buffer), VK_SUCCESS);
  rig.draw(command_buffer, 1u);
  rig.draw(command_buffer, 2u);
  CHECK_EQ(rig.begin(command_buffer), VK_SUCCESS);
  rig.draw(command_buffer, 3u);
  CHECK_EQ(rig.end(command_buffer), VK_SUCCESS);
  rig.check_recording(command_buffer_host, 3u, 1u);
  /* And after an end, a new recording ships only itself. */
  CHECK_EQ(rig.begin(command_buffer), VK_SUCCESS);
  CHECK_EQ(rig.end(command_buffer), VK_SUCCESS);
  rig.check_recording(command_buffer_host, 0u, 0u); }

TEST(icd_command_buffers, reset_drops_the_recording_and_reaches_the_host_at_once)
{ DeviceRig rig;
  VkCommandBuffer command_buffer = VK_NULL_HANDLE;
  CHECK_EQ(rig.allocate(1u, &command_buffer), VK_SUCCESS);
  CHECK_EQ(rig.begin(command_buffer), VK_SUCCESS);
  rig.draw(command_buffer, 9u);
  const std::uint32_t submitted = rig.kmt().calls(MockKmt::Call::submit);
  CHECK_EQ(rig.entry<ResetCommandBuffer>("vkResetCommandBuffer")(command_buffer, VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT),
           VK_SUCCESS);
  CHECK(rig.rig().root().invariants_hold());
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::submit), submitted + 1u);
  CHECK_EQ(rig.kmt().command_id(rig.kmt().commands() - 1u), static_cast<std::uint32_t>(kCmd_vkResetCommandBuffer));
  WireReader reset = last_body(rig.kmt());
  CHECK_EQ(reset.read_u64(), command_buffer_host);
  /* End without a new begin ships only the End: the draw went with the reset. */
  CHECK_EQ(rig.end(command_buffer), VK_SUCCESS);
  ShippedBatch shipped(rig.kmt(), rig.ring());
  CHECK_STATUS(shipped.read(), Status::ok);
  CHECK_EQ(shipped.commands(), 1u);
  CHECK_EQ(shipped.header(0u).id(), static_cast<std::uint32_t>(kCmd_vkEndCommandBuffer)); }

TEST(icd_command_buffers, destroying_a_device_destroys_its_queues_command_buffers_and_their_batches)
{ DeviceRig rig;
  VkQueue queue = VK_NULL_HANDLE;
  rig.rig().host().vk_handle(0, queue_host);
  rig.entry<GetDeviceQueue>("vkGetDeviceQueue")(rig.device(), 0u, 0u, &queue);
  CHECK(rig.rig().root().invariants_hold());
  VkQueue second = VK_NULL_HANDLE;
  VkDeviceQueueInfo2 info = {};
  info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_INFO_2;
  info.queueIndex = 1u;
  rig.rig().host().vk_handle(0, queue_host + 1u);
  rig.entry<GetDeviceQueue2>("vkGetDeviceQueue2")(rig.device(), &info, &second);
  CHECK(rig.rig().root().invariants_hold());
  CHECK_EQ(handle_to_wire(queue), queue_host);
  CHECK_EQ(handle_to_wire(second), queue_host + 1u);
  WireReader asked = last_body(rig.kmt());
  CHECK_EQ(asked.read_u64(), ScriptedHost::vk_device);
  VkCommandBuffer command_buffers[2] = {};
  CHECK_EQ(rig.allocate(2u, command_buffers), VK_SUCCESS);
  CHECK_EQ(handle_to_wire(command_buffers[1]), command_buffer_host + 1u);
  CHECK_EQ(rig.begin(command_buffers[0]), VK_SUCCESS);
  rig.draw(command_buffers[0], 1u);
  const LoaderHandle *const device = static_cast<LoaderHandle *>(static_cast<void *>(rig.device()));
  void *const children[] = {queue, second, command_buffers[0], command_buffers[1]};
  for (void *const child : children) CHECK(static_cast<LoaderHandle *>(child)->parent() == device);
  rig.entry<DestroyDevice>("vkDestroyDevice")(rig.device(), nullptr);
  CHECK(rig.rig().root().invariants_hold());
  CHECK(av::icd::connection_of(queue) == nullptr);
  CHECK(av::icd::connection_of(second) == nullptr);
  CHECK(av::icd::connection_of(command_buffers[0]) == nullptr);
  CHECK(av::icd::connection_of(command_buffers[1]) == nullptr);
  CHECK_EQ(rig.rig().platform().live_allocations(), rig.before_device()); }

TEST(icd_command_buffers, free_sends_the_free_and_takes_the_handles_and_their_batches)
{ DeviceRig rig;
  const std::uint32_t before = rig.rig().platform().live_allocations();
  VkCommandBuffer command_buffers[2] = {};
  CHECK_EQ(rig.allocate(2u, command_buffers), VK_SUCCESS);
  for (VkCommandBuffer command_buffer : command_buffers) {
      CHECK_EQ(rig.begin(command_buffer), VK_SUCCESS);
      rig.draw(command_buffer, 4u);
  }
  const VkCommandBuffer freed[3] = {command_buffers[0], VK_NULL_HANDLE, command_buffers[1]};
  rig.entry<FreeCommandBuffers>("vkFreeCommandBuffers")(rig.device(), reinterpret_cast<VkCommandPool>(pool_host), 3u,
                                                        freed);
  CHECK(rig.rig().root().invariants_hold());
  CHECK_EQ(rig.kmt().command_id(rig.kmt().commands() - 1u), static_cast<std::uint32_t>(kCmd_vkFreeCommandBuffers));
  WireReader body = last_body(rig.kmt());
  CHECK_EQ(body.read_u64(), ScriptedHost::vk_device);
  CHECK_EQ(body.read_u64(), pool_host);
  CHECK_EQ(body.read_u32(), 3u);
  CHECK_EQ(body.read_u64(), command_buffer_host);
  CHECK_EQ(body.read_u64(), 0u);
  CHECK_EQ(body.read_u64(), command_buffer_host + 1u);
  CHECK(av::icd::connection_of(command_buffers[0]) == nullptr);
  CHECK(av::icd::connection_of(command_buffers[1]) == nullptr);
  CHECK_EQ(rig.rig().platform().live_allocations(), before);
  CHECK_EQ(rig.rig().log().holding("did not make"), 0u); }

TEST(icd_command_buffers, a_foreign_device_makes_and_frees_nothing)
{ DeviceRig rig;
  int stack = 0;
  const VkDevice foreign = reinterpret_cast<VkDevice>(&stack);
  const std::uint32_t sent = rig.kmt().calls(MockKmt::Call::submit);
  VkCommandBufferAllocateInfo info = {};
  info.commandBufferCount = 1u;
  VkCommandBuffer command_buffer = reinterpret_cast<VkCommandBuffer>(&stack);
  CHECK_EQ(rig.entry<AllocateCommandBuffers>("vkAllocateCommandBuffers")(foreign, &info, &command_buffer),
           VK_ERROR_DEVICE_LOST);
  CHECK_EQ(rig.rig().log().holding("vkAllocateCommandBuffers on a device this driver did not make"), 1u);
  rig.entry<FreeCommandBuffers>("vkFreeCommandBuffers")(foreign, VK_NULL_HANDLE, 1u, &command_buffer);
  VkQueue queue = reinterpret_cast<VkQueue>(&stack);
  rig.entry<GetDeviceQueue>("vkGetDeviceQueue")(foreign, 0u, 0u, &queue);
  VkDeviceQueueInfo2 queue_info = {};
  rig.entry<GetDeviceQueue2>("vkGetDeviceQueue2")(foreign, &queue_info, &queue);
  CHECK(queue == reinterpret_cast<VkQueue>(&stack));
  /* A command buffer is not a device either. */
  VkCommandBuffer made = VK_NULL_HANDLE;
  CHECK_EQ(rig.allocate(1u, &made), VK_SUCCESS);
  rig.entry<GetDeviceQueue>("vkGetDeviceQueue")(reinterpret_cast<VkDevice>(made), 0u, 0u, &queue);
  CHECK(queue == reinterpret_cast<VkQueue>(&stack));
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::submit), sent + 1u);
  CHECK(rig.rig().root().invariants_hold()); }

TEST(icd_command_buffers, a_queue_the_host_does_not_answer_is_no_handle)
{ DeviceRig rig;
  VkQueue queue = reinterpret_cast<VkQueue>(&rig);
  rig.kmt().withhold_answer_at(1u);
  rig.entry<GetDeviceQueue>("vkGetDeviceQueue")(rig.device(), 0u, 0u, &queue);
  CHECK(queue == VK_NULL_HANDLE);
  CHECK_EQ(rig.rig().log().holding("no handle for host object, status"), 1u);
  CHECK(rig.rig().root().invariants_hold()); }

TEST(icd_command_buffers, allocation_passes_on_the_hosts_refusal_and_refuses_more_than_it_stages)
{ DeviceRig rig;
  VkCommandBufferAllocateInfo info = {};
  info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  info.commandBufferCount = 1u;
  VkCommandBuffer command_buffer = VK_NULL_HANDLE;
  /* The generated decoder reads a handle for every one asked for, whatever the result. */
  rig.rig().host().vk_handle(vk_error_initialization_failed, 0u);
  CHECK_EQ(rig.entry<AllocateCommandBuffers>("vkAllocateCommandBuffers")(rig.device(), &info, &command_buffer),
           vk_error_initialization_failed);
  CHECK(command_buffer == VK_NULL_HANDLE);
  CHECK(rig.rig().root().invariants_hold());
  const std::uint32_t submitted = rig.kmt().calls(MockKmt::Call::submit);
  info.commandBufferCount = 1025u;
  CHECK_EQ(rig.entry<AllocateCommandBuffers>("vkAllocateCommandBuffers")(rig.device(), &info, &command_buffer),
           VK_ERROR_OUT_OF_HOST_MEMORY);
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::submit), submitted);
  CHECK_EQ(rig.rig().log().holding("more command buffers at once than 1024"), 1u); }

/* The second command buffer's batch needs a new chunk of batches, which is
 * refused: the first goes back to the host and both answers are null. */
TEST(icd_command_buffers, no_memory_for_a_batch_frees_the_command_buffers_already_made)
{ DeviceRig rig;
  Connection *const connection = av::icd::connection_of(rig.instance());
  const std::uint32_t fillers = av::ObjectPool<LoaderHandle>::chunk_slots - 1u;
  for (std::uint64_t host = 0u; host < fillers; host++)
      CHECK(rig.rig().root().wrap(Kind::command_buffer, 0x90000u + host, connection) != nullptr);
  const std::uint32_t before = rig.rig().platform().live_allocations();
  rig.rig().platform().fail_allocation_at(1u);
  VkCommandBuffer command_buffers[2] = {reinterpret_cast<VkCommandBuffer>(&rig), reinterpret_cast<VkCommandBuffer>(&rig)};
  CHECK_EQ(rig.allocate(2u, command_buffers), VK_ERROR_OUT_OF_HOST_MEMORY);
  CHECK(command_buffers[0] == VK_NULL_HANDLE && command_buffers[1] == VK_NULL_HANDLE);
  CHECK_EQ(rig.rig().log().holding("no batch for host command buffer 30465"), 1u);
  CHECK_EQ(rig.rig().log().holding("host command buffers left in their pool: 1"), 1u);
  CHECK_EQ(rig.kmt().command_id(rig.kmt().commands() - 1u), static_cast<std::uint32_t>(kCmd_vkFreeCommandBuffers));
  WireReader body = last_body(rig.kmt());
  CHECK_EQ(body.read_u64(), ScriptedHost::vk_device);
  CHECK_EQ(body.read_u64(), pool_host);
  CHECK_EQ(body.read_u32(), 1u);
  CHECK_EQ(body.read_u64(), command_buffer_host);
  CHECK_EQ(rig.rig().platform().live_allocations(), before); }

TEST(icd_command_buffers, a_command_buffer_wrapped_again_keeps_its_one_batch)
{ DeviceRig rig;
  Connection *const connection = av::icd::connection_of(rig.instance());
  void *const first = rig.rig().root().wrap(Kind::command_buffer, command_buffer_host, connection);
  const std::uint32_t allocations = rig.rig().platform().live_allocations();
  CHECK(rig.rig().root().wrap(Kind::command_buffer, command_buffer_host, connection) == first);
  CHECK(rig.rig().root().invariants_hold());
  CHECK_EQ(rig.rig().platform().live_allocations(), allocations);
  CHECK(&rig.rig().root().record_target(static_cast<VkCommandBuffer>(first)) ==
        static_cast<LoaderHandle *>(first)->batch()); }

TEST(icd_command_buffers, begin_with_no_memory_to_record_into_answers_out_of_host_memory_and_so_does_end)
{ DeviceRig rig;
  VkCommandBuffer command_buffer = VK_NULL_HANDLE;
  CHECK_EQ(rig.allocate(1u, &command_buffer), VK_SUCCESS);
  const std::uint32_t submitted = rig.kmt().calls(MockKmt::Call::submit);
  rig.rig().platform().fail_allocation_at(1u);
  CHECK_EQ(rig.begin(command_buffer), VK_ERROR_OUT_OF_HOST_MEMORY);
  CHECK_EQ(rig.rig().log().holding("vkBeginCommandBuffer: no memory to record into"), 1u);
  rig.draw(command_buffer, 1u);
  CHECK_EQ(rig.end(command_buffer), VK_ERROR_OUT_OF_HOST_MEMORY);
  CHECK_EQ(rig.rig().log().holding("vkEndCommandBuffer: batch not shipped, status 2"), 1u);
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::submit), submitted);
  /* The failure went with the recording. */
  CHECK_EQ(rig.begin(command_buffer), VK_SUCCESS);
  CHECK_EQ(rig.end(command_buffer), VK_SUCCESS);
  rig.check_recording(command_buffer_host, 0u, 0u); }

TEST(icd_command_buffers, ending_on_a_lost_connection_answers_device_lost)
{ DeviceRig rig;
  VkCommandBuffer command_buffer = VK_NULL_HANDLE;
  CHECK_EQ(rig.allocate(1u, &command_buffer), VK_SUCCESS);
  CHECK_EQ(rig.begin(command_buffer), VK_SUCCESS);
  rig.draw(command_buffer, 1u);
  rig.kmt().withhold_answer_at(1u);
  CHECK_EQ(rig.entry<GetFenceStatus>("vkGetFenceStatus")(rig.device(), VK_NULL_HANDLE), VK_ERROR_DEVICE_LOST);
  CHECK_EQ(rig.end(command_buffer), VK_ERROR_DEVICE_LOST);
  CHECK_EQ(rig.rig().log().holding("vkEndCommandBuffer: batch not shipped, status"), 1u); }

/* The root's own spellings of the entry points, as the loader's table reaches them. */
TEST(icd_command_buffers, the_roots_methods_answer_as_its_entry_points_do)
{ DeviceRig rig;
  av::icd::IcdRoot &root = rig.rig().root();
  VkQueue queue = VK_NULL_HANDLE;
  rig.rig().host().vk_handle(0, queue_host);
  root.device_queue(rig.device(), 0u, 0u, &queue);
  CHECK(root.invariants_hold());
  VkDeviceQueueInfo2 info = {};
  info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_INFO_2;
  VkQueue again = VK_NULL_HANDLE;
  rig.rig().host().vk_handle(0, queue_host);
  root.device_queue2(rig.device(), &info, &again);
  CHECK(root.invariants_hold());
  CHECK(queue != VK_NULL_HANDLE && again == queue);
  VkCommandBuffer command_buffer = VK_NULL_HANDLE;
  CHECK_EQ(rig.allocate(1u, &command_buffer), VK_SUCCESS);
  VkCommandBufferBeginInfo begin = {};
  begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  CHECK_EQ(root.begin_command_buffer(command_buffer, &begin), VK_SUCCESS);
  CHECK(root.invariants_hold());
  rig.draw(command_buffer, 11u);
  CHECK_EQ(root.end_command_buffer(command_buffer), VK_SUCCESS);
  CHECK(root.invariants_hold());
  rig.check_recording(command_buffer_host, 11u, 1u);
  CHECK_EQ(root.reset_command_buffer(command_buffer, 0u), VK_SUCCESS);
  CHECK(root.invariants_hold());
  CHECK_EQ(rig.kmt().command_id(rig.kmt().commands() - 1u), static_cast<std::uint32_t>(kCmd_vkResetCommandBuffer));
  root.free_command_buffers(rig.device(), reinterpret_cast<VkCommandPool>(pool_host), 1u, &command_buffer);
  CHECK(root.invariants_hold());
  CHECK(av::icd::connection_of(command_buffer) == nullptr); }

TEST(icd_command_buffers, invariants_see_a_batch_no_command_buffer_owns_or_one_that_does_not_frame)
{ DeviceRig rig;
  av::icd::IcdRoot &root = rig.rig().root();
  VkCommandBuffer command_buffer = VK_NULL_HANDLE;
  CHECK_EQ(rig.allocate(1u, &command_buffer), VK_SUCCESS);
  CHECK_EQ(rig.begin(command_buffer), VK_SUCCESS);
  av::ObjectPool<av::icd::CommandBatch> &batches = IcdRootProbe::batches(root);
  av::icd::CommandBatch *stray = nullptr;
  CHECK_STATUS(batches.create(stray, rig.rig().platform(), *av::icd::connection_of(rig.instance())), Status::ok);
  CHECK(!root.invariants_hold());
  CHECK_STATUS(batches.destroy(stray), Status::ok);
  CHECK(root.invariants_hold());
  ObjectPoolProbe::miscount(batches, 1);
  CHECK(!root.invariants_hold());
  ObjectPoolProbe::miscount(batches, -1);
  CHECK(root.invariants_hold());
  std::size_t &used = CommandBatchProbe::used(root.record_target(command_buffer));
  used += 1u;
  CHECK(!root.invariants_hold());
  used -= 1u;
  CHECK(root.invariants_hold()); }
