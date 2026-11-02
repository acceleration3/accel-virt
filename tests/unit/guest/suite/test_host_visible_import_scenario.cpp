#include "core/support/status.hpp"
#include "guest/kmt/pci_identity.hpp"
#include "guest/suite/adapter_scenario.hpp"
#include "guest/kmt/adapter_search.hpp"
#include "guest/kmt/avgpu_adapters.hpp"
#include "guest/suite/guest_report.hpp"
#include "guest/suite/host_visible_import_scenario.hpp"
#include "harness/platform/mock_platform.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "harness/guest/scripted_host.hpp"
#include "proto/generated/vulkan/vk_protocol_ids.h"
#include "proto/generated/vulkan/vk_serialize.h"
#include "proto/submission/blob_import_answer.hpp"
#include "proto/submission/blob_import_request.hpp"
#include "proto/submission/command_header.hpp"
#include "proto/wire/wire_arena.hpp"
#include "proto/wire/wire_reader.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

using av::AdapterScenario;
using av::AdapterSearch;
using av::AvgpuAdapters;
using av::BlobImportAnswer;
using av::BlobImportRequest;
using av::CommandHeader;
using av::GuestReport;
using av::HostVisibleImportScenario;
using av::MockKmt;
using av::PciIdentity;
using av::ScriptedHost;
using av::Status;
using namespace av::vkproto;

namespace {

const PciIdentity av(PciIdentity::virtio_vendor, AvgpuAdapters::virtio_gpu_device,
                      PciIdentity::virtio_vendor, AvgpuAdapters::av_subsystem);
constexpr std::int32_t  vk_timeout = 2;
constexpr std::int32_t  vk_error_out_of_device_memory = -2;
constexpr std::int32_t  vk_error_device_lost = -4;
constexpr std::int32_t  vk_error_incompatible_driver = -9;
constexpr std::uint32_t refused = 0xC0000001u;
constexpr std::uint32_t chunk_pages = static_cast<std::uint32_t>(HostVisibleImportScenario::chunk_bytes / 4096u);
constexpr std::uint32_t command_buffer_wait = 15u;

bool has_line(const GuestReport &report, const char *line)
{
    return std::strstr(report.text(), line) != nullptr;
}

class HostGpu {
public:
    HostGpu(MockKmt &kmt, std::uint32_t spoiled) : kmt_(kmt), spoiled_(spoiled), copies_(0u) {}

    void copy_at_wait(std::uint32_t nth)
    {
        kmt_.play_at_wait(nth, [](void *self) { static_cast<HostGpu *>(self)->copy(); }, this);
    }

    std::uint32_t copies() const { return copies_; }

private:
    void copy()
    {
        const std::uint8_t *const source = kmt_.window_chunk_memory(MockKmt::first_window_resource);
        std::uint8_t *const       destination = kmt_.window_chunk_memory(MockKmt::first_window_resource + 1u);
        if (source == nullptr || destination == nullptr) return;
        std::memcpy(destination, source, HostVisibleImportScenario::copy_bytes);
        for (std::uint32_t byte = 0u; byte < spoiled_; byte++)
            destination[HostVisibleImportScenario::copy_bytes - 1u - byte] = 0u;
        copies_++;
    }

    MockKmt      &kmt_;
    std::uint32_t spoiled_;
    std::uint32_t copies_;
};

class Rig {
public:
    Rig() : search_(kmt_), host_(kmt_), gpu_(kmt_, 0u)
    {
        kmt_.add_adapter(av, AdapterScenario::wddm_3_2);
        kmt_.open_host_window(2u * HostVisibleImportScenario::chunk_bytes);
        CHECK_STATUS(search_.find_avgpu(0u), Status::ok);
    }

    MockKmt       &kmt() { return kmt_; }
    AdapterSearch &search() { return search_; }
    ScriptedHost  &host() { return host_; }
    HostGpu       &gpu() { return gpu_; }

    void run(GuestReport &report)
    {
        HostVisibleImportScenario scenario(kmt_, report, search_);
        scenario.run();
    }

    void check_nothing_left_but_the_adapter()
    {
        CHECK_EQ(kmt_.open_objects(), 1u);
        CHECK_EQ(kmt_.misuses(), 0u);
        CHECK_EQ(kmt_.window_chunks(), 0u);
        CHECK_EQ(kmt_.queued_answers(), 0u);
        CHECK_STATUS(search_.close(), Status::ok);
    }

