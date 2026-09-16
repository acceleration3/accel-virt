/* The live experiment of feature 045 over MockKmt, the test playing the host
 * through ScriptedHost and the host GPU's copy through a callback at the fence
 * wait, which copies between the two locked chunks' views: the whole run
 * passes with the pattern read back; a failure at every step is reported with
 * the later steps skipped and everything made released; the memory type is
 * searched lowest first until the host takes one; a copy that leaves bytes
 * wrong says how many matched; and a fence that never comes fails the copy. */
#include "core/support/status.hpp"
#include "guest/kmt/pci_identity.hpp"
#include "guest/suite/adapter_scenario.hpp"
#include "guest/suite/adapter_search.hpp"
#include "guest/suite/guest_report.hpp"
#include "guest/suite/host_visible_import_scenario.hpp"
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

const PciIdentity rvk(AdapterScenario::virtio_vendor, AdapterScenario::virtio_gpu_device,
                      AdapterScenario::virtio_vendor, AdapterScenario::rvk_subsystem);
constexpr std::int32_t  vk_timeout = 2;
constexpr std::int32_t  vk_error_out_of_device_memory = -2;
constexpr std::int32_t  vk_error_device_lost = -4;
constexpr std::int32_t  vk_error_incompatible_driver = -9;
constexpr std::uint32_t refused = 0xC0000001u;
constexpr std::uint32_t chunk_pages = static_cast<std::uint32_t>(HostVisibleImportScenario::chunk_bytes / 4096u);
/* The wait for the command buffer's allocation: after the guest cleared the
 * destination, before it reads it ahead of the submit. */
constexpr std::uint32_t command_buffer_wait = 15u;

bool has_line(const GuestReport &report, const char *line)
{
    return std::strstr(report.text(), line) != nullptr;
}

/* The host GPU's vkCmdCopyBuffer, played on the two chunks' views in the order
 * they were bound, spoiling the last `spoiled` bytes it wrote. */
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
        const std::uint8_t *const source = kmt_.view_of(kmt_.bound_allocation(0u));
        std::uint8_t *const       destination = kmt_.view_of(kmt_.bound_allocation(1u));
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
        kmt_.add_adapter(rvk, AdapterScenario::wddm_3_2);
        CHECK_STATUS(search_.find(rvk), Status::ok);
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

    /* Only the adapter the search owns, every chunk unbound, every answer taken. */
    void check_nothing_left_but_the_adapter()
    {
        CHECK_EQ(kmt_.open_objects(), 1u);
        CHECK_EQ(kmt_.misuses(), 0u);
        CHECK_EQ(kmt_.bindings(), 0u);
        CHECK_EQ(kmt_.queued_answers(), 0u);
        CHECK_STATUS(search_.close(), Status::ok);
    }

    /* How many commands of `id` the scenario sent. */
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

/* The answer a passing run's wait at `index` (0-based) finds. */
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
        default: host.vk_result(0); break;   /* the binds and the fence wait */
    }
}

/* The same wait answered as a host that refuses it. */
void refuse(ScriptedHost &host, std::uint32_t index)
{
    switch (index) {
        case 0u: case 3u: host.vk_handle(vk_error_incompatible_driver, 0u); break;
        case 1u: host.vk_count(vk_error_incompatible_driver, 0u); break;
        case 2u: host.vk_physical_devices(vk_error_incompatible_driver, 0u); break;
        case 4u: host.vk_memory_properties(0u, 0u); break;
        case 6u: case 10u: host.vk_memory_requirements(0u); break;
        case 7u: case 11u:   /* each host-visible type the buffer can live in */
            host.blob_import(BlobImportAnswer::executor_refused, 0u);
            host.blob_import(BlobImportAnswer::executor_refused, 0u);
            break;
        case 8u: case 12u: host.vk_result(vk_error_out_of_device_memory); break;
        case 15u: host.vk_handle(0, 0u); break;
        default: host.vk_handle(vk_error_out_of_device_memory, 0u); break;
    }
}

/* A passing run's first `answers`, and every wait after them answered empty -
 * what the teardown's releases and no-op wait for. */
