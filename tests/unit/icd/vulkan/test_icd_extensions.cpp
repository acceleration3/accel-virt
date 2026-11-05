#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "harness/guest/scripted_host.hpp"
#include "harness/icd/icd_rig.hpp"
#include "icd/vulkan/host_extension_list.hpp"
#include "icd/vulkan/icd_root.hpp"
#include "icd/vulkan/loader_handle.hpp"
#include "proto/generated/vulkan/vk_protocol_ids.h"
#include "proto/generated/vulkan/vk_serialize.h"
#include "proto/client/icd_handle_registry.hpp"
#include "proto/submission/command_header.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

#include <cstdint>
#include <cstring>

using av::CommandHeader;
using av::IcdRig;
using av::MockKmt;
using av::ScriptedHost;
using av::WireReader;
using av::WireWriter;
using av::icd::LoaderHandle;
using namespace av::vkproto;

namespace {

HandleIdentity identity_translation;

std::uint64_t wire_of(av::IcdRig &rig, HandleKind kind, const void *handle)
{
    return rig.root().wire_handles().wire_for(kind, reinterpret_cast<std::uint64_t>(handle));
}

using EnumerateDeviceExtensions = VkResult (*)(VkPhysicalDevice, const char *, std::uint32_t *,
                                               VkExtensionProperties *);
using CreateDevice = VkResult (*)(VkPhysicalDevice, const VkDeviceCreateInfo *, const VkAllocationCallbacks *,
                                  VkDevice *);

constexpr std::int32_t vk_error_initialization_failed = -3;

class ExtensionRig {
public:
    ExtensionRig() : instance_(rig_.create_instance()), physical_device_(rig_.physical_device(instance_)) {}
    ~ExtensionRig() { rig_.destroy_instance(instance_); }

    IcdRig          &rig() { return rig_; }
    MockKmt         &kmt() { return rig_.kmt(); }
    VkPhysicalDevice physical_device() const { return physical_device_; }

    VkResult enumerate(std::uint32_t *count, VkExtensionProperties *properties, const char *layer = nullptr)
    {
        const VkResult result = rig_.entry<EnumerateDeviceExtensions>(instance_, "vkEnumerateDeviceExtensionProperties")(
            physical_device_, layer, count, properties);
        CHECK(rig_.root().invariants_hold());
        return result;
    }

    VkResult create(const char *const *names, std::uint32_t count, VkDevice *device)
    {
        VkDeviceCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        info.enabledExtensionCount = count;
        info.ppEnabledExtensionNames = names;
        const VkResult result =
            rig_.entry<CreateDevice>(instance_, "vkCreateDevice")(physical_device_, &info, nullptr, device);
        CHECK(rig_.root().invariants_hold());
        return result;
    }

    void answer_count(std::int32_t result, std::uint32_t listed) { rig_.host().vk_count(result, listed); }

    void answer_list(std::int32_t result, std::uint32_t claimed, const char *const *names, std::uint32_t sent)
    {
        std::uint8_t answer[MockKmt::max_answer] = {};
        WireWriter   writer(answer, sizeof answer);
        writer.write_pod(result);
        writer.write_pod(claimed);
        for (std::uint32_t index = 0u; index < sent; index++) {
            VkExtensionProperties entry = {};
            std::strncpy(entry.extensionName, names[index], sizeof entry.extensionName);
            entry.specVersion = 1u;
            serialize_VkExtensionProperties(writer, entry);
        }
        CHECK_STATUS(writer.status(), av::Status::ok);
        rig_.kmt().queue_answer(answer, static_cast<std::uint32_t>(writer.offset()));
    }

    std::uint32_t last_capacity()
    {
        WireReader body(kmt().command_bytes(kmt().commands() - 1u) + CommandHeader::encoded_size,
                        MockKmt::journal_bytes - CommandHeader::encoded_size);
        CHECK_EQ(body.read_u64(), ScriptedHost::vk_physical_device);
        CHECK_EQ(body.read_u8(), 0u);
        return body.read_u8() == 1u ? body.read_u32() : 0u;
    }

private:
    IcdRig           rig_;
    VkInstance       instance_;
    VkPhysicalDevice physical_device_;
};

const char *const host_extensions[] = {"VK_KHR_spirv_1_4", "VK_KHR_timeline_semaphore", "VK_KHR_surface"};

}