    std::uint32_t sent(std::uint32_t id) const
    {
        std::uint32_t count = 0u;
        for (std::uint32_t index = 0u; index < kmt_.commands(); index++)
            count += kmt_.command_id(index) == id ? 1u : 0u;
        return count;
    }

private:
    MockKmt       kmt_;
    AdapterSearch search_;
    ScriptedHost  host_;
    HostGpu       gpu_;
};

void answer(ScriptedHost &host, std::uint32_t index)
{
    const std::uint32_t side = index >= 9u ? 1u : 0u;
    switch (index) {
        case 0u: host.vk_handle(0, ScriptedHost::vk_instance); break;
        case 1u: host.vk_count(0, 1u); break;
        case 2u: host.vk_physical_devices(0, 1u); break;
        case 3u: host.vk_handle(0, ScriptedHost::vk_device); break;
        case 4u:
            host.vk_memory_properties(ScriptedHost::vk_host_memory_types, ScriptedHost::vk_host_visible_types);
            break;
        case 5u: case 9u: host.vk_handle(0, ScriptedHost::vk_buffer + side); break;
        case 6u: case 10u: host.vk_memory_requirements(ScriptedHost::vk_importable_types); break;
        case 7u: case 11u: host.blob_import(0u, ScriptedHost::vk_memory + side); break;
        case 13u: host.vk_handle(0, ScriptedHost::vk_command_pool); break;
        case 14u: host.vk_handle(0, ScriptedHost::vk_command_buffer); break;
        case 15u: host.vk_handle(0, ScriptedHost::vk_queue); break;
        case 16u: host.vk_handle(0, ScriptedHost::vk_fence); break;
        default: host.vk_result(0); break;
    }
}

void refuse(ScriptedHost &host, std::uint32_t index)
{
    switch (index) {
        case 0u: case 3u: host.vk_handle(vk_error_incompatible_driver, 0u); break;
        case 1u: host.vk_count(vk_error_incompatible_driver, 0u); break;
        case 2u: host.vk_physical_devices(vk_error_incompatible_driver, 0u); break;
        case 4u: host.vk_memory_properties(0u, 0u); break;
        case 6u: case 10u: host.vk_memory_requirements(0u); break;
        case 7u: case 11u: host.blob_import(BlobImportAnswer::executor_refused, 0u); break;
        case 8u: case 12u: host.vk_result(vk_error_out_of_device_memory); break;
        case 15u: host.vk_handle(0, 0u); break;
        default: host.vk_handle(vk_error_out_of_device_memory, 0u); break;
    }
}

void answer_until(Rig &rig, std::uint32_t answers)
{
    for (std::uint32_t index = 0u; index < answers; index++) answer(rig.host(), index);
    rig.kmt().answer_waits(nullptr, 0u);
}

}

TEST(host_visible_import_scenario, the_host_gpu_copies_guest_written_pages_into_guest_read_ones)
{ Rig rig;
  rig.host().host_visible_import_run();
  rig.gpu().copy_at_wait(ScriptedHost::host_visible_import_copy_wait);
  av::MockPlatform report_memory;
  GuestReport report(report_memory);
  rig.run(report);
  char expected[2048] = {};
  static_cast<void>(std::snprintf(
      expected, sizeof expected,
      "ok import.adapter\nok import.connection\nok import.initialize\nok import.instance\n"
      "ok import.physical_device 2\nok import.device\nok import.memory_types 4\n"
      "ok import.src_buffer\nok import.src_chunk %u\nok import.src_import 1\nok import.src_bind_buffer\n"
      "ok import.dst_buffer\nok import.dst_chunk %u\nok import.dst_import 1\nok import.dst_bind_buffer\n"
      "ok import.command_pool\nok import.command_buffer\nok import.record\nok import.queue\n"
      "ok import.fence\nok import.submit\nok import.wait 1\nok import.copied %u\n"
      "ok import.guest_saw_host_write 0\nok import.release\n",
      MockKmt::first_window_resource, MockKmt::first_window_resource + 1u, HostVisibleImportScenario::copy_bytes));
  CHECK(std::strcmp(report.text(), expected) == 0);
  CHECK_EQ(report.failed(), 0u);
  CHECK_EQ(rig.gpu().copies(), 1u);
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::wait_for_context), ScriptedHost::host_visible_import_waits);
  CHECK_EQ(rig.sent(CommandHeader::import_blob_id), 2u);
  CHECK_EQ(rig.sent(CommandHeader::release_blob_id), 2u);
  CHECK_EQ(rig.sent(kCmd_vkCmdCopyBuffer), 1u);
  CHECK_EQ(rig.sent(kCmd_vkFreeCommandBuffers), 1u);
  CHECK_EQ(rig.sent(kCmd_vkDestroyFence), 1u);
  CHECK_EQ(rig.sent(kCmd_vkDestroyCommandPool), 1u);
  CHECK_EQ(rig.sent(kCmd_vkDestroyBuffer), 2u);
  CHECK_EQ(rig.sent(kCmd_vkDestroyDevice), 1u);
  CHECK_EQ(rig.sent(kCmd_vkDestroyInstance), 1u);
  CHECK_EQ(rig.kmt().command_id(rig.kmt().commands() - 1u), CommandHeader::nop_id);
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::escape), 6u);
  CHECK(report.invariants_hold());
  rig.check_nothing_left_but_the_adapter(); }

