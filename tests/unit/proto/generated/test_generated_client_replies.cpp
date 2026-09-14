/* The generated clients against a host that answers more than it was asked
 * for, over MockKmt with the host played through ScriptedHost (feature 044,
 * T032). A count, a length or a byte total in a reply is the host's word, and
 * the caller's array or buffer holds only what the caller said it does: a
 * reply past that is a failed call - the API's own error - with nothing
 * written into the caller's memory, and a reply cut short is a failed call
 * too, never the success its first bytes claimed. Every destination sits
 * between guard bands, and every byte of it the call had no business writing
 * is checked still to hold what the test put there. */
#include "core/support/status.hpp"
#include "guest/kmt/pci_identity.hpp"
#include "guest/suite/adapter_scenario.hpp"
#include "guest/suite/adapter_search.hpp"
#include "guest/suite/guest_report.hpp"
#include "guest/suite/scenario_steps.hpp"
#include "guest/suite/scenario_transport.hpp"
#include "harness/framework/guarded_buffer.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "harness/guest/scripted_host.hpp"
#include "harness/transport/mock_transport.hpp"
#include "proto/client_blob_arena.hpp"
#include "proto/client_transport.hpp"
#include "proto/context/sub_api.hpp"
#include "proto/generated/opencl/cl_dispatch_client.h"
#include "proto/generated/vulkan/vk_default_dispatch.h"
#include "proto/generated/vulkan/vk_dispatch_client.h"
#include "proto/generated/vulkan/vk_dispatch_server.h"
#include "proto/generated/vulkan/vk_serialize.h"
#include "proto/wire/wire_arena.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

using av::AdapterScenario;
using av::AdapterSearch;
using av::GuardedBuffer;
using av::GuestReport;
using av::MockKmt;
using av::MockTransport;
using av::PciIdentity;
using av::ScenarioSteps;
using av::ScenarioTransport;
using av::ScriptedHost;
using av::Status;
using av::SubApi;
using av::TransportResult;
using av::WireArena;
using av::WireReader;
using av::WireWriter;

namespace {

const PciIdentity rvk(AdapterScenario::virtio_vendor, AdapterScenario::virtio_gpu_device,
                      AdapterScenario::virtio_vendor, AdapterScenario::rvk_subsystem);
constexpr std::uint8_t  untouched = 0xC3u;
constexpr std::uint32_t never_reported = 77u;
/* A Vulkan reply's result and count, before its elements. */
constexpr std::uint32_t vk_reply_head = sizeof(std::int32_t) + sizeof(std::uint32_t);

/* One context of a sub-API, open on the adapter, with the host behind it. */
class Rig {
public:
    explicit Rig(SubApi sub_api)
        : search_(kmt_), host_(kmt_), steps_(kmt_, report_, "replies"), link_(kmt_, search_, steps_, sub_api)
    {
        kmt_.add_adapter(rvk, AdapterScenario::wddm_3_2);
        CHECK_STATUS(search_.find(rvk), Status::ok);
        CHECK_STATUS(link_.open(), Status::ok);
    }

    ScriptedHost        &host() { return host_; }
    av::ClientTransport &transport() { return link_.transport(); }

    ~Rig() { CHECK_EQ(kmt_.queued_answers(), 0u); }

private:
    MockKmt           kmt_;
    AdapterSearch     search_;
    ScriptedHost      host_;
    GuestReport       report_;
    ScenarioSteps     steps_;
    ScenarioTransport link_;
};

/* A caller's buffer of `size` bytes, filled with a byte no reply here carries. */
class Destination {
public:
    explicit Destination(std::size_t size) : buffer_(size) { std::memset(buffer_.data(), untouched, size); }

    template <typename T>
    T *as() { return reinterpret_cast<T *>(buffer_.data()); }

    /* Nothing was written from `offset` on, and nothing around the buffer. */
    bool untouched_from(std::size_t offset) const
    {
        for (std::size_t index = offset; index < buffer_.size(); index++) {
            if (buffer_.data()[index] != untouched) return false;
        }
        return buffer_.guards_intact();
    }

private:
    mutable GuardedBuffer buffer_;
};

VkInstance       instance() { return reinterpret_cast<VkInstance>(ScriptedHost::vk_instance); }
VkPhysicalDevice physical_device() { return reinterpret_cast<VkPhysicalDevice>(ScriptedHost::vk_physical_device); }

}  // namespace

