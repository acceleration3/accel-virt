/* rvkvkprobe's proof over a scripted Vulkan (feature 045, T037): the adapter's
 * LUID and the physical device's are the same; a device on it doubles 256
 * elements read back through vkMapMemory and draws a 64x64 image read back
 * through a host-visible buffer; a forced fault answers VK_ERROR_DEVICE_LOST
 * and a new instance does the compute again. Each step's line is what
 * tools/guest/run_guest_suite.py reads; a failure at every call is reported
 * and leaves no object behind, and nothing is destroyed twice. */
#include "core/support/status.hpp"
#include "guest/kmt/pci_identity.hpp"
#include "guest/suite/adapter_scenario.hpp"
#include "guest/suite/guest_report.hpp"
#include "guest/suite/probe_device.hpp"
#include "guest/suite/shaders/double_spv.hpp"
#include "guest/suite/shaders/solid_spv.hpp"
#include "guest/suite/shaders/triangle_spv.hpp"
#include "guest/suite/vulkan_probe.hpp"
#include "harness/guest/scripted_image_sink.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "harness/guest/scripted_vulkan.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>

using av::AdapterScenario;
using av::GuestReport;
using av::MockKmt;
using av::PciIdentity;
using av::ScriptedVulkan;
using av::VulkanProbe;
using Call = av::ScriptedVulkan::Call;

namespace {

const PciIdentity rvk(AdapterScenario::virtio_vendor, AdapterScenario::virtio_gpu_device,
                      AdapterScenario::virtio_vendor, AdapterScenario::rvk_subsystem);
/* LowPart 0x1234, HighPart 2: a HighPart the report must not print. */
constexpr std::uint64_t adapter_luid = 0x0000000200001234ull;

class ProbeRig {
public:
    ProbeRig() : report_("rvkvkprobe")
    {
        kmt_.add_adapter(rvk, AdapterScenario::wddm_3_2);
        kmt_.set_adapter_luid(0u, adapter_luid);
        vulkan_.set_luid(0u, adapter_luid, true);
    }
    ~ProbeRig()
    {
        CHECK_EQ(vulkan_.live(), 0u);
        CHECK_EQ(vulkan_.misuses(), 0u);
        CHECK_EQ(kmt_.open_objects(), 0u);
        CHECK_EQ(kmt_.misuses(), 0u);
    }

    MockKmt        &kmt() { return kmt_; }
    ScriptedVulkan &vulkan() { return vulkan_; }
    GuestReport    &report() { return report_; }
    const av::ScriptedImageSink &images() const { return images_; }

    void run()
    {
        VulkanProbe probe(kmt_, report_, vulkan_, images_);
        probe.run();
        report_.finish();
    }

    bool reads(const char *text) const { return std::strcmp(report_.text(), text) == 0; }
    bool holds(const char *line) const { return std::strstr(report_.text(), line) != nullptr; }
    void print() const { std::fprintf(stderr, "%s", report_.text()); }

private:
    MockKmt        kmt_;
    ScriptedVulkan vulkan_;
    av::ScriptedImageSink images_;
    GuestReport    report_;
};

}  // namespace

/* The whole proof, as the runner reads it: every step once, the LUID's low
 * part in decimal twice, 256 elements and 4096 pixels. */
TEST(vulkan_probe, every_step_passes_on_a_working_gpu)
{ ProbeRig rig;
  rig.run();
  CHECK(rig.reads("ok vulkan.instance\n"
                  "ok vulkan.adapter_luid 4660\n"
                  "ok vulkan.physical_device 4660\n"
                  "ok vulkan.device\n"
                  "ok vulkan.compute 256\n"
                  "ok vulkan.draw 4096\n"
                  "ok vulkan.lost\n"
                  "ok vulkan.recreated 256\n"
                  "rvkvkprobe: 8 passed, 0 failed\n"));
  CHECK_EQ(rig.report().failed(), 0u);
  CHECK_EQ(rig.vulkan().instance_api_version(), (std::uint32_t{1} << 22u) | (std::uint32_t{3} << 12u));
  CHECK_EQ(rig.vulkan().calls(Call::create_instance), 2u);
  CHECK_EQ(rig.vulkan().calls(Call::force_context_fault), 1u);
  CHECK_EQ(rig.vulkan().dispatched_groups(), VulkanProbe::elements / 16u);
  CHECK_EQ(rig.vulkan().drawn_vertices(), 3u);
  CHECK_EQ(rig.vulkan().shader_words(),
           2u * av::double_spv_words + av::triangle_spv_words + av::solid_spv_words);
  /* The compute buffer and the readback in the host-visible type, the image
   * in the device-local one. */
  CHECK_EQ(rig.vulkan().allocated_type(0u), 1u);
  CHECK_EQ(rig.vulkan().allocated_type(1u), 0u);
  CHECK_EQ(rig.vulkan().allocated_type(2u), 1u);
  CHECK_EQ(rig.vulkan().allocated_type(3u), 1u);
  if (rig.report().failed() != 0u) rig.print(); }