TEST(host_visible_import_scenario, every_import_is_released_before_its_chunk_is_closed)
{ Rig rig;
  rig.host().host_visible_import_run();
  struct Seen {
      MockKmt      *kmt;
      std::uint32_t escapes;
  } seen = {&rig.kmt(), 0u};
  rig.kmt().play_at_wait(ScriptedHost::host_visible_import_waits - 1u, [](void *context) {
      Seen &at_release = *static_cast<Seen *>(context);
      at_release.escapes = at_release.kmt->calls(MockKmt::Call::escape);
  }, &seen);
  av::MockPlatform report_memory;
  GuestReport report(report_memory);
  rig.run(report);
  CHECK_EQ(seen.escapes, 4u);
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::escape), 6u);
  const std::uint32_t commands = rig.kmt().commands();
  CHECK_EQ(rig.kmt().command_id(commands - 5u), CommandHeader::release_blob_id);
  CHECK_EQ(rig.kmt().command_id(commands - 4u), CommandHeader::release_blob_id);
  CHECK_EQ(rig.kmt().command_id(commands - 3u), static_cast<std::uint32_t>(kCmd_vkDestroyDevice));
  CHECK_EQ(rig.kmt().command_id(commands - 2u), static_cast<std::uint32_t>(kCmd_vkDestroyInstance));
  rig.check_nothing_left_but_the_adapter(); }

TEST(host_visible_import_scenario, what_crosses_names_the_chunk_the_range_and_the_type)
{ Rig rig;
  rig.host().host_visible_import_run();
  rig.gpu().copy_at_wait(ScriptedHost::host_visible_import_copy_wait);
  av::MockPlatform report_memory;
  GuestReport report(report_memory);
  rig.run(report);
  CHECK_EQ(report.failed(), 0u);
  std::uint8_t  block[512] = {};
  av::WireArena arena(block, sizeof block);
  std::uint32_t imports = 0u;
  std::uint32_t buffers = 0u;
  for (std::uint32_t index = 0u; index < rig.kmt().commands(); index++) {
      const std::uint32_t       id = rig.kmt().command_id(index);
      const std::uint8_t *const body = rig.kmt().command_bytes(index) + CommandHeader::encoded_size;
      const std::size_t         room = MockKmt::journal_bytes - CommandHeader::encoded_size;
      av::WireReader            reader(body, room);
      if (id == CommandHeader::import_blob_id) {
          BlobImportRequest request;
          CHECK_STATUS(request.decode(body, BlobImportRequest::encoded_size), Status::ok);
          CHECK_EQ(request.resource_id(), MockKmt::first_window_resource + imports);
          imports++;
      }
      if (id == kCmd_vkCreateBuffer) {
          CHECK_EQ(reader.read_u64(), ScriptedHost::vk_device);
          VkBufferCreateInfo buffer = {};
          CHECK(deserialize_VkBufferCreateInfo(reader, &buffer, arena));
          CHECK_EQ(buffer.size, HostVisibleImportScenario::copy_bytes);
          CHECK_EQ(buffer.usage, static_cast<VkBufferUsageFlags>(buffers == 0u ? VK_BUFFER_USAGE_TRANSFER_SRC_BIT
                                                                               : VK_BUFFER_USAGE_TRANSFER_DST_BIT));
          CHECK(buffer.pNext == nullptr);
          buffers++;
      }
      if (id == kCmd_vkCmdCopyBuffer) {
          CHECK_EQ(reader.read_u64(), ScriptedHost::vk_command_buffer);
          CHECK_EQ(reader.read_u64(), ScriptedHost::vk_buffer);
          CHECK_EQ(reader.read_u64(), ScriptedHost::vk_buffer + 1u);
          CHECK_EQ(reader.read_u32(), 1u);
          VkBufferCopy region = {};
          CHECK(deserialize_VkBufferCopy(reader, &region, arena));
          CHECK(region.srcOffset == 0u && region.dstOffset == 0u);
          CHECK_EQ(region.size, HostVisibleImportScenario::copy_bytes);
      }
      if (id == kCmd_vkWaitForFences) {
          CHECK_EQ(reader.read_u64(), ScriptedHost::vk_device);
          CHECK_EQ(reader.read_u32(), 1u);
          CHECK_EQ(reader.read_u64(), ScriptedHost::vk_fence);
          CHECK_EQ(reader.read_u32(), static_cast<std::uint32_t>(VK_TRUE));
          CHECK_EQ(reader.read_u64(), HostVisibleImportScenario::fence_slice_ns);
      }
  }
  CHECK_EQ(imports, 2u);
  CHECK_EQ(buffers, 2u);
  CHECK(rig.kmt().last_window_request().kind() == av::HostMemoryRequest::Kind::vulkan);
  CHECK_EQ(rig.kmt().last_window_request().vulkan_type(), 1u);
  rig.check_nothing_left_but_the_adapter(); }