TEST(generated_client_replies, vulkan_devices_listed_past_the_callers_array_are_refused_and_none_written)
{ Rig rig(SubApi::vulkan);
  rig.host().vk_physical_devices(0, 3u);
  Destination devices(2u * sizeof(std::uint64_t));
  std::uint32_t count = 2u;
  CHECK(av::vkproto::rvk_vkEnumeratePhysicalDevices(rig.transport(), instance(), &count,
                                                    devices.as<std::uint64_t>()) == VK_ERROR_DEVICE_LOST);
  CHECK_EQ(count, 2u);
  CHECK(devices.untouched_from(0u)); }

/* The bound is the room given, not less: a host filling it exactly is read
 * whole, and one listing fewer leaves the rest of the array alone. */
TEST(generated_client_replies, vulkan_devices_filling_at_most_the_room_given_are_read)
{ Rig rig(SubApi::vulkan);
  rig.host().vk_physical_devices(0, 2u);
  rig.host().vk_physical_devices(0, 2u);
  { Destination devices(2u * sizeof(std::uint64_t));
    std::uint32_t count = 2u;
    CHECK(av::vkproto::rvk_vkEnumeratePhysicalDevices(rig.transport(), instance(), &count,
                                                      devices.as<std::uint64_t>()) == VK_SUCCESS);
    CHECK_EQ(count, 2u);
    CHECK_EQ(devices.as<std::uint64_t>()[1], ScriptedHost::vk_physical_device + 1u);
    CHECK(devices.untouched_from(2u * sizeof(std::uint64_t))); }
  { Destination devices(3u * sizeof(std::uint64_t));
    std::uint32_t count = 3u;
    CHECK(av::vkproto::rvk_vkEnumeratePhysicalDevices(rig.transport(), instance(), &count,
                                                      devices.as<std::uint64_t>()) == VK_SUCCESS);
    CHECK_EQ(count, 2u);
    CHECK_EQ(devices.as<std::uint64_t>()[0], ScriptedHost::vk_physical_device);
    CHECK(devices.untouched_from(2u * sizeof(std::uint64_t))); } }

/* A reply that stops inside its list fails, and the caller's count still says
 * what the caller passed rather than what the host began to answer. The
 * element the reply stopped inside is cleared, as the reader clears whatever
 * it could not fill; nothing past it is written. */
TEST(generated_client_replies, vulkan_a_device_list_cut_short_fails_and_keeps_the_callers_count)
{ Rig rig(SubApi::vulkan);
  rig.host().cut_next(vk_reply_head + sizeof(std::uint64_t));
  rig.host().vk_physical_devices(0, 2u);
  Destination devices(3u * sizeof(std::uint64_t));
  std::uint32_t count = 3u;
  CHECK(av::vkproto::rvk_vkEnumeratePhysicalDevices(rig.transport(), instance(), &count,
                                                    devices.as<std::uint64_t>()) == VK_ERROR_DEVICE_LOST);
  CHECK_EQ(count, 3u);
  CHECK_EQ(devices.as<std::uint64_t>()[0], ScriptedHost::vk_physical_device);
  CHECK_EQ(devices.as<std::uint64_t>()[1], 0u);
  CHECK(devices.untouched_from(2u * sizeof(std::uint64_t))); }

/* The structure-array shape, on a command with no result to fail with: the
 * caller's count and array are left as they were. */
TEST(generated_client_replies, vulkan_structures_listed_past_the_callers_array_are_refused)
{ Rig rig(SubApi::vulkan);
  rig.host().vk_queue_families(4u);
  rig.host().vk_queue_families(1u);
  { Destination families(sizeof(VkQueueFamilyProperties));
    std::uint32_t count = 1u;
    av::vkproto::rvk_vkGetPhysicalDeviceQueueFamilyProperties(rig.transport(), physical_device(), &count,
                                                              families.as<VkQueueFamilyProperties>());
    CHECK_EQ(count, 1u);
    CHECK(families.untouched_from(0u)); }
  { Destination families(sizeof(VkQueueFamilyProperties));
    std::uint32_t count = 1u;
    av::vkproto::rvk_vkGetPhysicalDeviceQueueFamilyProperties(rig.transport(), physical_device(), &count,
                                                              families.as<VkQueueFamilyProperties>());
    CHECK_EQ(count, 1u);
    CHECK_EQ(families.as<VkQueueFamilyProperties>()->queueCount, 1u);
    CHECK(families.untouched_from(sizeof(VkQueueFamilyProperties))); } }