/* The device used is the one whose valid LUID is the adapter's, wherever it
 * is listed: an invalid LUID of the same value does not count. */
TEST(vulkan_probe, the_adapter_s_device_is_chosen_by_its_luid)
{ ProbeRig rig;
  rig.vulkan().set_physical_devices(3u);
  rig.vulkan().set_luid(0u, adapter_luid, false);
  rig.vulkan().set_luid(1u, adapter_luid + 1u, true);
  rig.vulkan().set_luid(2u, adapter_luid, true);
  rig.run();
  CHECK_EQ(rig.report().failed(), 0u);
  CHECK_EQ(rig.vulkan().device_parent(), 2u);
  CHECK(rig.holds("ok vulkan.physical_device 4660\n")); }

/* A HighPart that differs is a different adapter, whatever the LowPart. */
TEST(vulkan_probe, a_luid_differing_only_in_its_high_part_is_not_the_adapter)
{ ProbeRig rig;
  rig.vulkan().set_luid(0u, adapter_luid + (std::uint64_t{1} << 32u), true);
  rig.run();
  CHECK(rig.reads("ok vulkan.instance\n"
                  "ok vulkan.adapter_luid 4660\n"
                  "FAIL vulkan.physical_device status=3 1\n"
                  "rvkvkprobe: 2 passed, 1 failed\n")); }

/* No listed device is the adapter's: the count is what is reported, and
 * nothing is made on any of them. */
TEST(vulkan_probe, no_device_with_the_adapter_s_luid_ends_the_run)
{ ProbeRig rig;
  rig.vulkan().set_physical_devices(4u);
  rig.vulkan().set_luid(0u, 7u, true);
  rig.run();
  CHECK(rig.holds("FAIL vulkan.physical_device status=3 4\n"));
  CHECK_EQ(rig.vulkan().calls(Call::create_device), 0u);
  CHECK_EQ(rig.report().failed(), 1u);
  CHECK_EQ(rig.report().passed(), 2u); }

/* More devices than the probe looks at: the first ones listed are looked
 * at, and VK_INCOMPLETE is not a failure; one listed past them is not seen. */
TEST(vulkan_probe, an_incomplete_list_is_searched_as_far_as_it_goes)
{ ProbeRig rig;
  const std::uint32_t looked_at = av::ProbeDevice::max_physical_devices;
  rig.vulkan().set_physical_devices(ScriptedVulkan::max_physical_devices);
  rig.vulkan().set_luid(0u, 7u, true);
  rig.vulkan().set_luid(looked_at - 1u, adapter_luid, true);
  rig.run();
  CHECK_EQ(rig.report().failed(), 0u);
  CHECK_EQ(rig.vulkan().device_parent(), looked_at - 1u); }

TEST(vulkan_probe, a_device_listed_past_the_room_is_not_seen)
{ ProbeRig rig;
  rig.vulkan().set_physical_devices(ScriptedVulkan::max_physical_devices);
  rig.vulkan().set_luid(0u, 7u, true);
  rig.vulkan().set_luid(av::ProbeDevice::max_physical_devices, adapter_luid, true);
  rig.run();
  CHECK(rig.holds("FAIL vulkan.physical_device status=3 16\n")); }

