/* Device extensions as the loader reaches them through the root (research.md
 * §9): the host is asked how many it lists and then for all of them, and the
 * application sees what the ICD's policy reports - only VK_EXT_memory_budget,
 * which it answers itself (T035), whatever else the host lists. A device naming an
 * extension the policy would not report is refused before the host sees it.
 * The host's answers are hostile: refusals, a count past what it sends, more
 * than any driver lists. Invariants after every call that changes the root. */
#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "harness/guest/scripted_host.hpp"
#include "harness/icd/icd_rig.hpp"
#include "icd/vulkan/icd_root.hpp"
#include "icd/vulkan/loader_handle.hpp"
#include "proto/generated/vulkan/vk_protocol_ids.h"
#include "proto/generated/vulkan/vk_serialize.h"
#include "proto/icd_handle_registry.hpp"
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

    /* The host's count: a result and how many it lists. */
    void answer_count(std::int32_t result, std::uint32_t listed) { rig_.host().vk_count(result, listed); }

    /* The host's list: a result, the count it claims, and `sent` entries. */
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

    /* Whether the last command asked for the list itself, and with room for how many. */
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

const char *const host_extensions[] = {"VK_KHR_spirv_1_4", "VK_KHR_timeline_semaphore", "VK_KHR_swapchain"};

}  // namespace

/* The one extension the ICD has proven, once the host lists it, beside others
 * it does not report. */
TEST(icd_extensions, reports_the_memory_budget_the_host_lists_and_nothing_else)
{ ExtensionRig rig;
  const char *const listed[] = {"VK_KHR_swapchain", "VK_EXT_memory_budget", "VK_KHR_spirv_1_4"};
  rig.answer_count(VK_SUCCESS, 3u);
  rig.answer_list(VK_SUCCESS, 3u, listed, 3u);
  std::uint32_t         count = 3u;
  VkExtensionProperties properties[3] = {};
  CHECK_EQ(rig.enumerate(&count, properties), VK_SUCCESS);
  CHECK_EQ(count, 1u);
  CHECK(std::strcmp(properties[0].extensionName, "VK_EXT_memory_budget") == 0);
  CHECK_EQ(properties[0].specVersion, 1u);
  CHECK_EQ(properties[1].extensionName[0], '\0'); }

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
  /* A host listing none is asked once. */
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
  /* The host's list shrank between the two questions. */
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
  const char *const withheld[] = {"VK_KHR_swapchain"};
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
  CHECK_EQ(handle_to_wire(device), ScriptedHost::vk_device);
  CHECK(static_cast<LoaderHandle *>(static_cast<void *>(device))->parent() ==
        static_cast<LoaderHandle *>(static_cast<void *>(rig.physical_device())));
  CHECK_EQ(rig.kmt().command_id(rig.kmt().commands() - 1u), static_cast<std::uint32_t>(kCmd_vkCreateDevice));
  /* The host's refusal is passed on; a device the host names 0 has no handle. */
  VkDevice refused = VK_NULL_HANDLE;
  rig.rig().host().vk_handle(vk_error_initialization_failed, 0u);
  CHECK_EQ(rig.create(nullptr, 0u, &refused), vk_error_initialization_failed);
  CHECK(refused == VK_NULL_HANDLE);
  rig.rig().host().vk_handle(0, 0u);
  CHECK_EQ(rig.create(nullptr, 0u, &refused), VK_ERROR_OUT_OF_HOST_MEMORY);
  CHECK(refused == VK_NULL_HANDLE);
  CHECK_EQ(rig.rig().log().holding("no handle for host object"), 1u); }