TEST(host_visible_import_scenario, an_import_no_type_takes_fails_the_step)
{ struct Case { std::uint32_t script; const char *line; };
  const Case cases[] = {
      {0u, "FAIL import.src_import status=1 3\n"},
      {1u, "FAIL import.src_import status=6 0\n"},
      {2u, "FAIL import.src_import status=9 0\n"},
      {3u, "FAIL import.src_chunk status=3 code=0x00000000\n"},
      {4u, "FAIL import.src_chunk status=3 code=0x00000000\n"},
  };
  for (const Case &one : cases) {
      Rig rig;
      for (std::uint32_t index = 0u; index < 4u; index++) answer(rig.host(), index);
      rig.host().vk_memory_properties(ScriptedHost::vk_host_memory_types,
                                      one.script == 3u ? 0u : ScriptedHost::vk_host_visible_types);
      rig.host().vk_handle(0, ScriptedHost::vk_buffer);
      rig.host().vk_memory_requirements(one.script == 4u ? 0u : ScriptedHost::vk_importable_types);
      if (one.script == 0u) rig.host().blob_import(BlobImportAnswer::executor_refused, 0u);
      if (one.script == 1u) rig.host().nop();
      if (one.script == 2u) {
          rig.host().cut_next(BlobImportAnswer::encoded_size - 1u);
          rig.host().blob_import(0u, ScriptedHost::vk_memory);
      }
      rig.host().nop();
      av::MockPlatform report_memory;
      GuestReport report(report_memory);
      rig.run(report);
      CHECK(has_line(report, one.line));
      CHECK(!has_line(report, "import.src_bind_buffer"));
      CHECK(!has_line(report, "import.dst_"));
      CHECK(!has_line(report, "import.release"));
      CHECK_EQ(report.failed(), 1u);
      CHECK_EQ(rig.sent(CommandHeader::import_blob_id), one.script >= 3u ? 0u : 1u);
      CHECK_EQ(rig.sent(CommandHeader::release_blob_id), 0u);
      CHECK_EQ(rig.sent(kCmd_vkDestroyBuffer), 2u);
      rig.check_nothing_left_but_the_adapter();
  } }