/* With no rvkgpu adapter there is no LUID to look for: the run ends there. */
TEST(vulkan_probe, no_adapter_ends_the_run)
{ MockKmt kmt;
  ScriptedVulkan vulkan;
  GuestReport report("rvkvkprobe");
  av::ScriptedImageSink images;
  { VulkanProbe probe(kmt, report, vulkan, images);
    probe.run(); }
  report.finish();
  CHECK(std::strcmp(report.text(), "ok vulkan.instance\n"
                                   "FAIL vulkan.adapter_luid status=3 0\n"
                                   "rvkvkprobe: 1 passed, 1 failed\n") == 0);
  CHECK_EQ(vulkan.live(), 0u);
  CHECK_EQ(vulkan.calls(Call::enumerate_physical_devices), 0u);
  CHECK_EQ(kmt.open_objects(), 0u); }

/* The adapter found but its LUID not answered. */
TEST(vulkan_probe, an_unanswered_luid_ends_the_run)
{ ProbeRig rig;
  rig.kmt().fail_at(MockKmt::Call::adapter_luid, 1u, 0xC0000001u);
  rig.run();
  CHECK(rig.holds("FAIL vulkan.adapter_luid status="));
  CHECK_EQ(rig.report().failed(), 1u);
  CHECK_EQ(rig.vulkan().calls(Call::enumerate_physical_devices), 0u); }

/* No instance, nothing else. */
TEST(vulkan_probe, a_refused_instance_is_the_only_line)
{ ProbeRig rig;
  rig.vulkan().fail(Call::create_instance, 1u, VK_ERROR_INCOMPATIBLE_DRIVER);
  rig.run();
  CHECK(rig.reads("FAIL vulkan.instance status=7 4294967287\n"
                  "rvkvkprobe: 0 passed, 1 failed\n")); }

/* No family both draws and computes. */
TEST(vulkan_probe, no_queue_family_for_both_fails_the_device)
{ ProbeRig rig;
  rig.vulkan().set_queue_families(2u, VK_QUEUE_COMPUTE_BIT);
  rig.run();
  CHECK(rig.holds("FAIL vulkan.device status=3 0\n"));
  CHECK_EQ(rig.report().failed(), 1u);
  CHECK_EQ(rig.vulkan().calls(Call::create_device), 0u); }

/* A device listing no family at all. */
TEST(vulkan_probe, no_queue_family_at_all_fails_the_device)
{ ProbeRig rig;
  rig.vulkan().set_queue_families(0u, VK_QUEUE_COMPUTE_BIT);
  rig.run();
  CHECK(rig.holds("FAIL vulkan.device status=3 0\n")); }

/* A compute result off by one element is a failed compute, and a failed
 * recreated run, each saying how many were right. */
TEST(vulkan_probe, a_wrong_element_fails_the_compute)
{ ProbeRig rig;
  rig.vulkan().miscompute();
  rig.run();
  CHECK(rig.holds("FAIL vulkan.compute status=4 255\n"));
  CHECK(rig.holds("ok vulkan.draw 4096\n"));
  CHECK(rig.holds("ok vulkan.lost\n"));
  CHECK(rig.holds("FAIL vulkan.recreated status=4 255\n"));
  CHECK_EQ(rig.report().failed(), 2u); }

/* One pixel short of the colour fails the draw. */
TEST(vulkan_probe, a_wrong_pixel_fails_the_draw)
{ ProbeRig rig;
  rig.vulkan().misdraw();
  rig.run();
  CHECK(rig.holds("FAIL vulkan.draw status=4 4095\n"));
  CHECK_EQ(rig.report().failed(), 1u); }

/* Memory no type of which the host can map fails the work that reads it, and
 * the image falls back to the host-visible type when it may not be local. */
TEST(vulkan_probe, no_host_visible_type_for_a_buffer_fails_the_work)
{ ProbeRig rig;
  rig.vulkan().set_allowed_types(0x1u, 0x2u);
  rig.run();
  CHECK(rig.holds("FAIL vulkan.compute status=3 0\n"));
  CHECK(rig.holds("FAIL vulkan.draw status=3 0\n"));
  CHECK(rig.holds("ok vulkan.lost\n"));
  CHECK(rig.holds("FAIL vulkan.recreated status=3 0\n"));
  CHECK_EQ(rig.vulkan().allocated_type(0u), 1u); }

