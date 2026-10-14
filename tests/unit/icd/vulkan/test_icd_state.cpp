/* The Vulkan ICD's state dump (docs/trace.md): what avinspect.exe shows of a
 * process's connections and handles, read by the state service's thread
 * without waiting on the thread that may be hung. */
#include "core/support/text_buffer.hpp"
#include "core/trace/state_reporter.hpp"
#include "core/trace/state_writer.hpp"
#include "guest/transport/guest_connection.hpp"
#include "guest/transport/kmt_transport.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "harness/guest/scripted_host.hpp"
#include "harness/icd/icd_rig.hpp"
#include "harness/icd/lock_trace.hpp"
#include "harness/icd/recording_thread_lock.hpp"
#include "icd/vulkan/command_batch.hpp"
#include "icd/vulkan/connection.hpp"
#include "icd/vulkan/device_memory.hpp"
#include "icd/vulkan/handle_registry.hpp"
#include "icd/vulkan/icd_root.hpp"
#include "icd/vulkan/loader_handle.hpp"
#include "proto/client/icd_connection.hpp"
#include "proto/submission/command_header.hpp"
#include "proto/client/icd_handle_registry.hpp"
#include "proto/wire/wire_writer.hpp"

#include <cstdint>
#include <cstring>

using av::IcdRig;
using av::MockKmt;
using av::ScriptedHost;
using av::StateWriter;
using av::TextBuffer;
using av::WireWriter;
using av::icd::Connection;

namespace av::icd {

struct CommandBatchProbe {
    static const std::uint8_t *block(const CommandBatch &batch) { return batch.framed_.bytes(); }
};

struct ConnectionProbe {
    static std::uint32_t device(const Connection &c) { return c.connection_.device(); }
    static std::uint32_t context(const Connection &c) { return c.channel_.transport().context(); }
};

}  // namespace av::icd

namespace {

using CreateDevice = VkResult (*)(VkPhysicalDevice, const VkDeviceCreateInfo *, const VkAllocationCallbacks *,
                                  VkDevice *);
using AllocateCommandBuffers = VkResult (*)(VkDevice, const VkCommandBufferAllocateInfo *, VkCommandBuffer *);
using BeginCommandBuffer = VkResult (*)(VkCommandBuffer, const VkCommandBufferBeginInfo *);
using CmdDraw = void (*)(VkCommandBuffer, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t);
using FreeMemory = void (*)(VkDevice, VkDeviceMemory, const VkAllocationCallbacks *);

constexpr std::uint64_t command_buffer_host = 0x7700u;
constexpr std::uint32_t draw_bytes = 32u;

/* The line `connection/N ...` the rig's connection writes, with the handles
 * MockKmt gave it. */
void connection_line(TextBuffer &out, std::uint32_t number, const Connection &connection)
{
    out.append("connection/");
    out.append_decimal(number);
    out.append(" opened=true faulted=false adapter=");
    out.append_decimal(connection.adapter());
    out.append(" luid=0x0000000200001234 device=");
    out.append_decimal(av::icd::ConnectionProbe::device(connection));
    out.append(" context=");
    out.append_decimal(av::icd::ConnectionProbe::context(connection));
}

}  // namespace