void answer_until(Rig &rig, std::uint32_t answers)
{
    for (std::uint32_t index = 0u; index < answers; index++) answer(rig.host(), index);
    rig.kmt().answer_waits(nullptr, 0u);
}

}  // namespace

TEST(host_visible_import_scenario, the_host_gpu_copies_guest_written_pages_into_guest_read_ones)
{ Rig rig;
  rig.host().host_visible_import_run();
  rig.gpu().copy_at_wait(ScriptedHost::host_visible_import_copy_wait);
  GuestReport report;
  rig.run(report);
  char expected[2048] = {};
  static_cast<void>(std::snprintf(
      expected, sizeof expected,
      "ok import.adapter\nok import.connection\nok import.initialize\nok import.instance\n"
      "ok import.physical_device 2\nok import.device\nok import.memory_types 4\n"
      "ok import.src_chunk\nok import.src_lock\nok import.src_bind %u\nok import.src_pages %u\n"
      "ok import.src_buffer\nok import.src_import 1\nok import.src_bind_buffer\n"
      "ok import.dst_chunk\nok import.dst_lock\nok import.dst_bind %u\nok import.dst_pages %u\n"
      "ok import.dst_buffer\nok import.dst_import 1\nok import.dst_bind_buffer\n"
      "ok import.command_pool\nok import.command_buffer\nok import.record\nok import.queue\n"
      "ok import.fence\nok import.submit\nok import.wait 1\nok import.copied %u\n"
      "ok import.guest_saw_host_write 0\nok import.release\n",
      MockKmt::first_chunk_resource, chunk_pages, MockKmt::first_chunk_resource + 1u, chunk_pages,
      HostVisibleImportScenario::copy_bytes));
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
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::escape), 4u);   /* two binds, two unbinds */
  CHECK(report.invariants_hold());
  rig.check_nothing_left_but_the_adapter(); }

/* Every import is released before any chunk is unbound - the device refuses to
 * unreference a blob an import still holds - and before the device it was
 * made on is destroyed. */
TEST(host_visible_import_scenario, every_import_is_released_before_its_chunk_is_unbound)
{ Rig rig;
  rig.host().host_visible_import_run();
  struct Seen {
      MockKmt      *kmt;
      std::uint32_t escapes;
  } seen = {&rig.kmt(), 0u};
  /* The second release's wait, the run's twentieth. */
  rig.kmt().play_at_wait(ScriptedHost::host_visible_import_waits - 1u, [](void *context) {
      Seen &at_release = *static_cast<Seen *>(context);
      at_release.escapes = at_release.kmt->calls(MockKmt::Call::escape);
  }, &seen);
  GuestReport report;
  rig.run(report);
  CHECK_EQ(seen.escapes, 2u);   /* the binds only */
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::escape), 4u);
  const std::uint32_t commands = rig.kmt().commands();
  /* Then the device the imports were made on, and its instance. */
  CHECK_EQ(rig.kmt().command_id(commands - 5u), CommandHeader::release_blob_id);
  CHECK_EQ(rig.kmt().command_id(commands - 4u), CommandHeader::release_blob_id);
  CHECK_EQ(rig.kmt().command_id(commands - 3u), static_cast<std::uint32_t>(kCmd_vkDestroyDevice));
  CHECK_EQ(rig.kmt().command_id(commands - 2u), static_cast<std::uint32_t>(kCmd_vkDestroyInstance));
  rig.check_nothing_left_but_the_adapter(); }

/* What crossed is what the host needs: each import names its chunk's resource,
 * the start of it, the copy's whole pages and the host-visible type; each
 * buffer is the copy's size, source then destination, made to be bound to
 * host memory; the copy is the whole range; the fence wait is a slice. */