TEST(host_visible_import_scenario, a_host_refusal_at_each_step_is_reported_and_everything_released)
{ struct Case { std::uint32_t index; const char *line; const char *skipped; std::uint32_t releases; };
  const Case cases[] = {
      {0u, "FAIL import.instance status=7 4294967287\n", "import.physical_device", 0u},
      {1u, "FAIL import.physical_device status=7 4294967287\n", "import.device", 0u},
      {2u, "FAIL import.physical_device status=7 4294967287\n", "import.device", 0u},
      {3u, "FAIL import.device status=7 4294967287\n", "import.memory_types", 0u},
      {4u, "FAIL import.memory_types status=3 0\n", "import.src_chunk", 0u},
      {5u, "FAIL import.src_buffer status=7 4294967294\n", "import.src_import", 0u},
      {6u, "FAIL import.src_chunk status=3 code=0x00000000\n", "import.src_import", 0u},
      {7u, "FAIL import.src_import status=1 3\n", "import.src_bind_buffer", 0u},
      {8u, "FAIL import.src_bind_buffer status=7 4294967294\n", "import.dst_chunk", 1u},
      {9u, "FAIL import.dst_buffer status=7 4294967294\n", "import.dst_import", 1u},
      {10u, "FAIL import.dst_chunk status=3 code=0x00000000\n", "import.dst_import", 1u},
      {11u, "FAIL import.dst_import status=1 3\n", "import.dst_bind_buffer", 1u},
      {12u, "FAIL import.dst_bind_buffer status=7 4294967294\n", "import.command_pool", 2u},
      {13u, "FAIL import.command_pool status=7 4294967294\n", "import.command_buffer", 2u},
      {14u, "FAIL import.command_buffer status=7 4294967294\n", "import.record", 2u},
      {15u, "FAIL import.queue status=3 code=0x00000000\n", "import.fence", 2u},
      {16u, "FAIL import.fence status=7 4294967294\n", "import.submit", 2u},
  };
  for (const Case &one : cases) {
      Rig rig;
      for (std::uint32_t index = 0u; index < one.index; index++) answer(rig.host(), index);
      refuse(rig.host(), one.index);
      rig.kmt().answer_waits(nullptr, 0u);
      av::MockPlatform report_memory;
      GuestReport report(report_memory);
      rig.run(report);
      CHECK(has_line(report, one.line));
      CHECK(!has_line(report, one.skipped));
      CHECK(!has_line(report, "import.release"));
      CHECK_EQ(report.failed(), 1u);
      CHECK_EQ(rig.sent(CommandHeader::release_blob_id), one.releases);
      CHECK_EQ(rig.sent(kCmd_vkFreeCommandBuffers), one.index > 14u ? 1u : 0u);
      CHECK_EQ(rig.sent(kCmd_vkDestroyDevice), one.index > 3u ? 1u : 0u);
      CHECK_EQ(rig.sent(kCmd_vkDestroyInstance), one.index > 0u ? 1u : 0u);
      CHECK_EQ(rig.kmt().command_id(rig.kmt().commands() - 1u), CommandHeader::nop_id);
      rig.check_nothing_left_but_the_adapter();
  } }

TEST(host_visible_import_scenario, a_driver_or_transport_failure_at_each_step_is_reported_and_released)
{ enum class Fault { fail, zero_escape, short_pages };
  struct Case {
      Fault         fault;
      MockKmt::Call call;
      std::uint32_t nth;
      std::uint32_t answers;
      const char   *line;
      const char   *skipped;
  };
  const Case cases[] = {
      {Fault::fail, MockKmt::Call::escape, 3u, 7u, "FAIL import.src_chunk status=7 code=0xC0000001\n",
       "import.src_import"},
      {Fault::zero_escape, MockKmt::Call::escape, 0u, 7u, "FAIL import.src_chunk status=9 code=0x00000000\n",
       "import.src_import"},
      {Fault::short_pages, MockKmt::Call::escape, 0u, 7u, "FAIL import.src_chunk status=9 code=0x00000000\n",
       "import.src_import"},
      {Fault::fail, MockKmt::Call::escape, 4u, 11u, "FAIL import.dst_chunk status=7 code=0xC0000001\n",
       "import.dst_import"},
      {Fault::fail, MockKmt::Call::wait_for_context, 8u, 7u, "FAIL import.src_import status=7 0\n",
       "import.src_bind_buffer"},
      {Fault::fail, MockKmt::Call::submit, 16u, 15u, "FAIL import.record status=7 4294967292\n", "import.queue"},
      {Fault::fail, MockKmt::Call::submit, 18u, 15u, "FAIL import.record status=7 4294967292\n", "import.queue"},
      {Fault::fail, MockKmt::Call::submit, 21u, 17u, "FAIL import.submit status=7 4294967292\n", "import.wait"},
  };
  for (const Case &one : cases) {
      Rig rig;
      answer_until(rig, one.answers);
      if (one.fault == Fault::fail) rig.kmt().fail_at(one.call, one.nth, refused);
      if (one.fault == Fault::zero_escape) rig.kmt().zero_at(MockKmt::Call::escape, 3u);
      if (one.fault == Fault::short_pages) rig.kmt().answer_next_chunk_bytes((chunk_pages - 1u) * MockKmt::page_bytes);
      av::MockPlatform report_memory;
      GuestReport report(report_memory);
      rig.run(report);
      CHECK(has_line(report, one.line));
      CHECK(!has_line(report, one.skipped));
      CHECK(!has_line(report, "import.release"));
      CHECK_EQ(report.failed(), 1u);
      rig.check_nothing_left_but_the_adapter();
  } }