/* A type the host can map but that the buffer may not use does not count. */
TEST(vulkan_probe, a_host_visible_type_the_buffer_cannot_use_is_not_chosen)
{ ProbeRig rig;
  rig.vulkan().set_memory_types(3u);
  rig.vulkan().set_memory_type(2u, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
  rig.vulkan().set_allowed_types(0x4u, 0x1u);
  rig.run();
  CHECK_EQ(rig.report().failed(), 0u);
  CHECK_EQ(rig.vulkan().allocated_type(0u), 2u);
  CHECK_EQ(rig.vulkan().allocated_type(1u), 0u); }

/* An image allowed no type at all. */
TEST(vulkan_probe, an_image_with_no_type_fails_the_draw)
{ ProbeRig rig;
  rig.vulkan().set_allowed_types(0x3u, 0x0u);
  rig.run();
  CHECK(rig.holds("FAIL vulkan.draw status=3 0\n"));
  CHECK_EQ(rig.report().failed(), 1u); }

/* The loss must be answered by the fault and by the next call alike. */
TEST(vulkan_probe, a_fault_that_is_not_a_loss_fails_the_step)
{ ProbeRig rig;
  rig.vulkan().answer_fault(VK_SUCCESS, VK_ERROR_DEVICE_LOST);
  rig.run();
  CHECK(rig.holds("FAIL vulkan.lost status=4 0\n"));
  CHECK(rig.holds("ok vulkan.recreated 256\n"));
  CHECK_EQ(rig.report().failed(), 1u); }

TEST(vulkan_probe, a_loader_without_the_fault_fails_the_step)
{ ProbeRig rig;
  rig.vulkan().answer_fault(VK_ERROR_EXTENSION_NOT_PRESENT, VK_ERROR_DEVICE_LOST);
  rig.run();
  CHECK(rig.holds("FAIL vulkan.lost status=4 4294967289\n")); }

TEST(vulkan_probe, a_device_still_working_after_the_fault_fails_the_step)
{ ProbeRig rig;
  rig.vulkan().answer_fault(VK_ERROR_DEVICE_LOST, VK_SUCCESS);
  rig.run();
  CHECK(rig.holds("FAIL vulkan.lost status=4 0\n"));
  CHECK_EQ(rig.report().failed(), 1u); }

/* A fence that never signals is a failed step with VK_TIMEOUT's code. */
TEST(vulkan_probe, a_wait_that_times_out_fails_the_work)
{ ProbeRig rig;
  rig.vulkan().fail(Call::wait_for_fence, 1u, VK_TIMEOUT);
  rig.run();
  CHECK(rig.holds("FAIL vulkan.compute status=7 2\n"));
  CHECK(rig.holds("ok vulkan.draw 4096\n")); }

/* Every call that can fail, failing at its first and at its second use: the
 * run reports a failure, and nothing is left made or destroyed twice. */
TEST(vulkan_probe, a_failure_at_any_call_is_reported_and_cleaned_up)
{ for (std::uint32_t call = 0u; call < static_cast<std::uint32_t>(Call::count); call++) {
      for (std::uint32_t nth = 1u; nth <= 3u; nth++) {
          ProbeRig rig;
          rig.vulkan().fail(static_cast<Call>(call), nth, VK_ERROR_OUT_OF_HOST_MEMORY);
          rig.run();
          const bool used = rig.vulkan().calls(static_cast<Call>(call)) >= nth;
          if (!used) continue;
          if (rig.report().failed() == 0u) {
              std::fprintf(stderr, "call %u use %u failed nothing\n", call, nth);
              rig.print();
          }
          CHECK(rig.report().failed() != 0u);
      }
  } }

/* The recreated device is looked for as the first was: gone from the list, or
 * with no family that draws and computes, it is missing. */
TEST(vulkan_probe, a_recreated_run_without_the_adapter_s_device_is_missing)
{ ProbeRig rig;
  rig.vulkan().change_after_fault(std::uint64_t{1} << 40u, VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT);
  rig.run();
  CHECK(rig.holds("ok vulkan.lost\n"));
  CHECK(rig.holds("FAIL vulkan.recreated status=3 0\n"));
  CHECK_EQ(rig.vulkan().calls(Call::create_device), 1u); }

TEST(vulkan_probe, a_recreated_run_without_a_family_is_missing)
{ ProbeRig rig;
  rig.vulkan().change_after_fault(0u, VK_QUEUE_TRANSFER_BIT);
  rig.run();
  CHECK(rig.holds("FAIL vulkan.recreated status=3 0\n"));
  CHECK_EQ(rig.vulkan().calls(Call::create_device), 1u); }