TEST(host_visible_import_scenario, what_crosses_names_the_chunk_the_range_and_the_type)
{ Rig rig;
  rig.host().host_visible_import_run();
  rig.gpu().copy_at_wait(ScriptedHost::host_visible_import_copy_wait);
  GuestReport report;
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
          CHECK_EQ(request.resource_id(), MockKmt::first_chunk_resource + imports);
          CHECK_EQ(request.offset(), 0u);
          CHECK_EQ(request.size(), HostVisibleImportScenario::copy_bytes);
          CHECK_EQ(request.memory_type(), 1u);
          imports++;
      }
      if (id == kCmd_vkCreateBuffer) {
          CHECK_EQ(reader.read_u64(), ScriptedHost::vk_device);
          VkBufferCreateInfo buffer = {};
          CHECK(deserialize_VkBufferCreateInfo(reader, &buffer, arena));
          CHECK_EQ(buffer.size, HostVisibleImportScenario::copy_bytes);
          CHECK_EQ(buffer.usage, static_cast<VkBufferUsageFlags>(buffers == 0u ? VK_BUFFER_USAGE_TRANSFER_SRC_BIT
                                                                               : VK_BUFFER_USAGE_TRANSFER_DST_BIT));
          const auto *const external = static_cast<const VkExternalMemoryBufferCreateInfo *>(buffer.pNext);
          CHECK(external != nullptr && external->sType == VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO &&
                external->handleTypes == VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT);
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
  rig.check_nothing_left_but_the_adapter(); }

/* A host that cannot import into the first host-visible type the buffer can
 * live in refuses it, and the next is taken: the type is on the line. */
TEST(host_visible_import_scenario, a_refused_type_is_passed_over_for_the_next_host_visible_one)
{ Rig rig;
  rig.host().host_visible_import_device();
  rig.host().vk_handle(0, ScriptedHost::vk_buffer);
  rig.host().vk_memory_requirements(ScriptedHost::vk_importable_types);
  rig.host().blob_import(BlobImportAnswer::executor_refused, 0u);
  rig.host().blob_import(0u, ScriptedHost::vk_memory);
  rig.host().vk_result(0);
  rig.host().host_visible_import_side(1u);
  rig.host().host_visible_import_copy();
  rig.host().host_visible_import_releases();
  rig.gpu().copy_at_wait(ScriptedHost::host_visible_import_copy_wait + 1u);
  GuestReport report;
  rig.run(report);
  CHECK(has_line(report, "ok import.src_import 2\n"));
  CHECK(has_line(report, "ok import.dst_import 1\n"));
  CHECK_EQ(report.failed(), 0u);
  CHECK_EQ(rig.sent(CommandHeader::import_blob_id), 3u);
  CHECK_EQ(rig.sent(CommandHeader::release_blob_id), 2u);   /* a refusal holds nothing to release */
  rig.check_nothing_left_but_the_adapter(); }

/* The search ends when the host refuses every type, answers what the contract
 * does not allow, or there is no type to try; the line says with what. */
TEST(host_visible_import_scenario, an_import_no_type_takes_fails_the_step)
{ struct Case { std::uint32_t script; const char *line; };
  /* script: 0 every host-visible type refused, 1 an empty answer, 2 a cut
   * answer, 3 no host-visible type, 4 a buffer that lives nowhere */
  const Case cases[] = {
      {0u, "FAIL import.src_import status=1 5\n"},
      {1u, "FAIL import.src_import status=6 0\n"},
      {2u, "FAIL import.src_import status=9 0\n"},
      {3u, "FAIL import.src_import status=3 0\n"},
      {4u, "FAIL import.src_import status=3 0\n"},
  };
  for (const Case &one : cases) {
      Rig rig;
      for (std::uint32_t index = 0u; index < 4u; index++) answer(rig.host(), index);
      rig.host().vk_memory_properties(ScriptedHost::vk_host_memory_types,
                                      one.script == 3u ? 0u : ScriptedHost::vk_host_visible_types);
      rig.host().vk_handle(0, ScriptedHost::vk_buffer);
      rig.host().vk_memory_requirements(one.script == 4u ? 0u : ScriptedHost::vk_importable_types);
      if (one.script == 0u) {
          rig.host().blob_import(BlobImportAnswer::unmappable_pages, 0u);
          rig.host().blob_import(BlobImportAnswer::executor_refused, 0u);
      }
      if (one.script == 1u) rig.host().nop();
      if (one.script == 2u) {
          rig.host().cut_next(BlobImportAnswer::encoded_size - 1u);
          rig.host().blob_import(0u, ScriptedHost::vk_memory);
      }
      rig.host().nop();
      GuestReport report;
      rig.run(report);
      CHECK(has_line(report, one.line));
      CHECK(!has_line(report, "import.src_bind_buffer"));
      CHECK(!has_line(report, "import.dst_"));
      CHECK(!has_line(report, "import.release"));
      CHECK_EQ(report.failed(), 1u);
      CHECK_EQ(rig.sent(CommandHeader::import_blob_id), one.script == 0u ? 2u : one.script >= 3u ? 0u : 1u);
      CHECK_EQ(rig.sent(CommandHeader::release_blob_id), 0u);
      CHECK_EQ(rig.sent(kCmd_vkDestroyBuffer), 2u);   /* the source's, and the destination's null */
      rig.check_nothing_left_but_the_adapter();
  } }

