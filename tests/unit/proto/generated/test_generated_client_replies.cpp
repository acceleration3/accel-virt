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
#include "proto/context/sub_api.hpp"
#include "proto/generated/opencl/cl_dispatch_client.h"
#include "proto/generated/vulkan/vk_dispatch_client.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

using av::AdapterScenario;
using av::AdapterSearch;
using av::GuardedBuffer;
using av::GuestReport;
using av::MockKmt;
using av::PciIdentity;
using av::ScenarioSteps;
using av::ScenarioTransport;
using av::ScriptedHost;
using av::Status;
using av::SubApi;

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