TEST(icd_extensions, reports_the_memory_budget_and_the_swapchain_the_host_lists_and_nothing_else)
{ ExtensionRig rig;
  const char *const listed[] = {"VK_KHR_swapchain", "VK_EXT_memory_budget", "VK_KHR_spirv_1_4"};
  rig.answer_count(VK_SUCCESS, 3u);
  rig.answer_list(VK_SUCCESS, 3u, listed, 3u);
  std::uint32_t         count = 3u;
  VkExtensionProperties properties[3] = {};
  CHECK_EQ(rig.enumerate(&count, properties), VK_SUCCESS);
  CHECK_EQ(count, 2u);
  CHECK(std::strcmp(properties[0].extensionName, "VK_EXT_memory_budget") == 0);
  CHECK_EQ(properties[0].specVersion, 1u);
  CHECK(std::strcmp(properties[1].extensionName, "VK_KHR_swapchain") == 0);
  CHECK_EQ(properties[1].specVersion, 1u);
  CHECK_EQ(properties[2].extensionName[0], '\0'); }

TEST(icd_extensions, reports_none_of_the_hosts_extensions_while_none_is_proven)
{ ExtensionRig rig;
  rig.answer_count(VK_SUCCESS, 3u);
  rig.answer_list(VK_SUCCESS, 3u, host_extensions, 3u);
  std::uint32_t count = 7u;
  const std::uint32_t sent = rig.kmt().commands();
  const std::uint32_t allocations = rig.rig().platform().live_allocations();
  VkExtensionProperties properties[3] = {};
  CHECK_EQ(rig.enumerate(&count, properties), VK_SUCCESS);
  CHECK_EQ(count, 0u);
  CHECK_EQ(properties[0].extensionName[0], '\0');
  CHECK_EQ(rig.kmt().commands(), sent + 2u);
  CHECK_EQ(rig.kmt().command_id(sent), static_cast<std::uint32_t>(kCmd_vkEnumerateDeviceExtensionProperties));
  CHECK_EQ(rig.last_capacity(), 3u);
  CHECK_EQ(rig.rig().platform().live_allocations(), allocations);
  CHECK_EQ(rig.kmt().queued_answers(), 0u);
  rig.answer_count(VK_SUCCESS, 0u);
  count = 7u;
  CHECK_EQ(rig.enumerate(&count, nullptr), VK_SUCCESS);
  CHECK_EQ(count, 0u);
  CHECK_EQ(rig.kmt().commands(), sent + 3u);
  CHECK_EQ(rig.last_capacity(), 0u); }

TEST(icd_extensions, the_hosts_refusals_are_passed_on_and_a_shorter_list_is_filtered)
{ ExtensionRig rig;
  std::uint32_t count = 0u;
  rig.answer_count(vk_error_initialization_failed, 0u);
  CHECK_EQ(rig.enumerate(&count, nullptr), vk_error_initialization_failed);
  rig.answer_count(VK_SUCCESS, 3u);
  rig.answer_list(VK_ERROR_OUT_OF_HOST_MEMORY, 0u, host_extensions, 0u);
  CHECK_EQ(rig.enumerate(&count, nullptr), VK_ERROR_OUT_OF_HOST_MEMORY);
  rig.answer_count(VK_SUCCESS, 3u);
  rig.answer_list(VK_INCOMPLETE, 2u, host_extensions, 2u);
  count = 5u;
  CHECK_EQ(rig.enumerate(&count, nullptr), VK_SUCCESS);
  CHECK_EQ(count, 0u);
  CHECK_EQ(rig.kmt().queued_answers(), 0u); }