/* Each step the host answers, refused: the line, nothing after it, and
 * everything made released - device and instance, buffers, imports and chunks. */
TEST(host_visible_import_scenario, a_host_refusal_at_each_step_is_reported_and_everything_released)
{ struct Case { std::uint32_t index; const char *line; const char *skipped; std::uint32_t releases; };
  const Case cases[] = {
      {0u, "FAIL import.instance status=7 4294967287\n", "import.physical_device", 0u},
      {1u, "FAIL import.physical_device status=7 4294967287\n", "import.device", 0u},
      {2u, "FAIL import.physical_device status=7 4294967287\n", "import.device", 0u},
      {3u, "FAIL import.device status=7 4294967287\n", "import.memory_types", 0u},
      {4u, "FAIL import.memory_types status=3 0\n", "import.src_chunk", 0u},
      {5u, "FAIL import.src_buffer status=7 4294967294\n", "import.src_import", 0u},
      {6u, "FAIL import.src_import status=3 0\n", "import.src_bind_buffer", 0u},
      {7u, "FAIL import.src_import status=1 5\n", "import.src_bind_buffer", 0u},
      {8u, "FAIL import.src_bind_buffer status=7 4294967294\n", "import.dst_chunk", 1u},
      {9u, "FAIL import.dst_buffer status=7 4294967294\n", "import.dst_import", 1u},
      {10u, "FAIL import.dst_import status=3 0\n", "import.dst_bind_buffer", 1u},
      {11u, "FAIL import.dst_import status=1 5\n", "import.dst_bind_buffer", 1u},
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
      GuestReport report;
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

/* Each D3DKMT call and each misanswer of the driver's, and a transport the
 * device lost at each kind of send: the line, nothing after it, and nothing
 * left open but the adapter. */
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
      {Fault::fail, MockKmt::Call::create_allocation, 5u, 5u, "FAIL import.src_chunk status=7 code=0xC0000001\n",
       "import.src_lock"},
      {Fault::fail, MockKmt::Call::make_resident, 5u, 5u, "FAIL import.src_chunk status=7 code=0xC0000001\n",
       "import.src_lock"},
      {Fault::fail, MockKmt::Call::wait_for_fence, 6u, 5u, "FAIL import.src_chunk status=7 code=0xC0000001\n",
       "import.src_lock"},
      {Fault::fail, MockKmt::Call::lock, 3u, 5u, "FAIL import.src_lock status=7 code=0xC0000001\n",
       "import.src_bind"},
      {Fault::fail, MockKmt::Call::escape, 1u, 5u, "FAIL import.src_bind status=7 code=0xC0000001\n",
       "import.src_pages"},
      {Fault::zero_escape, MockKmt::Call::escape, 0u, 5u, "FAIL import.src_bind status=9 code=0x00000000\n",
       "import.src_pages"},
      {Fault::short_pages, MockKmt::Call::escape, 0u, 5u, "FAIL import.src_pages status=9 1\n", "import.src_buffer"},
      {Fault::fail, MockKmt::Call::create_allocation, 6u, 9u, "FAIL import.dst_chunk status=7 code=0xC0000001\n",
       "import.dst_lock"},
      {Fault::fail, MockKmt::Call::lock, 4u, 9u, "FAIL import.dst_lock status=7 code=0xC0000001\n",
       "import.dst_bind"},
      {Fault::fail, MockKmt::Call::escape, 2u, 9u, "FAIL import.dst_bind status=7 code=0xC0000001\n",
       "import.dst_pages"},
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
      if (one.fault == Fault::zero_escape) rig.kmt().zero_next(MockKmt::Call::escape);
      if (one.fault == Fault::short_pages) rig.kmt().answer_next_bind_pages(1u);
      GuestReport report;
      rig.run(report);
      CHECK(has_line(report, one.line));
      CHECK(!has_line(report, one.skipped));
      CHECK(!has_line(report, "import.release"));
      CHECK_EQ(report.failed(), 1u);
      rig.check_nothing_left_but_the_adapter();
  } }

