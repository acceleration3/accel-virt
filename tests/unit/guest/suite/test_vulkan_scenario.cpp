/* The Vulkan object chain over MockKmt, the test playing the host through
 * ScriptedHost: every step passes on VK_SUCCESS in the order an application
 * makes them; a failure at each is reported with the rest skipped; whatever
 * was made is destroyed in reverse and a no-op proves the destroys were sent;
 * and nothing is left open but the adapter the search owns. */
#include "core/support/status.hpp"
#include "guest/kmt/pci_identity.hpp"
#include "guest/suite/adapter_scenario.hpp"
#include "guest/kmt/adapter_search.hpp"
#include "guest/kmt/avgpu_adapters.hpp"
#include "guest/suite/guest_report.hpp"
#include "guest/suite/vulkan_scenario.hpp"
#include "harness/platform/mock_platform.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "harness/guest/scripted_host.hpp"
#include "proto/escape/av_context_record.hpp"
#include "proto/generated/vulkan/vk_protocol_ids.h"
#include "proto/generated/vulkan/vk_serialize.h"
#include "proto/submission/command_header.hpp"
#include "proto/wire/wire_arena.hpp"
#include "proto/wire/wire_reader.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>

using av::AdapterScenario;
using av::AdapterSearch;
using av::AvgpuAdapters;
using av::CommandHeader;
using av::GuestReport;
using av::MockKmt;
using av::PciIdentity;
using av::ScriptedHost;
using av::Status;
using av::VulkanScenario;
using namespace av::vkproto;

namespace {

const PciIdentity av(AvgpuAdapters::virtio_vendor, AvgpuAdapters::virtio_gpu_device,
                      AvgpuAdapters::virtio_vendor, AvgpuAdapters::av_subsystem);
constexpr std::int32_t vk_error_initialization_failed = -3;
constexpr std::int32_t vk_error_device_lost = -4;
constexpr std::int32_t vk_error_out_of_device_memory = -2;
constexpr std::int32_t vk_error_incompatible_driver = -9;
constexpr std::uint32_t device_removed = 0xC00E0102u;

bool has_line(const GuestReport &report, const char *line)
{
    return std::strstr(report.text(), line) != nullptr;
}

class Rig {
public:
    Rig() : search_(kmt_), host_(kmt_)
    {
        kmt_.add_adapter(av, AdapterScenario::wddm_3_2);
        CHECK_STATUS(search_.find_avgpu(0u), Status::ok);
    }

    MockKmt       &kmt() { return kmt_; }
    AdapterSearch &search() { return search_; }
    ScriptedHost  &host() { return host_; }

    /* The ids of every command the scenario sent, in order, into `ids`. */
    std::uint32_t sent(std::uint32_t *ids, std::uint32_t capacity) const
    {
        const std::uint32_t count = kmt_.commands() < capacity ? kmt_.commands() : capacity;
        for (std::uint32_t index = 0u; index < count; index++) ids[index] = kmt_.command_id(index);
        return count;
    }

    void check_nothing_left_but_the_adapter()
    {
        CHECK_EQ(kmt_.open_objects(), 1u);
        CHECK_EQ(kmt_.misuses(), 0u);
        CHECK_EQ(kmt_.queued_answers(), 0u);
        CHECK_STATUS(search_.close(), Status::ok);
        CHECK_EQ(kmt_.open_objects(), 0u);
    }

private:
    MockKmt       kmt_;
    AdapterSearch search_;
    ScriptedHost  host_;
};

/* The first `answers` of a successful run's answers, in the order it waits. */
void answer_until(ScriptedHost &host, std::uint32_t answers)
{
    for (std::uint32_t answer = 0u; answer < answers; answer++) {
        switch (answer) {
            case 0u: host.vk_handle(0, ScriptedHost::vk_instance); break;
            case 1u: host.vk_count(0, 2u); break;
            case 2u: host.vk_physical_devices(0, 2u); break;
            case 3u: host.vk_handle(0, ScriptedHost::vk_device); break;
            case 4u: host.vk_handle(0, ScriptedHost::vk_buffer); break;
            case 5u: host.vk_memory_requirements(ScriptedHost::vk_memory_types); break;
            default: host.vk_handle(0, ScriptedHost::vk_memory); break;
        }
    }
}

bool same_ids(const std::uint32_t *sent, std::uint32_t count, const std::uint32_t *expected, std::uint32_t length)
{
    return count == length && std::memcmp(sent, expected, length * sizeof *sent) == 0;
}

}  // namespace