TEST(icd_extensions, a_host_claiming_more_than_it_sends_or_than_any_driver_lists_is_refused)
{ ExtensionRig rig;
  const std::uint32_t allocations = rig.rig().platform().live_allocations();
  std::uint32_t count = 0u;
  rig.answer_count(VK_SUCCESS, 2u);
  rig.answer_list(VK_SUCCESS, 3u, host_extensions, 3u);
  CHECK_EQ(rig.enumerate(&count, nullptr), VK_ERROR_DEVICE_LOST);
  CHECK_EQ(rig.rig().platform().live_allocations(), allocations);
  const std::uint32_t sent = rig.kmt().commands();
  rig.answer_count(VK_SUCCESS, 1025u);
  CHECK_EQ(rig.enumerate(&count, nullptr), VK_ERROR_OUT_OF_HOST_MEMORY);
  CHECK_EQ(rig.kmt().commands(), sent + 1u);
  CHECK_EQ(rig.rig().log().holding("the host lists more device extensions than 1024: 1025"), 1u);
  rig.answer_count(VK_SUCCESS, 1024u);
  rig.answer_list(VK_SUCCESS, 1024u, host_extensions, 3u);
  CHECK_EQ(rig.enumerate(&count, nullptr), VK_ERROR_DEVICE_LOST);
  CHECK_EQ(rig.kmt().commands(), sent + 3u);
  CHECK_EQ(rig.rig().platform().live_allocations(), allocations); }

TEST(icd_extensions, no_memory_for_the_hosts_list_answers_out_of_host_memory_without_asking_for_it)
{ ExtensionRig rig;
  rig.answer_count(VK_SUCCESS, 3u);
  rig.rig().platform().fail_allocation_at(1u);
  const std::uint32_t sent = rig.kmt().commands();
  std::uint32_t count = 0u;
  CHECK_EQ(rig.enumerate(&count, nullptr), VK_ERROR_OUT_OF_HOST_MEMORY);
  CHECK_EQ(rig.kmt().commands(), sent + 1u); }

TEST(icd_extensions, a_layer_or_a_physical_device_it_did_not_make_is_not_asked_about)
{ ExtensionRig rig;
  const std::uint32_t sent = rig.kmt().commands();
  std::uint32_t count = 4u;
  CHECK_EQ(rig.enumerate(&count, nullptr, "VK_LAYER_KHRONOS_validation"), VK_ERROR_LAYER_NOT_PRESENT);
  CHECK_EQ(count, 4u);
  CHECK_EQ(rig.rig().log().holding("device extensions of a layer"), 1u);
  int stack = 0;
  CHECK_EQ(rig.rig().root().enumerate_device_extensions(reinterpret_cast<VkPhysicalDevice>(&stack), nullptr, &count,
                                                         nullptr),
           VK_ERROR_INITIALIZATION_FAILED);
  VkDeviceCreateInfo info = {};
  VkDevice device = VK_NULL_HANDLE;
  CHECK_EQ(rig.rig().root().create_device(reinterpret_cast<VkPhysicalDevice>(&stack), &info, nullptr, &device),
           VK_ERROR_INITIALIZATION_FAILED);
  CHECK_EQ(rig.rig().log().holding("on a physical device this driver did not make"), 2u);
  CHECK_EQ(rig.kmt().commands(), sent); }

TEST(icd_extensions, a_device_naming_an_extension_not_reported_is_refused_before_the_host_sees_it)
{ ExtensionRig rig;
  const std::uint32_t sent = rig.kmt().commands();
  const char *const implemented[] = {"VK_KHR_spirv_1_4"};
  VkDevice device = VK_NULL_HANDLE;
  CHECK_EQ(rig.create(implemented, 1u, &device), VK_ERROR_EXTENSION_NOT_PRESENT);
  CHECK(device == VK_NULL_HANDLE);
  CHECK_EQ(rig.rig().log().holding("does not report: VK_KHR_spirv_1_4"), 1u);
  const char *const withheld[] = {"VK_KHR_surface"};
  CHECK_EQ(rig.create(withheld, 1u, &device), VK_ERROR_EXTENSION_NOT_PRESENT);
  const char *const unnamed[] = {nullptr};
  CHECK_EQ(rig.create(unnamed, 1u, &device), VK_ERROR_EXTENSION_NOT_PRESENT);
  CHECK_EQ(rig.rig().log().holding("does not report: (null)"), 1u);
  CHECK_EQ(rig.kmt().commands(), sent); }