/* The copy is recorded only once it has begun: a transport lost at the begin
 * sends no copy, one lost at the end has sent it. */
TEST(host_visible_import_scenario, a_recording_lost_at_its_begin_or_its_end_is_told_apart_by_the_copy_sent)
{ const std::uint32_t lost_at[] = {16u, 18u};
  for (const std::uint32_t nth : lost_at) {
      Rig rig;
      answer_until(rig, 15u);
      rig.kmt().fail_at(MockKmt::Call::submit, nth, refused);
      GuestReport report;
      rig.run(report);
      CHECK_EQ(rig.sent(kCmd_vkBeginCommandBuffer), nth == 16u ? 0u : 1u);
      CHECK_EQ(rig.sent(kCmd_vkCmdCopyBuffer), nth == 16u ? 0u : 1u);
      CHECK_EQ(rig.sent(kCmd_vkEndCommandBuffer), 0u);
      rig.check_nothing_left_but_the_adapter();
  } }

/* A context the transport could not set up is reported and nothing is sent. */
TEST(host_visible_import_scenario, a_context_that_cannot_be_set_up_sends_nothing)
{ Rig rig;
  rig.kmt().fail_at(MockKmt::Call::create_context_virtual, 1u, refused);
  GuestReport report;
  rig.run(report);
  CHECK(has_line(report, "FAIL import.initialize status=7"));
  CHECK(!has_line(report, "import.instance"));
  CHECK_EQ(rig.kmt().commands(), 0u);
  rig.check_nothing_left_but_the_adapter(); }

/* The host's bytes are what the guest reads back: a copy with bytes wrong says
 * how many matched, and the guest did not see the host's write. */
TEST(host_visible_import_scenario, a_copy_that_leaves_bytes_wrong_reports_how_many_matched)
{ Rig rig;
  HostGpu spoiling(rig.kmt(), 5u);
  rig.host().host_visible_import_run();
  spoiling.copy_at_wait(ScriptedHost::host_visible_import_copy_wait);
  GuestReport report;
  rig.run(report);
  CHECK(has_line(report, "ok import.wait 1\n"));
  CHECK(has_line(report, "FAIL import.copied status=9 1048571\n"));
  CHECK(has_line(report, "FAIL import.guest_saw_host_write status=9 0\n"));
  CHECK(!has_line(report, "import.release"));
  CHECK_EQ(report.failed(), 2u);
  CHECK_EQ(spoiling.copies(), 1u);
  CHECK_EQ(rig.sent(CommandHeader::release_blob_id), 2u);
  rig.check_nothing_left_but_the_adapter(); }

/* A host that never copies leaves the destination as the guest cleared it. */
TEST(host_visible_import_scenario, a_host_that_never_copies_matches_nothing)
{ Rig rig;
  rig.host().host_visible_import_run();
  GuestReport report;
  rig.run(report);
  CHECK(has_line(report, "FAIL import.copied status=9 0\n"));
  CHECK_EQ(report.failed(), 2u);
  rig.check_nothing_left_but_the_adapter(); }

/* The destination already holding the pattern before the copy was submitted
 * is no proof of the host's write, even when the bytes end right. */
TEST(host_visible_import_scenario, a_destination_written_before_the_submit_fails_guest_saw_host_write)
{ Rig rig;
  rig.host().host_visible_import_run();
  rig.gpu().copy_at_wait(command_buffer_wait);
  GuestReport report;
  rig.run(report);
  CHECK(has_line(report, "ok import.copied 1048576\n"));
  CHECK(has_line(report, "FAIL import.guest_saw_host_write status=9 1048576\n"));
  CHECK_EQ(report.failed(), 1u);
  rig.check_nothing_left_but_the_adapter(); }