TEST(vulkan_scenario, the_chain_passes_on_the_host_and_is_destroyed_in_reverse)
{ Rig rig;
  rig.host().vulkan_run();
  av::MockPlatform report_memory;
  GuestReport report(report_memory);
  VulkanScenario scenario(rig.kmt(), report, rig.search());
  scenario.run();
  CHECK(std::strcmp(report.text(),
                    "ok vulkan.adapter\n"
                    "ok vulkan.connection\n"
                    "ok vulkan.initialize\n"
                    "ok vulkan.instance\n"
                    "ok vulkan.physical_device 2\n"
                    "ok vulkan.device\n"
                    "ok vulkan.buffer\n"
                    "ok vulkan.memory\n"
                    "ok vulkan.bind\n"
                    "ok vulkan.destroy\n") == 0);
  CHECK_EQ(report.failed(), 0u);
  std::uint32_t ids[32] = {};
  const std::uint32_t count = rig.sent(ids, 32u);
  const std::uint32_t expected[] = {kCmd_vkCreateInstance, kCmd_vkEnumeratePhysicalDevices,
                                    kCmd_vkEnumeratePhysicalDevices, kCmd_vkCreateDevice, kCmd_vkCreateBuffer,
                                    kCmd_vkGetBufferMemoryRequirements, kCmd_vkAllocateMemory,
                                    kCmd_vkBindBufferMemory, kCmd_vkFreeMemory, kCmd_vkDestroyBuffer,
                                    kCmd_vkDestroyDevice, kCmd_vkDestroyInstance, CommandHeader::nop_id};
  CHECK(same_ids(ids, count, expected, sizeof expected / sizeof expected[0]));
  CHECK(report.invariants_hold());
  /* The context the chain ran on asked the host for Vulkan. */
  av::AvContextRecord record;
  CHECK_STATUS(record.decode(rig.kmt().last_private(), rig.kmt().last_private_size()), Status::ok);
  CHECK(record.sub_api() == av::SubApi::vulkan);
  rig.check_nothing_left_but_the_adapter(); }

/* What crossed is what the host needs: the device it made named back, and the
 * memory type the lowest set bit of the buffer's requirements names (3 of
 * 0x28), for the size they asked; the buffer is 4 KiB of transfer destination. */
TEST(vulkan_scenario, allocates_the_lowest_memory_type_for_the_size_the_buffer_needs)
{ Rig rig;
  rig.host().vulkan_run();
  av::MockPlatform report_memory;
  GuestReport report(report_memory);
  VulkanScenario scenario(rig.kmt(), report, rig.search());
  scenario.run();
  CHECK_EQ(rig.kmt().command_id(4u), static_cast<std::uint32_t>(kCmd_vkCreateBuffer));
  CHECK_EQ(rig.kmt().command_id(6u), static_cast<std::uint32_t>(kCmd_vkAllocateMemory));
  std::uint8_t   block[256] = {};
  av::WireArena arena(block, sizeof block);
  { const std::uint8_t *bytes = rig.kmt().command_bytes(4u);
    av::WireReader body(bytes + CommandHeader::encoded_size, MockKmt::journal_bytes - CommandHeader::encoded_size);
    CHECK_EQ(body.read_u64(), ScriptedHost::vk_device);
    VkBufferCreateInfo buffer = {};
    CHECK(deserialize_VkBufferCreateInfo(body, &buffer, arena));
    CHECK_EQ(buffer.size, VulkanScenario::buffer_bytes);
    CHECK_EQ(buffer.usage, static_cast<VkBufferUsageFlags>(VK_BUFFER_USAGE_TRANSFER_DST_BIT)); }
  { const std::uint8_t *bytes = rig.kmt().command_bytes(6u);
    av::WireReader body(bytes + CommandHeader::encoded_size, MockKmt::journal_bytes - CommandHeader::encoded_size);
    CHECK_EQ(body.read_u64(), ScriptedHost::vk_device);
    VkMemoryAllocateInfo allocate = {};
    CHECK(deserialize_VkMemoryAllocateInfo(body, &allocate, arena));
    CHECK_EQ(allocate.memoryTypeIndex, 3u);
    CHECK_EQ(allocate.allocationSize, 8192u); }
  rig.check_nothing_left_but_the_adapter(); }