TEST(icd_extensions, a_device_naming_no_extension_is_made_through_its_physical_device)
{ ExtensionRig rig;
  VkDevice device = VK_NULL_HANDLE;
  rig.rig().host().vk_handle(0, ScriptedHost::vk_device);
  CHECK_EQ(rig.create(nullptr, 0u, &device), VK_SUCCESS);
  CHECK_EQ(wire_of(rig.rig(), HandleKind::device, device), ScriptedHost::vk_device);
  CHECK(static_cast<LoaderHandle *>(static_cast<void *>(device))->parent() ==
        static_cast<LoaderHandle *>(static_cast<void *>(rig.physical_device())));
  CHECK_EQ(rig.kmt().command_id(rig.kmt().commands() - 1u), static_cast<std::uint32_t>(kCmd_vkCreateDevice));
  VkDevice refused = VK_NULL_HANDLE;
  rig.rig().host().vk_handle(vk_error_initialization_failed, 0u);
  CHECK_EQ(rig.create(nullptr, 0u, &refused), vk_error_initialization_failed);
  CHECK(refused == VK_NULL_HANDLE);
  rig.rig().host().vk_handle(0, 0u);
  CHECK_EQ(rig.create(nullptr, 0u, &refused), VK_ERROR_OUT_OF_HOST_MEMORY);
  CHECK(refused == VK_NULL_HANDLE);
  CHECK_EQ(rig.rig().log().holding("no handle for host object"), 1u); }

TEST(icd_extensions, the_swapchain_the_application_enables_is_not_asked_of_the_host)
{ ExtensionRig rig;
  VkDevice     device = VK_NULL_HANDLE;
  rig.rig().host().vk_handle(0, ScriptedHost::vk_device);
  const char *const enabled[] = {"VK_KHR_swapchain"};
  CHECK_EQ(rig.create(enabled, 1u, &device), VK_SUCCESS);
  CHECK_EQ(wire_of(rig.rig(), HandleKind::device, device), ScriptedHost::vk_device);
  const std::uint32_t sent = rig.kmt().commands() - 1u;
  CHECK_EQ(rig.kmt().command_id(sent), static_cast<std::uint32_t>(kCmd_vkCreateDevice));
  std::uint8_t        block[512] = {};
  av::WireArena       arena(block, sizeof block);
  const std::uint8_t *bytes = rig.kmt().command_bytes(sent);
  WireReader          body(bytes + CommandHeader::encoded_size, MockKmt::journal_bytes - CommandHeader::encoded_size);
  CHECK_EQ(body.read_u64(), ScriptedHost::vk_physical_device);
  VkDeviceCreateInfo forwarded = {};
  CHECK(deserialize_VkDeviceCreateInfo(body, identity_translation, &forwarded, arena));
  CHECK_EQ(forwarded.enabledExtensionCount, 0u); }

TEST(icd_extensions, the_window_system_instance_extensions_are_not_asked_of_the_host)
{ IcdRig            rig;
  const char *const enabled[] = {"VK_KHR_surface", "VK_KHR_win32_surface"};
  const VkInstance  instance = rig.create_instance(enabled, 2u);
  CHECK(instance != VK_NULL_HANDLE);
  const std::uint32_t sent = rig.kmt().commands() - 1u;
  CHECK_EQ(rig.kmt().command_id(sent), static_cast<std::uint32_t>(kCmd_vkCreateInstance));
  std::uint8_t        block[512] = {};
  av::WireArena       arena(block, sizeof block);
  const std::uint8_t *bytes = rig.kmt().command_bytes(sent);
  WireReader          body(bytes + CommandHeader::encoded_size, MockKmt::journal_bytes - CommandHeader::encoded_size);
  VkInstanceCreateInfo forwarded = {};
  CHECK(deserialize_VkInstanceCreateInfo(body, identity_translation, &forwarded, arena));
  CHECK_EQ(forwarded.enabledExtensionCount, 0u);
  CHECK(rig.root().invariants_hold());
  rig.destroy_instance(instance); }

