/* The probe's render target over a scripted Vulkan: device-local memory when
 * the image may use it, else the lowest type it may; a view of it; a refusal
 * at each call stops there; and everything made is destroyed. */
#include "guest/suite/probe_device.hpp"
#include "guest/suite/probe_image.hpp"
#include "guest/suite/probe_outcome.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/guest/scripted_vulkan.hpp"

#include <cstdint>

namespace av {

struct ProbeImageProbe {
    static void view_without_memory(ProbeImage &image) { image.memory_ = VK_NULL_HANDLE; }
    static void memory_without_image(ProbeImage &image) { image.image_ = VK_NULL_HANDLE; image.view_ = VK_NULL_HANDLE; }
};

}  // namespace av

using av::ProbeDevice;
using av::ProbeImage;
using av::ProbeImageProbe;
using av::ProbeOutcome;
using av::ScriptedVulkan;
using Call = av::ScriptedVulkan::Call;

namespace {

void make(ProbeDevice &device)
{
    CHECK_EQ(device.make_instance(), VK_SUCCESS);
    CHECK_EQ(device.list_physical_devices(), VK_SUCCESS);
    CHECK(device.choose(1u));
    CHECK(device.choose_family());
    CHECK_EQ(device.make_device(), VK_SUCCESS);
}

}  // namespace

TEST(probe_image, an_image_prefers_device_local_memory)
{ ScriptedVulkan vulkan;
  vulkan.set_memory_types(3u);
  vulkan.set_memory_type(0u, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
  vulkan.set_memory_type(1u, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
  vulkan.set_memory_type(2u, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  vulkan.set_allowed_types(0x7u, 0x7u);
  { ProbeDevice device(vulkan);
    make(device);
    ProbeImage image(device);
    CHECK(image.invariants_hold());
    ProbeOutcome outcome;
    CHECK(image.make(64u, outcome));
    CHECK(image.invariants_hold());
    CHECK(image.image() != VK_NULL_HANDLE);
    CHECK(image.view() != VK_NULL_HANDLE);
    CHECK_EQ(vulkan.allocated_type(0u), 2u); }
  CHECK_EQ(vulkan.live(), 0u);
  CHECK_EQ(vulkan.misuses(), 0u); }

TEST(probe_image, without_device_local_memory_the_lowest_allowed_type_is_used)
{ ScriptedVulkan vulkan;
  vulkan.set_allowed_types(0x3u, 0x2u);
  { ProbeDevice device(vulkan);
    make(device);
    ProbeImage image(device);
    ProbeOutcome outcome;
    CHECK(image.make(64u, outcome));
    CHECK_EQ(vulkan.allocated_type(0u), 1u); }
  CHECK_EQ(vulkan.live(), 0u); }

TEST(probe_image, no_allowed_type_is_missing)
{ ScriptedVulkan vulkan;
  vulkan.set_allowed_types(0x3u, 0x0u);
  { ProbeDevice device(vulkan);
    make(device);
    ProbeImage image(device);
    ProbeOutcome outcome;
    CHECK(!image.make(64u, outcome));
    CHECK(image.view() == VK_NULL_HANDLE);
    CHECK_EQ(vulkan.calls(Call::allocate_memory), 0u); }
  CHECK_EQ(vulkan.live(), 0u);
  CHECK_EQ(vulkan.misuses(), 0u); }

TEST(probe_image, a_refusal_at_each_call_stops_there)
{ const Call calls[] = {Call::create_image, Call::allocate_memory, Call::bind_image_memory, Call::create_image_view};
  for (const Call call : calls) {
      ScriptedVulkan vulkan;
      vulkan.fail(call, 1u, VK_ERROR_OUT_OF_DEVICE_MEMORY);
      { ProbeDevice device(vulkan);
        make(device);
        ProbeImage image(device);
        ProbeOutcome outcome;
        CHECK(!image.make(64u, outcome));
        CHECK(image.invariants_hold());
        CHECK(image.view() == VK_NULL_HANDLE);
        CHECK_EQ(vulkan.calls(Call::create_image_view), call == Call::create_image_view ? 1u : 0u); }
      CHECK_EQ(vulkan.live(), 0u);
      CHECK_EQ(vulkan.misuses(), 0u);
  } }

TEST(probe_image, invariants_refuse_a_view_or_memory_without_its_owner)
{ ScriptedVulkan vulkan;
  ProbeDevice device(vulkan);
  make(device);
  ProbeImage viewed(device);
  ProbeOutcome outcome;
  CHECK(viewed.make(64u, outcome));
  ProbeImageProbe::view_without_memory(viewed);
  CHECK(!viewed.invariants_hold());
  ProbeImage bound(device);
  CHECK(bound.make(64u, outcome));
  const VkImage image = bound.image();
  const VkImageView view = bound.view();
  ProbeImageProbe::memory_without_image(bound);
  CHECK(!bound.invariants_hold());
  vulkan.destroy_image_view(device.device(), view);
  vulkan.destroy_image(device.device(), image); }