/* Each step's failure is the last step reported, with the host's own result on
 * the line; whatever was made before it is destroyed, device before instance,
 * and nothing that was never made is named to the host. */
TEST(vulkan_scenario, a_failure_at_each_step_destroys_only_what_was_made)
{ struct Case {
      std::uint32_t answers;      /* successful answers before the failing one */
      std::uint32_t failing;      /* which answer fails: 0 instance, 1 count, 2 list, 3 device, 4 buffer, 5 requirements, 6 allocate, 7 bind */
      const char   *line;
      const char   *skipped;
      std::uint32_t destroys;     /* commands between the last answered one and the no-op */
  };
  const Case cases[] = {
      {0u, 0u, "FAIL vulkan.instance status=7 4294967287\n", "vulkan.physical_device", 0u},
      {1u, 1u, "FAIL vulkan.physical_device status=7 4294967293\n", "vulkan.device", 1u},
      {2u, 2u, "FAIL vulkan.physical_device status=7 4294967293\n", "vulkan.device", 1u},
      {3u, 3u, "FAIL vulkan.device status=7 4294967287\n", "vulkan.buffer", 1u},
      {4u, 4u, "FAIL vulkan.buffer status=7 4294967294\n", "vulkan.memory", 4u},
      {5u, 5u, "FAIL vulkan.memory status=3 0\n", "vulkan.bind", 4u},
      {6u, 6u, "FAIL vulkan.memory status=7 4294967294\n", "vulkan.bind", 4u},
      {7u, 7u, "FAIL vulkan.bind status=7 4294967293\n", "vulkan.destroy", 4u}};
  for (const Case &failure : cases) {
      Rig rig;
      answer_until(rig.host(), failure.answers);
      switch (failure.failing) {
          case 0u: rig.host().vk_handle(vk_error_incompatible_driver, 0u); break;
          case 1u: rig.host().vk_count(vk_error_initialization_failed, 0u); break;
          case 2u: rig.host().vk_physical_devices(vk_error_initialization_failed, 0u); break;
          case 3u: rig.host().vk_handle(vk_error_incompatible_driver, 0u); break;
          case 4u: rig.host().vk_handle(vk_error_out_of_device_memory, 0u); break;
          case 5u: rig.host().vk_memory_requirements(0u); break;
          case 6u: rig.host().vk_handle(vk_error_out_of_device_memory, 0u); break;
          default: rig.host().vk_result(vk_error_initialization_failed); break;
      }
      rig.host().nop();
      av::MockPlatform report_memory;
      GuestReport report(report_memory);
      VulkanScenario scenario(rig.kmt(), report, rig.search());
      scenario.run();
      CHECK(has_line(report, failure.line));
      CHECK(!has_line(report, failure.skipped));
      CHECK(!has_line(report, "vulkan.destroy"));
      CHECK_EQ(report.failed(), 1u);
      /* After the command that failed come the destroys, then the no-op. */
      const std::uint32_t commands = rig.kmt().commands();
      CHECK_EQ(rig.kmt().command_id(commands - 1u), CommandHeader::nop_id);
      std::uint32_t destroyed = 0u;
      for (std::uint32_t index = 0u; index < commands; index++) {
          const std::uint32_t id = rig.kmt().command_id(index);
          destroyed += id == kCmd_vkFreeMemory || id == kCmd_vkDestroyBuffer || id == kCmd_vkDestroyDevice ||
                               id == kCmd_vkDestroyInstance
                           ? 1u
                           : 0u;
      }
      CHECK_EQ(destroyed, failure.destroys);
      if (failure.destroys != 0u)
          CHECK_EQ(rig.kmt().command_id(commands - 2u), static_cast<std::uint32_t>(kCmd_vkDestroyInstance));
      if (failure.destroys == 4u)
          CHECK_EQ(rig.kmt().command_id(commands - 3u), static_cast<std::uint32_t>(kCmd_vkDestroyDevice));
      rig.check_nothing_left_but_the_adapter();
  } }