TEST(host_visible_import_scenario, a_recording_lost_at_its_begin_or_its_end_is_told_apart_by_the_copy_sent)
{ const std::uint32_t lost_at[] = {16u, 18u};
  for (const std::uint32_t nth : lost_at) {
      Rig rig;
      answer_until(rig, 15u);
      rig.kmt().fail_at(MockKmt::Call::submit, nth, refused);
      av::MockPlatform report_memory;
      GuestReport report(report_memory);
      rig.run(report);
      CHECK_EQ(rig.sent(kCmd_vkBeginCommandBuffer), nth == 16u ? 0u : 1u);
      CHECK_EQ(rig.sent(kCmd_vkCmdCopyBuffer), nth == 16u ? 0u : 1u);
      CHECK_EQ(rig.sent(kCmd_vkEndCommandBuffer), 0u);
      rig.check_nothing_left_but_the_adapter();
  } }

TEST(host_visible_import_scenario, a_context_that_cannot_be_set_up_sends_nothing)
{ Rig rig;
  rig.kmt().fail_at(MockKmt::Call::create_context_virtual, 1u, refused);
  av::MockPlatform report_memory;
  GuestReport report(report_memory);
  rig.run(report);
  CHECK(has_line(report, "FAIL import.initialize status=7"));
  CHECK(!has_line(report, "import.instance"));
  CHECK_EQ(rig.kmt().commands(), 0u);
  rig.check_nothing_left_but_the_adapter(); }

TEST(host_visible_import_scenario, a_copy_that_leaves_bytes_wrong_reports_how_many_matched)
{ Rig rig;
  HostGpu spoiling(rig.kmt(), 5u);
  rig.host().host_visible_import_run();
  spoiling.copy_at_wait(ScriptedHost::host_visible_import_copy_wait);
  av::MockPlatform report_memory;
  GuestReport report(report_memory);
  rig.run(report);
  CHECK(has_line(report, "ok import.wait 1\n"));
  CHECK(has_line(report, "FAIL import.copied status=9 1048571\n"));
  CHECK(has_line(report, "FAIL import.guest_saw_host_write status=9 0\n"));
  CHECK(!has_line(report, "import.release"));
  CHECK_EQ(report.failed(), 2u);
  CHECK_EQ(spoiling.copies(), 1u);
  CHECK_EQ(rig.sent(CommandHeader::release_blob_id), 2u);
  rig.check_nothing_left_but_the_adapter(); }

TEST(host_visible_import_scenario, a_host_that_never_copies_matches_nothing)
{ Rig rig;
  rig.host().host_visible_import_run();
  av::MockPlatform report_memory;
  GuestReport report(report_memory);
  rig.run(report);
  CHECK(has_line(report, "FAIL import.copied status=9 0\n"));
  CHECK_EQ(report.failed(), 2u);
  rig.check_nothing_left_but_the_adapter(); }

TEST(host_visible_import_scenario, a_destination_written_before_the_submit_fails_guest_saw_host_write)
{ Rig rig;
  rig.host().host_visible_import_run();
  rig.gpu().copy_at_wait(command_buffer_wait);
  av::MockPlatform report_memory;
  GuestReport report(report_memory);
  rig.run(report);
  CHECK(has_line(report, "ok import.copied 1048576\n"));
  CHECK(has_line(report, "FAIL import.guest_saw_host_write status=9 1048576\n"));
  CHECK_EQ(report.failed(), 1u);
  rig.check_nothing_left_but_the_adapter(); }