/* The fence is waited in slices: one the host is slow to reach passes on the
 * slice it arrives in. */
TEST(host_visible_import_scenario, a_fence_that_comes_on_a_later_slice_passes)
{ Rig rig;
  answer_until(rig, 17u);
  rig.host().vk_result(vk_timeout);
  rig.host().vk_result(vk_timeout);
  rig.host().vk_result(0);
  rig.gpu().copy_at_wait(ScriptedHost::host_visible_import_copy_wait + 2u);
  GuestReport report;
  rig.run(report);
  CHECK(has_line(report, "ok import.wait 3\n"));
  CHECK(has_line(report, "ok import.copied 1048576\n"));
  CHECK(has_line(report, "ok import.release\n"));
  CHECK_EQ(report.failed(), 0u);
  CHECK_EQ(rig.sent(kCmd_vkWaitForFences), 3u);
  rig.check_nothing_left_but_the_adapter(); }

/* Ten slices that time out fail the wait and the copy, however the view reads:
 * a copy whose fence never came is not judged by its bytes. */
TEST(host_visible_import_scenario, a_fence_wait_that_times_out_ten_times_fails_copied)
{ Rig rig;
  answer_until(rig, 17u);
  for (std::uint32_t slice = 0u; slice < HostVisibleImportScenario::fence_slices; slice++)
      rig.host().vk_result(vk_timeout);
  rig.gpu().copy_at_wait(ScriptedHost::host_visible_import_copy_wait);
  GuestReport report;
  rig.run(report);
  CHECK(has_line(report, "FAIL import.wait status=7 10\n"));
  CHECK(has_line(report, "FAIL import.copied status=7 1048576\n"));
  CHECK(has_line(report, "FAIL import.guest_saw_host_write status=7 0\n"));
  CHECK_EQ(report.failed(), 3u);
  CHECK_EQ(rig.sent(kCmd_vkWaitForFences), HostVisibleImportScenario::fence_slices);
  rig.check_nothing_left_but_the_adapter(); }

/* A lost device ends the wait on its first slice. */
TEST(host_visible_import_scenario, a_lost_device_fails_the_wait_on_its_first_slice)
{ Rig rig;
  answer_until(rig, 17u);
  rig.host().vk_result(vk_error_device_lost);
  GuestReport report;
  rig.run(report);
  CHECK(has_line(report, "FAIL import.wait status=7 1\n"));
  CHECK(has_line(report, "FAIL import.copied status=7 0\n"));
  CHECK_EQ(rig.sent(kCmd_vkWaitForFences), 1u);
  rig.check_nothing_left_but_the_adapter(); }

/* The releases are the scenario's last word: an import released with bytes in
 * its answer, an unbind the driver refuses, or a no-op answered with bytes
 * fails release, and everything is still let go. */
TEST(host_visible_import_scenario, a_release_that_fails_is_reported_and_everything_still_let_go)
{ struct Case { std::uint32_t fault; const char *line; };
  /* fault: 0 the first release answered with bytes, 1 the first unbind refused,
   * 2 the no-op answered with bytes */
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
      if (one.fault == 1u) rig.kmt().fail_at(MockKmt::Call::escape, 3u, refused);
      rig.gpu().copy_at_wait(ScriptedHost::host_visible_import_copy_wait);
      GuestReport report;
      rig.run(report);
      CHECK(has_line(report, "ok import.guest_saw_host_write 0\n"));
      CHECK(has_line(report, one.line));
      CHECK_EQ(report.failed(), 1u);
      CHECK_EQ(rig.sent(CommandHeader::release_blob_id), 2u);
      CHECK_EQ(rig.kmt().calls(MockKmt::Call::escape), 4u);
      CHECK_EQ(rig.kmt().open_objects(), 1u);
      CHECK_EQ(rig.kmt().queued_answers(), 0u);
      CHECK_EQ(rig.kmt().misuses(), 0u);
      /* A refused unbind leaves the binding until the chunk is destroyed,
       * which unbinds it as the driver does. */
      CHECK_EQ(rig.kmt().bindings(), 0u);
      CHECK_STATUS(rig.search().close(), Status::ok);
  } }