TEST(generated_client_replies, opencl_platforms_listed_past_num_entries_are_refused_and_none_written)
{ Rig rig(SubApi::opencl);
  const std::uint64_t ids[] = {ScriptedHost::cl_cpu_platform, ScriptedHost::cl_gpu_platform,
                               ScriptedHost::cl_gpu_platform + 1u};
  rig.host().cl_ids(0, ids, 3u, 3u);
  Destination platforms(2u * sizeof(std::uint64_t));
  cl_uint total = never_reported;
  CHECK_EQ(av::clproto::rvk_clGetPlatformIDs(rig.transport(), 2u, platforms.as<std::uint64_t>(), &total),
           CL_OUT_OF_RESOURCES);
  CHECK_EQ(total, never_reported);
  CHECK(platforms.untouched_from(0u)); }

TEST(generated_client_replies, opencl_devices_are_bounded_by_num_entries_and_read_up_to_it)
{ Rig rig(SubApi::opencl);
  const std::uint64_t ids[] = {ScriptedHost::cl_device, ScriptedHost::cl_device + 1u};
  rig.host().cl_ids(0, ids, 2u, 2u);
  rig.host().cl_ids(0, ids, 1u, 2u);
  { Destination devices(sizeof(std::uint64_t));
    cl_uint total = never_reported;
    CHECK_EQ(av::clproto::rvk_clGetDeviceIDs(rig.transport(), ScriptedHost::cl_gpu_platform, CL_DEVICE_TYPE_GPU, 1u,
                                             devices.as<std::uint64_t>(), &total),
             CL_OUT_OF_RESOURCES);
    CHECK_EQ(total, never_reported);
    CHECK(devices.untouched_from(0u)); }
  { Destination devices(sizeof(std::uint64_t));
    cl_uint total = never_reported;
    CHECK_EQ(av::clproto::rvk_clGetDeviceIDs(rig.transport(), ScriptedHost::cl_gpu_platform, CL_DEVICE_TYPE_GPU, 1u,
                                             devices.as<std::uint64_t>(), &total),
             CL_SUCCESS);
    CHECK_EQ(total, 2u);
    CHECK_EQ(devices.as<std::uint64_t>()[0], ScriptedHost::cl_device);
    CHECK(devices.untouched_from(sizeof(std::uint64_t))); } }

/* A read's byte total past the size the caller asked for is refused before a
 * byte lands; one exactly that size is the output. */