TEST(icd_state, dumps_each_connection_with_its_handles_memory_and_batches)
{ IcdRig rig;
  const VkInstance       instance = rig.create_instance();
  const VkPhysicalDevice physical_device = rig.physical_device(instance);
  VkDeviceCreateInfo     create = {};
  create.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  rig.host().vk_handle(0, ScriptedHost::vk_device);
  VkDevice device = VK_NULL_HANDLE;
  CHECK_EQ(rig.entry<CreateDevice>(instance, "vkCreateDevice")(physical_device, &create, nullptr, &device), VK_SUCCESS);
  std::uint8_t answer[MockKmt::max_answer] = {};
  WireWriter   writer(answer, sizeof answer);
  writer.write_pod<std::int32_t>(VK_SUCCESS);
  writer.write_pod(command_buffer_host);
  rig.kmt().queue_answer(answer, static_cast<std::uint32_t>(writer.offset()));
  VkCommandBufferAllocateInfo allocate = {};
  allocate.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocate.commandBufferCount = 1u;
  VkCommandBuffer command_buffer = VK_NULL_HANDLE;
  CHECK_EQ(rig.entry<AllocateCommandBuffers>(instance, "vkAllocateCommandBuffers")(device, &allocate, &command_buffer),
           VK_SUCCESS);
  VkCommandBufferBeginInfo begin = {};
  begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  CHECK_EQ(rig.entry<BeginCommandBuffer>(instance, "vkBeginCommandBuffer")(command_buffer, &begin), VK_SUCCESS);
  rig.entry<CmdDraw>(instance, "vkCmdDraw")(command_buffer, 3u, 1u, 0u, 0u);
  /* The batch holds the Begin as its encoder framed it, then the 32-byte draw. */
  av::CommandHeader begun;
  CHECK_STATUS(begun.decode(av::icd::CommandBatchProbe::block(
                                *static_cast<av::icd::LoaderHandle *>(static_cast<void *>(command_buffer))->batch()),
                            av::CommandHeader::encoded_size),
               av::Status::ok);
  const std::uint32_t recorded = begun.size() + draw_bytes;
  rig.entry<FreeMemory>(instance, "vkFreeMemory")(device, VK_NULL_HANDLE, nullptr);
  /* A second instance, with nothing made through it: its connection comes
   * second, and its instance is numbered after the first one's handles. */
  const VkInstance other = rig.create_instance();
  rig.trace().clear();

  char        bytes[2048];
  StateWriter dump(bytes, sizeof bytes);
  av::StateReporter &reporter = rig.root();
  reporter.report(dump);
  CHECK(dump.invariants_hold());
  CHECK(!dump.truncated());
  CHECK(std::strcmp(rig.trace().text(), "Hh") == 0);
  CHECK(rig.root().invariants_hold());

  char       expected_bytes[2048];
  TextBuffer expected(expected_bytes, sizeof expected_bytes);
  expected.append("root/0 busy=false connections=2 handles=5 batches=1 memories=1\n");
  connection_line(expected, 0u, *av::icd::connection_of(instance));
  expected.append("\nconnection/0/handle/0 kind=instance host=0x0000000000001100 parent=none\n"
                  "connection/0/handle/1 kind=physical_device host=0x0000000000002200 parent=0\n"
                  "connection/0/handle/2 kind=device host=0x0000000000003300 parent=1\n"
                  "connection/0/handle/2/memory/0 types_learnt=false host_allocations=0\n"
                  "connection/0/handle/3 kind=command_buffer host=0x0000000000007700 parent=2\n"
                  "connection/0/handle/3/batch/0 recorded=");
  expected.append_decimal(recorded);
  expected.append(" capacity=");
  expected.append_decimal(av::icd::CommandBatch::first_block);
  expected.append(" failed=false\n");
  connection_line(expected, 1u, *av::icd::connection_of(other));
  expected.append("\nconnection/1/handle/4 kind=instance host=0x0000000000001100 parent=none");
  CHECK(std::strcmp(dump.dump(), expected.text()) == 0);

  rig.destroy_instance(other);
  rig.destroy_instance(instance); }

TEST(icd_state, a_root_lock_held_elsewhere_is_reported_busy_without_waiting)
{ IcdRig rig;
  const VkInstance instance = rig.create_instance();
  rig.trace().clear();
  rig.lock().held_elsewhere(true);
  char        bytes[256];
  StateWriter dump(bytes, sizeof bytes);
  rig.root().report(dump);
  CHECK(std::strcmp(dump.dump(), "root/0 busy=true") == 0);
  CHECK(std::strcmp(rig.trace().text(), "!") == 0);
  CHECK(!rig.lock().held());
  rig.lock().held_elsewhere(false);
  rig.destroy_instance(instance); }

TEST(icd_state, an_empty_root_is_its_counts)
{ IcdRig rig;
  char        bytes[256];
  StateWriter dump(bytes, sizeof bytes);
  rig.root().report(dump);
  CHECK(std::strcmp(dump.dump(), "root/0 busy=false connections=0 handles=0 batches=0 memories=0") == 0); }

TEST(icd_state, a_closed_connection_reports_nothing_it_does_not_hold)
{ IcdRig rig;
  av::LockTrace           trace;
  av::RecordingThreadLock lock(trace, 'C');
  Connection              connection(rig.kmt(), rig.platform(), lock, rig.log());
  char                    bytes[256];
  StateWriter             dump(bytes, sizeof bytes);
  dump.open("connection", 0u);
  connection.report(dump);
  CHECK(std::strcmp(dump.dump(),
                    "connection/0 opened=false faulted=false adapter=0 luid=0x0000000000000000 device=0 context=0") == 0); }

TEST(recording_thread_lock, try_acquire_takes_a_free_lock_and_refuses_a_held_one)
{ av::LockTrace           trace;
  av::RecordingThreadLock lock(trace, 'L');
  CHECK(lock.try_acquire());
  CHECK(lock.held());
  CHECK(!lock.try_acquire());
  lock.release();
  lock.held_elsewhere(true);
  CHECK(!lock.try_acquire());
  CHECK(!lock.held());
  lock.held_elsewhere(false);
  CHECK_EQ(lock.acquisitions(), 1u);
  CHECK(std::strcmp(trace.text(), "L!l!") == 0);
  CHECK(!lock.claim_violation()); }