/* A host with no GPU answers a count of zero: the step fails with the count on
 * the line, the list is never asked for, and only the instance is destroyed. */
TEST(vulkan_scenario, zero_physical_devices_fails_the_step_with_its_count)
{ Rig rig;
  rig.host().vk_handle(0, ScriptedHost::vk_instance);
  rig.host().vk_count(0, 0u);
  rig.host().nop();
  av::MockPlatform report_memory;
  GuestReport report(report_memory);
  VulkanScenario scenario(rig.kmt(), report, rig.search());
  scenario.run();
  CHECK(has_line(report, "ok vulkan.instance\n"));
  CHECK(has_line(report, "FAIL vulkan.physical_device status=3 0\n"));
  CHECK(!has_line(report, "vulkan.device"));
  std::uint32_t ids[8] = {};
  const std::uint32_t expected[] = {kCmd_vkCreateInstance, kCmd_vkEnumeratePhysicalDevices, kCmd_vkDestroyInstance,
                                    CommandHeader::nop_id};
  CHECK(same_ids(ids, rig.sent(ids, 8u), expected, 4u));
  rig.check_nothing_left_but_the_adapter(); }

/* The destroys are sent without waiting, so only the no-op after them says the
 * context took them: an answer with bytes in it fails the step. */
TEST(vulkan_scenario, a_no_op_after_the_destroys_answered_wrong_fails_destroy)
{ Rig rig;
  answer_until(rig.host(), 7u);
  rig.host().vk_result(0);
  rig.host().vk_result(0);   /* four bytes where the no-op answers none */
  av::MockPlatform report_memory;
  GuestReport report(report_memory);
  VulkanScenario scenario(rig.kmt(), report, rig.search());
  scenario.run();
  CHECK(has_line(report, "ok vulkan.bind\n"));
  CHECK(has_line(report, "FAIL vulkan.destroy status=9"));
  CHECK_EQ(report.failed(), 1u);
  rig.check_nothing_left_but_the_adapter(); }

/* A context the device lost mid-chain: the client answers VK_ERROR_DEVICE_LOST,
 * which is reported, and the destroys and no-op after it fail unreported
 * without leaving anything open. */
TEST(vulkan_scenario, a_lost_context_reports_device_lost_and_still_tears_down)
{ Rig rig;
  rig.host().vk_handle(0, ScriptedHost::vk_instance);
  rig.kmt().fail_at(MockKmt::Call::wait_for_context, 2u, device_removed);
  av::MockPlatform report_memory;
  GuestReport report(report_memory);
  VulkanScenario scenario(rig.kmt(), report, rig.search());
  scenario.run();
  char line[64] = {};
  static_cast<void>(std::snprintf(line, sizeof line, "FAIL vulkan.physical_device status=7 %u\n",
                                  static_cast<std::uint32_t>(vk_error_device_lost)));
  CHECK(has_line(report, line));
  CHECK_EQ(report.failed(), 1u);
  CHECK(!has_line(report, "vulkan.destroy"));
  /* The lost context sends nothing more, not even its destroys. */
  CHECK_EQ(rig.kmt().commands(), 2u);
  rig.check_nothing_left_but_the_adapter(); }

TEST(vulkan_scenario, a_context_that_cannot_be_set_up_is_reported_and_nothing_is_sent)
{ Rig rig;
  rig.kmt().fail_at(MockKmt::Call::create_context_virtual, 1u, 0xC0000001u);
  av::MockPlatform report_memory;
  GuestReport report(report_memory);
  VulkanScenario scenario(rig.kmt(), report, rig.search());
  scenario.run();
  CHECK(has_line(report, "ok vulkan.connection\n"));
  CHECK(has_line(report, "FAIL vulkan.initialize status=7"));
  CHECK(!has_line(report, "vulkan.instance"));
  CHECK_EQ(rig.kmt().commands(), 0u);
  rig.check_nothing_left_but_the_adapter(); }