TEST(icd_extensions, a_list_naming_nothing_is_refused_and_a_long_one_is_passed_on_once_per_name)
{ const char *unnamed[] = {"VK_KHR_get_physical_device_properties2", nullptr};
  { IcdRig              rig;
    const std::uint32_t quiet = rig.kmt().commands();
    CHECK(rig.create_instance(unnamed, 2u) == VK_NULL_HANDLE);
    CHECK_EQ(rig.kmt().commands(), quiet);
    CHECK_EQ(rig.log().holding("not a list this driver can pass on"), 1u);
    CHECK(rig.root().invariants_hold()); }
  static const char *many[400];
  for (const char *&name : many) name = "VK_KHR_get_physical_device_properties2";
  { IcdRig         rig;
    const VkInstance instance = rig.create_instance(many, 400u);
    CHECK(instance != VK_NULL_HANDLE);
    const std::uint32_t sent = rig.kmt().commands() - 1u;
    CHECK_EQ(rig.kmt().command_id(sent), static_cast<std::uint32_t>(kCmd_vkCreateInstance));
    std::uint8_t         block[512] = {};
    av::WireArena        arena(block, sizeof block);
    const std::uint8_t  *bytes = rig.kmt().command_bytes(sent);
    WireReader           body(bytes + CommandHeader::encoded_size, MockKmt::journal_bytes - CommandHeader::encoded_size);
    VkInstanceCreateInfo forwarded = {};
    CHECK(deserialize_VkInstanceCreateInfo(body, identity_translation, &forwarded, arena));
    CHECK_EQ(forwarded.enabledExtensionCount, 1u);
    CHECK(rig.root().invariants_hold());
    rig.destroy_instance(instance); }
  { ExtensionRig        rig;
    const std::uint32_t quiet = rig.kmt().commands();
    VkDevice            device = VK_NULL_HANDLE;
    const char *const   enabled[] = {"VK_KHR_swapchain"};
    rig.rig().platform().fail_allocation_at(1u);
    CHECK_EQ(rig.create(enabled, 1u, &device), VK_ERROR_OUT_OF_HOST_MEMORY);
    CHECK(device == VK_NULL_HANDLE);
    CHECK_EQ(rig.kmt().commands(), quiet);
    CHECK_EQ(rig.rig().log().holding("not a list this driver can pass on"), 1u); } }

