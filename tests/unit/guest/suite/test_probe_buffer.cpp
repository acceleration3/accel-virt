/* A buffer the probe maps, over a scripted Vulkan: made, bound and mapped in
 * the lowest host-visible type it may use; a failure at each call stops there
 * and is the one the outcome keeps; no usable type is a missing one; and
 * whatever was made is destroyed with its memory. */
#include "guest/suite/probe_buffer.hpp"
#include "guest/suite/probe_device.hpp"
#include "guest/suite/probe_outcome.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/guest/scripted_vulkan.hpp"

#include <cstdint>

namespace av {

struct ProbeBufferProbe {
    static void mapped_without_memory(ProbeBuffer &buffer) { buffer.memory_ = VK_NULL_HANDLE; }
    static void memory_without_buffer(ProbeBuffer &buffer) { buffer.buffer_ = VK_NULL_HANDLE; buffer.bytes_ = nullptr; }
};

}  // namespace av

using av::ProbeBuffer;
using av::ProbeBufferProbe;
using av::ProbeDevice;
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

TEST(probe_buffer, a_buffer_is_mapped_in_a_host_visible_type)
{ ScriptedVulkan vulkan;
  { ProbeDevice device(vulkan);
    make(device);
    ProbeBuffer buffer(device);
    CHECK(buffer.invariants_hold());
    CHECK(buffer.bytes() == nullptr);
    ProbeOutcome outcome;
    CHECK(buffer.make(1024u, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, outcome));
    CHECK(buffer.invariants_hold());
    CHECK(outcome.invariants_hold());
    CHECK(buffer.buffer() != VK_NULL_HANDLE);
    CHECK(buffer.bytes() != nullptr);
    buffer.bytes()[1023] = 0x5A;
    CHECK_EQ(vulkan.allocated_type(0u), 1u); }
  CHECK_EQ(vulkan.live(), 0u);
  CHECK_EQ(vulkan.misuses(), 0u); }

TEST(probe_buffer, no_host_visible_type_is_missing)
{ ScriptedVulkan vulkan;
  vulkan.set_allowed_types(0x1u, 0x1u);
  { ProbeDevice device(vulkan);
    make(device);
    ProbeBuffer buffer(device);
    ProbeOutcome outcome;
    CHECK(!buffer.make(1024u, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, outcome));
    CHECK(buffer.invariants_hold());
    CHECK(buffer.bytes() == nullptr);
    CHECK_EQ(vulkan.calls(Call::allocate_memory), 0u); }
  CHECK_EQ(vulkan.live(), 0u);
  CHECK_EQ(vulkan.misuses(), 0u); }

/* Each call refused in turn: nothing after it is made, and nothing is left. */
TEST(probe_buffer, a_refusal_at_each_call_stops_there)
{ const Call calls[] = {Call::create_buffer, Call::allocate_memory, Call::bind_buffer_memory, Call::map_memory};
  for (const Call call : calls) {
      ScriptedVulkan vulkan;
      vulkan.fail(call, 1u, VK_ERROR_OUT_OF_DEVICE_MEMORY);
      { ProbeDevice device(vulkan);
        make(device);
        ProbeBuffer buffer(device);
        ProbeOutcome outcome;
        CHECK(!buffer.make(4096u, VK_BUFFER_USAGE_TRANSFER_DST_BIT, outcome));
        CHECK(buffer.invariants_hold());
        CHECK(buffer.bytes() == nullptr);
        CHECK_EQ(vulkan.calls(Call::map_memory), call == Call::map_memory ? 1u : 0u); }
      CHECK_EQ(vulkan.live(), 0u);
      CHECK_EQ(vulkan.misuses(), 0u);
  } }

TEST(probe_buffer, invariants_refuse_a_mapping_or_memory_without_its_owner)
{ ScriptedVulkan vulkan;
  ProbeDevice device(vulkan);
  make(device);
  ProbeBuffer mapped(device);
  ProbeOutcome outcome;
  CHECK(mapped.make(1024u, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, outcome));
  const VkBuffer buffer = mapped.buffer();
  ProbeBufferProbe::memory_without_buffer(mapped);
  CHECK(!mapped.invariants_hold());
  vulkan.destroy_buffer(device.device(), buffer);
  ProbeBuffer other(device);
  CHECK(other.make(1024u, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, outcome));
  ProbeBufferProbe::mapped_without_memory(other);
  CHECK(!other.invariants_hold()); }