TEST(host_visible_import_scenario, a_fence_that_comes_on_a_later_slice_passes)
{ Rig rig;
  answer_until(rig, 17u);
  rig.host().vk_result(vk_timeout);
  rig.host().vk_result(vk_timeout);
  rig.host().vk_result(0);
  rig.gpu().copy_at_wait(ScriptedHost::host_visible_import_copy_wait + 2u);
  av::MockPlatform report_memory;
  GuestReport report(report_memory);
  rig.run(report);
  CHECK(has_line(report, "ok import.wait 3\n"));
  CHECK(has_line(report, "ok import.copied 1048576\n"));
  CHECK(has_line(report, "ok import.release\n"));
  CHECK_EQ(report.failed(), 0u);
  CHECK_EQ(rig.sent(kCmd_vkWaitForFences), 3u);
  rig.check_nothing_left_but_the_adapter(); }

TEST(host_visible_import_scenario, a_fence_wait_that_times_out_ten_times_fails_copied)
{ Rig rig;
  answer_until(rig, 17u);
  for (std::uint32_t slice = 0u; slice < HostVisibleImportScenario::fence_slices; slice++)
      rig.host().vk_result(vk_timeout);
  rig.gpu().copy_at_wait(ScriptedHost::host_visible_import_copy_wait);
  av::MockPlatform report_memory;
  GuestReport report(report_memory);
  rig.run(report);
  CHECK(has_line(report, "FAIL import.wait status=7 10\n"));
  CHECK(has_line(report, "FAIL import.copied status=7 1048576\n"));
  CHECK(has_line(report, "FAIL import.guest_saw_host_write status=7 0\n"));
  CHECK_EQ(report.failed(), 3u);
  CHECK_EQ(rig.sent(kCmd_vkWaitForFences), HostVisibleImportScenario::fence_slices);
  rig.check_nothing_left_but_the_adapter(); }

TEST(host_visible_import_scenario, a_lost_device_fails_the_wait_on_its_first_slice)
{ Rig rig;
  answer_until(rig, 17u);
  rig.host().vk_result(vk_error_device_lost);
  av::MockPlatform report_memory;
  GuestReport report(report_memory);
  rig.run(report);
  CHECK(has_line(report, "FAIL import.wait status=7 1\n"));
  CHECK(has_line(report, "FAIL import.copied status=7 0\n"));
  CHECK_EQ(rig.sent(kCmd_vkWaitForFences), 1u);
  rig.check_nothing_left_but_the_adapter(); }

TEST(host_visible_import_scenario, a_release_that_fails_is_reported_and_everything_still_let_go)
{ struct Case { std::uint32_t fault; const char *line; };
  const Case cases[] = {{0u, "FAIL import.release status=9 code=0x00000000\n"},
                        {1u, "FAIL import.release status=7 code=0x00000000\n"},
                        {2u, "FAIL import.release status=9 code=0x00000000\n"}};
  for (const Case &one : cases) {
      Rig rig;
      rig.host().host_visible_import_device();
      rig.host().host_visible_import_side(0u);
      rig.host().host_visible_import_side(1u);
      rig.host().host_visible_import_copy();
      if (one.fault == 0u) rig.host().vk_result(0);
      else rig.host().nop();
      rig.host().nop();
      if (one.fault == 2u) rig.host().vk_result(0);
      else rig.host().nop();
      if (one.fault == 1u) rig.kmt().fail_at(MockKmt::Call::escape, 5u, refused);
      rig.gpu().copy_at_wait(ScriptedHost::host_visible_import_copy_wait);
      av::MockPlatform report_memory;
      GuestReport report(report_memory);
      rig.run(report);
      CHECK(has_line(report, "ok import.guest_saw_host_write 0\n"));
      CHECK(has_line(report, one.line));
      CHECK_EQ(report.failed(), 1u);
      CHECK_EQ(rig.sent(CommandHeader::release_blob_id), 2u);
      CHECK_EQ(rig.kmt().calls(MockKmt::Call::escape), 6u);
      CHECK_EQ(rig.kmt().open_objects(), 1u);
      CHECK_EQ(rig.kmt().queued_answers(), 0u);
      CHECK_EQ(rig.kmt().misuses(), 0u);
      CHECK_EQ(rig.kmt().window_chunks(), 0u);
      CHECK_STATUS(rig.search().close(), Status::ok);
  } }