TEST(icd_extensions, the_mapping_and_template_extensions_are_reported_and_placement_is_not)
{ ExtensionRig rig;
  const char *const listed[] = {"VK_KHR_map_memory2", "VK_EXT_map_memory_placed", "VK_KHR_push_descriptor"};
  rig.answer_count(VK_SUCCESS, 3u);
  rig.answer_list(VK_SUCCESS, 3u, listed, 3u);
  std::uint32_t         count = 8u;
  VkExtensionProperties properties[8] = {};
  CHECK_EQ(rig.enumerate(&count, properties), VK_SUCCESS);
  bool map_memory2 = false;
  bool placed = false;
  bool templates = false;
  bool push_descriptor = false;
  for (std::uint32_t index = 0u; index < count; index++) {
      map_memory2 = map_memory2 || std::strcmp(properties[index].extensionName, "VK_KHR_map_memory2") == 0;
      placed = placed || std::strcmp(properties[index].extensionName, "VK_EXT_map_memory_placed") == 0;
      templates = templates || std::strcmp(properties[index].extensionName,
                                           "VK_KHR_descriptor_update_template") == 0;
      push_descriptor = push_descriptor || std::strcmp(properties[index].extensionName,
                                                       "VK_KHR_push_descriptor") == 0;
  }
  CHECK(map_memory2);
  CHECK(push_descriptor);
  CHECK(!placed);
  CHECK(!templates);
  const char *const with_templates[] = {"VK_KHR_descriptor_update_template"};
  rig.answer_count(VK_SUCCESS, 1u);
  rig.answer_list(VK_SUCCESS, 1u, with_templates, 1u);
  count = 8u;
  VkExtensionProperties second[8] = {};
  CHECK_EQ(rig.enumerate(&count, second), VK_SUCCESS);
  CHECK_EQ(count, 1u);
  CHECK(std::strcmp(second[0].extensionName, "VK_KHR_descriptor_update_template") == 0);
  VkDevice          device = VK_NULL_HANDLE;
  const char *const enabling[] = {"VK_KHR_map_memory2", "VK_KHR_descriptor_update_template",
                                  "VK_KHR_push_descriptor"};
  rig.rig().host().vk_handle(0, ScriptedHost::vk_device);
  CHECK_EQ(rig.create(enabling, 3u, &device), VK_SUCCESS);
  const char *const placing[] = {"VK_EXT_map_memory_placed"};
  CHECK_EQ(rig.create(placing, 1u, &device), VK_ERROR_EXTENSION_NOT_PRESENT); }

TEST(icd_extensions, the_applications_own_enumeration_notes_the_hosts_dma_buf)
{ ExtensionRig rig;
  const char *const listed[] = {"VK_KHR_external_memory_fd", "VK_EXT_external_memory_dma_buf"};
  std::uint32_t     count = 0u;
  rig.answer_count(VK_SUCCESS, 2u);
  rig.answer_list(VK_SUCCESS, 2u, listed, 2u);
  CHECK_EQ(rig.enumerate(&count, nullptr), VK_SUCCESS);
  rig.answer_count(VK_SUCCESS, 2u);
  rig.answer_list(VK_SUCCESS, 2u, listed, 2u);
  VkExtensionProperties properties[8] = {};
  std::uint32_t         room = 8u;
  CHECK_EQ(rig.enumerate(&room, properties), VK_SUCCESS);
  for (std::uint32_t index = 0u; index < room; index++) {
      CHECK(std::strcmp(properties[index].extensionName, "VK_EXT_external_memory_dma_buf") != 0);
      CHECK(std::strcmp(properties[index].extensionName, "VK_KHR_external_memory_fd") != 0);
  } }

TEST(icd_extensions, an_extension_list_with_a_hole_in_it_is_refused_before_anything_reads_it)
{
  {
    ExtensionRig      rig;
    const char *const names[] = {nullptr, "VK_KHR_external_memory_win32"};
    VkDevice          device = VK_NULL_HANDLE;
    CHECK_EQ(rig.create(names, 2u, &device), VK_ERROR_EXTENSION_NOT_PRESENT);
    CHECK(device == VK_NULL_HANDLE); }

}


TEST(icd_extensions, no_memory_to_read_the_hosts_list_into_leaves_the_device_made_and_sharing_as_it_was)
{ const char *const host_listed[] = {"VK_KHR_external_memory_fd", "VK_EXT_external_memory_dma_buf"};
  const char *const names[] = {"VK_KHR_external_memory_win32"};
  std::uint32_t     made = 0u;
  for (std::uint32_t nth = 1u; nth <= 4u; nth++) {
      ExtensionRig rig;
      rig.answer_count(VK_SUCCESS, 2u);
      rig.answer_list(VK_SUCCESS, 2u, host_listed, 2u);
      rig.rig().host().vk_handle(0, av::ScriptedHost::vk_device);
      rig.rig().platform().fail_allocation_at(nth);
      VkDevice device = VK_NULL_HANDLE;
      if (rig.create(names, 1u, &device) != VK_SUCCESS) continue;
      made++;
      CHECK(device != VK_NULL_HANDLE);
      CHECK(rig.rig().root().invariants_hold());
  }
  CHECK(made != 0u); }