TEST(generated_client_replies, opencl_a_read_answering_more_bytes_than_its_size_is_refused)
{ Rig rig(SubApi::opencl);
  const float values[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
  rig.host().cl_read(0, values, 5u);
  rig.host().cl_read(0, values, 4u);
  { Destination output(4u * sizeof(float));
    CHECK_EQ(av::clproto::rvk_clEnqueueReadBuffer(rig.transport(), ScriptedHost::cl_queue, ScriptedHost::cl_buffer,
                                                  CL_TRUE, 0u, 4u * sizeof(float), output.as<float>(), 0u,
                                                  nullptr, nullptr),
             CL_OUT_OF_RESOURCES);
    CHECK(output.untouched_from(0u)); }
  { Destination output(4u * sizeof(float));
    CHECK_EQ(av::clproto::rvk_clEnqueueReadBuffer(rig.transport(), ScriptedHost::cl_queue, ScriptedHost::cl_buffer,
                                                  CL_TRUE, 0u, 4u * sizeof(float), output.as<float>(), 0u,
                                                  nullptr, nullptr),
             CL_SUCCESS);
    CHECK(output.as<float>()[3] == 4.0f);
    CHECK(output.untouched_from(4u * sizeof(float))); } }

/* The string and byte outputs sized by an input: an info answer longer than
 * param_value_size is refused, and the size it reports is not passed on. */
TEST(generated_client_replies, opencl_an_info_answer_longer_than_its_room_is_refused)
{ Rig rig(SubApi::opencl);
  constexpr const char vendor[] = "a vendor name longer than eight";
  rig.host().cl_info(0, vendor, sizeof vendor);
  Destination value(8u);
  std::size_t reported = never_reported;
  CHECK_EQ(av::clproto::rvk_clGetDeviceInfo(rig.transport(), ScriptedHost::cl_device, CL_DEVICE_VENDOR, 8u,
                                            value.as<char>(), &reported),
           CL_OUT_OF_RESOURCES);
  CHECK_EQ(reported, static_cast<std::size_t>(never_reported));
  CHECK(value.untouched_from(0u)); }

/* Cut before its result, and cut after a success but inside its list: both are
 * failed calls, and the second used to answer the success it began with.
 * Nothing is written past the id the reply stopped inside. */
TEST(generated_client_replies, opencl_a_reply_cut_short_is_a_failed_call_not_its_first_bytes)
{ Rig rig(SubApi::opencl);
  const std::uint64_t ids[] = {ScriptedHost::cl_cpu_platform, ScriptedHost::cl_gpu_platform};
  rig.host().cut_next(0u);
  rig.host().cl_ids(0, ids, 2u, 2u);
  rig.host().cut_next(2u + sizeof(std::uint64_t));
  rig.host().cl_ids(0, ids, 2u, 2u);
  for (std::uint32_t attempt = 0u; attempt < 2u; attempt++) {
      Destination platforms(3u * sizeof(std::uint64_t));
      cl_uint total = never_reported;
      CHECK_EQ(av::clproto::rvk_clGetPlatformIDs(rig.transport(), 3u, platforms.as<std::uint64_t>(), &total),
               CL_OUT_OF_RESOURCES);
      CHECK_EQ(total, never_reported);
      CHECK(platforms.untouched_from(attempt == 0u ? 0u : 2u * sizeof(std::uint64_t)));
  } }

/* An output structure with the caller's array hung off it (T037): the
 * application points a member at its own array and says in the count how much
 * room it has, and the call fills that array - it does not point the member
 * somewhere else. Driven through the generated server dispatch, so the request
 * is proven to carry the room as well as the reply to honour it. */
namespace {

constexpr std::uint64_t first_binary = 0x7700u;
constexpr std::uint32_t binaries_made = 3u;
constexpr std::size_t   served_bytes = 4096u;
constexpr std::size_t   host_arena_bytes = 16384u;
constexpr VkImageLayout host_layouts[] = {VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
constexpr std::uint32_t host_layout_count = sizeof host_layouts / sizeof host_layouts[0];

/* A driver answering as the specification has an implementation answer: a
 * null array is told how many there are; an array is filled up to the room its
 * count gives, and the count then says how many were written. */
class ArrayFillingDriver final : public av::vkproto::DefaultServerDispatch {
public:
    VkResult on_vkCreatePipelineBinariesKHR(VkDevice, const VkPipelineBinaryCreateInfoKHR *,
                                            const VkAllocationCallbacks *, VkPipelineBinaryHandlesInfoKHR *out) override
    { room_ = out->pipelineBinaryCount;
      if (out->pPipelineBinaries == nullptr) {
          out->pipelineBinaryCount = binaries_made;
          return VK_SUCCESS;
      }
      const std::uint32_t written = room_ < binaries_made ? room_ : binaries_made;
      for (std::uint32_t index = 0u; index < written; index++) {
          const std::uint64_t raw = first_binary + index;
          std::memcpy(&out->pPipelineBinaries[index], &raw, sizeof raw);
      }
      out->pipelineBinaryCount = written;
      return written < binaries_made ? VK_INCOMPLETE : VK_SUCCESS; }

    void on_vkGetPhysicalDeviceProperties2(VkPhysicalDevice, VkPhysicalDeviceProperties2 *out) override
    { for (auto *node = static_cast<VkBaseOutStructure *>(out->pNext); node != nullptr; node = node->pNext) {
          if (node->sType != VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_HOST_IMAGE_COPY_PROPERTIES) continue;
          auto *copy = reinterpret_cast<VkPhysicalDeviceHostImageCopyProperties *>(node);
          copy->copySrcLayoutCount = layouts(copy->pCopySrcLayouts, copy->copySrcLayoutCount);
          copy->copyDstLayoutCount = layouts(copy->pCopyDstLayouts, copy->copyDstLayoutCount);
      } }

    std::uint32_t room() const { return room_; }

private:
    static constexpr std::uint32_t never_asked = ~std::uint32_t{0};

    std::uint32_t layouts(VkImageLayout *array, std::uint32_t room) const
    { if (array == nullptr) return host_layout_count;
      const std::uint32_t written = room < host_layout_count ? room : host_layout_count;
      for (std::uint32_t index = 0u; index < written; index++) array[index] = host_layouts[index];
      return written; }

    std::uint32_t room_ = never_asked;
};

/* The generated server dispatch behind the client in one process: a request
 * is decoded into the driver, and what it answers is the reply. The client's
 * own arena refuses everything, as a guest's does. */
class ServedTransport final : public av::ClientTransport {
public:
    explicit ServedTransport(av::vkproto::ServerDispatch &driver)
        : driver_(driver), request_{}, reply_{}, block_{}, host_arena_(block_, sizeof block_),
          guest_arena_(nullptr, 0u), blobs_() {}

    WireWriter request_writer() override { return WireWriter(request_, sizeof request_); }
    TransportResult send_async(std::uint32_t, WireWriter &) override { return TransportResult::failed; }
    TransportResult send_sync(std::uint32_t command, WireWriter &writer, WireReader &reply) override
    { if (writer.status() != Status::ok) return TransportResult::failed;
      host_arena_.reset();
      WireReader payload(request_, writer.offset());
      WireWriter answer(reply_, sizeof reply_);
      if (!av::vkproto::dispatch_command(driver_, command, payload, answer, host_arena_))
          return TransportResult::failed;
      reply = WireReader(reply_, answer.offset());
      return TransportResult::ok; }
    WireArena           &decode_arena() override { return guest_arena_; }
    av::ClientBlobArena &blobs() override { return blobs_; }

private:
    av::vkproto::ServerDispatch &driver_;
    std::uint8_t                 request_[served_bytes];
    std::uint8_t                 reply_[served_bytes];
    std::uint8_t                 block_[host_arena_bytes];
    WireArena                    host_arena_;
    WireArena                    guest_arena_;
    av::ClientBlobArena          blobs_;
};

VkDevice device() { return reinterpret_cast<VkDevice>(ScriptedHost::vk_device); }

VkPipelineBinaryCreateInfoKHR binary_create()
{ VkPipelineBinaryCreateInfoKHR create = {};
  create.sType = VK_STRUCTURE_TYPE_PIPELINE_BINARY_CREATE_INFO_KHR;
  return create; }

VkPipelineBinaryHandlesInfoKHR binary_handles(std::uint32_t room, std::uint64_t *array)
{ VkPipelineBinaryHandlesInfoKHR handles = {};
  handles.sType = VK_STRUCTURE_TYPE_PIPELINE_BINARY_HANDLES_INFO_KHR;
  handles.pipelineBinaryCount = room;
  handles.pPipelineBinaries = reinterpret_cast<VkPipelineBinaryKHR *>(array);
  return handles; }

/* A host's whole answer to vkCreatePipelineBinariesKHR: VK_SUCCESS, then the
 * structure as its serializer writes it. */
std::size_t binaries_reply(std::uint8_t *reply, std::size_t size, const VkPipelineBinaryHandlesInfoKHR &answer)
{ WireWriter writer(reply, size);
  writer.write_pod<std::int32_t>(VK_SUCCESS);
  av::vkproto::serialize_VkPipelineBinaryHandlesInfoKHR(writer, answer);
  return writer.offset(); }

}  // namespace

TEST(generated_client_replies, vulkan_pipeline_binaries_fill_the_callers_array_up_to_the_room_it_gave)
{ ArrayFillingDriver driver;
  ServedTransport    link(driver);
  const VkPipelineBinaryCreateInfoKHR create = binary_create();
  Destination array(3u * sizeof(std::uint64_t));
  VkPipelineBinaryHandlesInfoKHR binaries = binary_handles(2u, array.as<std::uint64_t>());
  CHECK(av::vkproto::rvk_vkCreatePipelineBinariesKHR(link, device(), &create, nullptr, &binaries) == VK_INCOMPLETE);
  CHECK_EQ(driver.room(), 2u);
  CHECK_EQ(binaries.pipelineBinaryCount, 2u);
  CHECK(binaries.pPipelineBinaries == array.as<VkPipelineBinaryKHR>());
  CHECK_EQ(array.as<std::uint64_t>()[0], first_binary);
  CHECK_EQ(array.as<std::uint64_t>()[1], first_binary + 1u);
  CHECK(array.untouched_from(2u * sizeof(std::uint64_t))); }

/* The query half of the idiom: no array, and the count is how many there are. */
TEST(generated_client_replies, vulkan_pipeline_binaries_with_no_array_are_counted_and_stay_unpointed)
{ ArrayFillingDriver driver;
  ServedTransport    link(driver);
  const VkPipelineBinaryCreateInfoKHR create = binary_create();
  VkPipelineBinaryHandlesInfoKHR binaries = binary_handles(0u, nullptr);
  CHECK(av::vkproto::rvk_vkCreatePipelineBinariesKHR(link, device(), &create, nullptr, &binaries) == VK_SUCCESS);
  CHECK_EQ(driver.room(), 0u);
  CHECK_EQ(binaries.pipelineBinaryCount, binaries_made);
  CHECK(binaries.pPipelineBinaries == nullptr); }

/* The same shape inside a chain: a properties structure with two arrays of the
 * caller's, one with less room than the host has layouts and one with more. */
TEST(generated_client_replies, vulkan_host_image_copy_layouts_in_a_chain_fill_the_callers_arrays)
{ ArrayFillingDriver driver;
  ServedTransport    link(driver);
  Destination sources(3u * sizeof(VkImageLayout));
  Destination destinations(5u * sizeof(VkImageLayout));
  VkPhysicalDeviceHostImageCopyProperties copy = {};
  copy.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_HOST_IMAGE_COPY_PROPERTIES;
  copy.copySrcLayoutCount = 2u;
  copy.pCopySrcLayouts = sources.as<VkImageLayout>();
  copy.copyDstLayoutCount = 5u;
  copy.pCopyDstLayouts = destinations.as<VkImageLayout>();
  VkPhysicalDeviceProperties2 properties = {};
  properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
  properties.pNext = &copy;
  av::vkproto::rvk_vkGetPhysicalDeviceProperties2(link, physical_device(), &properties);
  CHECK(properties.pNext == &copy);
  CHECK_EQ(copy.copySrcLayoutCount, 2u);
  CHECK(copy.pCopySrcLayouts == sources.as<VkImageLayout>());
  CHECK_EQ(sources.as<std::uint32_t>()[0], static_cast<std::uint32_t>(VK_IMAGE_LAYOUT_GENERAL));
  CHECK_EQ(sources.as<std::uint32_t>()[1], static_cast<std::uint32_t>(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL));
  CHECK(sources.untouched_from(2u * sizeof(VkImageLayout)));
  CHECK_EQ(copy.copyDstLayoutCount, host_layout_count);
  CHECK(copy.pCopyDstLayouts == destinations.as<VkImageLayout>());
  CHECK_EQ(destinations.as<std::uint32_t>()[2],
           static_cast<std::uint32_t>(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));
  CHECK(destinations.untouched_from(host_layout_count * sizeof(VkImageLayout))); }

/* A host answering more binaries than the room given, or a count with no
 * binaries behind it, is refused with nothing written and the caller's count
 * as it was; so is one sending binaries for an array the caller never gave,
 * which has nowhere to land but the arena the guest refuses. */
TEST(generated_client_replies, vulkan_a_binary_count_the_caller_gave_no_room_for_is_refused)
{ MockTransport transport;
  const VkPipelineBinaryCreateInfoKHR create = binary_create();
  std::uint64_t host[] = {first_binary, first_binary + 1u, first_binary + 2u};
  std::uint8_t  reply[served_bytes] = {};
  const VkPipelineBinaryHandlesInfoKHR answers[] = {binary_handles(3u, host), binary_handles(2u, nullptr)};
  for (const VkPipelineBinaryHandlesInfoKHR &answer : answers) {
      transport.set_reply(reply, binaries_reply(reply, sizeof reply, answer));
      Destination array(2u * sizeof(std::uint64_t));
      VkPipelineBinaryHandlesInfoKHR binaries = binary_handles(2u, array.as<std::uint64_t>());
      CHECK(av::vkproto::rvk_vkCreatePipelineBinariesKHR(transport, device(), &create, nullptr, &binaries) ==
            VK_ERROR_DEVICE_LOST);
      CHECK_EQ(binaries.pipelineBinaryCount, 2u);
      CHECK(binaries.pPipelineBinaries == array.as<VkPipelineBinaryKHR>());
      CHECK(array.untouched_from(0u));
  }
  transport.set_reply(reply, binaries_reply(reply, sizeof reply, binary_handles(1u, host)));
  VkPipelineBinaryHandlesInfoKHR unasked = binary_handles(0u, nullptr);
  CHECK(av::vkproto::rvk_vkCreatePipelineBinariesKHR(transport, device(), &create, nullptr, &unasked) ==
        VK_ERROR_DEVICE_LOST);
  CHECK(unasked.pPipelineBinaries == nullptr); }

/* The void query has no error to return, so what protects the caller is that
 * its count and array are left alone: an application reading the count it
 * gets back never reads past its own array. */
TEST(generated_client_replies, vulkan_layouts_past_the_callers_room_leave_its_count_and_array_alone)
{ MockTransport transport;
  VkImageLayout host[4] = {VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL,
                           VK_IMAGE_LAYOUT_GENERAL};
  VkPhysicalDeviceHostImageCopyProperties host_copy = {};
  host_copy.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_HOST_IMAGE_COPY_PROPERTIES;
  host_copy.copySrcLayoutCount = 4u;
  host_copy.pCopySrcLayouts = host;
  VkPhysicalDeviceProperties2 host_properties = {};
  host_properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
  host_properties.pNext = &host_copy;
  std::uint8_t reply[served_bytes] = {};
  WireWriter   writer(reply, sizeof reply);
  writer.write_pod<std::int32_t>(0);
  av::vkproto::serialize_VkPhysicalDeviceProperties2(writer, host_properties);
  transport.set_reply(reply, writer.offset());

  Destination sources(2u * sizeof(VkImageLayout));
  VkPhysicalDeviceHostImageCopyProperties copy = {};
  copy.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_HOST_IMAGE_COPY_PROPERTIES;
  copy.copySrcLayoutCount = 2u;
  copy.pCopySrcLayouts = sources.as<VkImageLayout>();
  VkPhysicalDeviceProperties2 properties = {};
  properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
  properties.pNext = &copy;
  av::vkproto::rvk_vkGetPhysicalDeviceProperties2(transport, physical_device(), &properties);
  CHECK_EQ(copy.copySrcLayoutCount, 2u);
  CHECK(copy.pCopySrcLayouts == sources.as<VkImageLayout>());
  CHECK(sources.untouched_from(0u)); }

/* What tests/fuzz/fuzz_generated_client_replies.cpp found (T036): a size or a
 * count the reply carries beside what it wrote, rather than before it. */

/* An info query into a buffer succeeds only when the value fits, so a success
 * reporting a size past the buffer is refused - an application reads that many
 * bytes back. Asked with no buffer, the size is how much is needed, and any
 * size is an answer. */
TEST(generated_client_replies, opencl_a_successful_info_size_past_its_buffer_is_refused)
{ MockTransport transport;
  std::uint8_t reply[64] = {};
  const auto answer = [&](std::uint64_t written, std::uint64_t reported) {
      WireWriter writer(reply, sizeof reply);
      writer.write_varint(static_cast<std::int64_t>(CL_SUCCESS));
      if (written != 0u) {
          writer.write_varint(written);
          writer.write("name", written);
      }
      writer.write_varint(reported);
      transport.set_reply(reply, writer.offset());
  };
  { answer(4u, 9u);
    Destination value(8u);
    std::size_t reported = never_reported;
    CHECK_EQ(av::clproto::rvk_clGetDeviceInfo(transport, ScriptedHost::cl_device, CL_DEVICE_NAME, 8u,
                                              value.as<char>(), &reported),
             CL_OUT_OF_RESOURCES);
    CHECK_EQ(reported, static_cast<std::size_t>(never_reported)); }
  { answer(4u, 8u);
    Destination value(8u);
    std::size_t reported = never_reported;
    CHECK_EQ(av::clproto::rvk_clGetDeviceInfo(transport, ScriptedHost::cl_device, CL_DEVICE_NAME, 8u,
                                              value.as<char>(), &reported),
             CL_SUCCESS);
    CHECK_EQ(reported, 8u);
    CHECK(value.untouched_from(4u)); }
  { answer(0u, 64u);
    std::size_t reported = never_reported;
    CHECK_EQ(av::clproto::rvk_clGetDeviceInfo(transport, ScriptedHost::cl_device, CL_DEVICE_NAME, 0u, nullptr,
                                              &reported),
             CL_SUCCESS);
    CHECK_EQ(reported, 64u); } }

/* A count beside a fixed array: the structure's own size is the room, and the
 * void query has no error to return, so the caller's count is what is left. */
TEST(generated_client_replies, vulkan_memory_types_counted_past_their_array_are_refused_and_the_count_kept)
{ MockTransport transport;
  std::uint8_t reply[served_bytes] = {};
  const std::uint32_t counts[] = {VK_MAX_MEMORY_TYPES + 1u, VK_MAX_MEMORY_TYPES};
  for (const std::uint32_t types : counts) {
      VkPhysicalDeviceMemoryProperties host = {};
      host.memoryTypeCount = types;
      host.memoryHeapCount = 1u;
      WireWriter writer(reply, sizeof reply);
      writer.write_pod<std::int32_t>(0);
      av::vkproto::serialize_VkPhysicalDeviceMemoryProperties(writer, host);
      transport.set_reply(reply, writer.offset());
      VkPhysicalDeviceMemoryProperties properties = {};
      properties.memoryHeapCount = 2u;
      av::vkproto::rvk_vkGetPhysicalDeviceMemoryProperties(transport, physical_device(), &properties);
      const bool fits = types <= VK_MAX_MEMORY_TYPES;
      CHECK_EQ(properties.memoryTypeCount, fits ? types : 0u);
      CHECK_EQ(properties.memoryHeapCount, fits ? 1u : 2u);
  } }

/* A name filling every byte of its array is terminated inside it, and the rest
 * of the element is still read. */
TEST(generated_client_replies, vulkan_an_extension_name_filling_its_array_is_terminated_inside_it)
{ MockTransport transport;
  VkExtensionProperties host = {};
  std::memset(host.extensionName, 'a', sizeof host.extensionName);
  host.specVersion = 7u;
  std::uint8_t reply[served_bytes] = {};
  WireWriter   writer(reply, sizeof reply);
  writer.write_pod<std::int32_t>(0);
  writer.write_pod<std::uint32_t>(1u);
  av::vkproto::serialize_VkExtensionProperties(writer, host);
  transport.set_reply(reply, writer.offset());
  Destination extensions(sizeof(VkExtensionProperties));
  std::uint32_t count = 1u;
  CHECK(av::vkproto::rvk_vkEnumerateDeviceExtensionProperties(transport, physical_device(), nullptr, &count,
                                                              extensions.as<VkExtensionProperties>()) == VK_SUCCESS);
  CHECK_EQ(count, 1u);
  const VkExtensionProperties &read = *extensions.as<VkExtensionProperties>();
  CHECK_EQ(read.extensionName[VK_MAX_EXTENSION_NAME_SIZE - 2u], 'a');
  CHECK_EQ(read.extensionName[VK_MAX_EXTENSION_NAME_SIZE - 1u], '\0');
  CHECK_EQ(read.specVersion, 7u);
  CHECK(extensions.untouched_from(sizeof(VkExtensionProperties))); }
