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
#include "harness/guest/scripted_window_frame.hpp"

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

/* The probe's window, as the guest's is: 64x64, on the scripted desktop. */
constexpr std::uint32_t window_side = VulkanProbe::extent;
void *const window_handle = reinterpret_cast<void *>(0x7770u);
void *const module_handle = reinterpret_cast<void *>(0x7771u);

class ProbeRig {
public:
    ProbeRig() : report_("rvkvkprobe")
    {
        kmt_.add_adapter(rvk, AdapterScenario::wddm_3_2);
        kmt_.set_adapter_luid(0u, adapter_luid);
        vulkan_.set_luid(0u, adapter_luid, true);
        frames_.set_window(window_handle, window_side, window_side);
        vulkan_.present_into(frames_);
        /* The scripted GPU paces its presents on the mock adapter, as the
         * driver's FIFO pacing does on the real one. */
        std::uint32_t adapters[2] = {};
        std::uint32_t found = 0u;
        CHECK_STATUS(kmt_.enumerate_adapters(adapters, 2u, found), av::Status::ok);
        adapter_ = adapters[0];
        CHECK_STATUS(kmt_.create_device(adapter_, device_), av::Status::ok);
        vulkan_.pace_on(kmt_, adapter_, device_);
    }
    ~ProbeRig()
    {
        CHECK_STATUS(kmt_.destroy_device(device_), av::Status::ok);
        CHECK_STATUS(kmt_.close_adapter(adapter_), av::Status::ok);
        CHECK_EQ(vulkan_.live(), 0u);
        CHECK_EQ(vulkan_.misuses(), 0u);
        CHECK_EQ(kmt_.open_objects(), 0u);
        CHECK_EQ(kmt_.misuses(), 0u);
    }

    MockKmt             &kmt() { return kmt_; }
    ScriptedVulkan      &vulkan() { return vulkan_; }
    av::ScriptedWindowFrame &frames() { return frames_; }
    GuestReport    &report() { return report_; }
    const av::ScriptedImageSink &images() const { return images_; }

    void run()
    {
        VulkanProbe probe(kmt_, report_, vulkan_, images_, frames_, module_handle, window_handle);
        probe.run();
        report_.finish();
    }

    /* Presents that wait for nothing: a driver that does not pace FIFO. */
    void unpace() { vulkan_.present_unpaced(); }

    /* A run with no window at all: the window-system steps do not happen. */
    void run_without_a_window()
    {
        VulkanProbe probe(kmt_, report_, vulkan_, images_, frames_, module_handle, nullptr);
        probe.run();
        report_.finish();
    }

    bool reads(const char *text) const { return std::strcmp(report_.text(), text) == 0; }
    bool holds(const char *line) const { return std::strstr(report_.text(), line) != nullptr; }
    void print() const { std::fprintf(stderr, "%s", report_.text()); }

private:
    MockKmt                 kmt_;
    ScriptedVulkan          vulkan_;
    av::ScriptedImageSink   images_;
    av::ScriptedWindowFrame frames_;
    GuestReport             report_;
    std::uint32_t           adapter_ = 0u;
    std::uint32_t           device_ = 0u;
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
                  "ok vulkan.surface 4096\n"
                  "ok vulkan.present 4096\n"
                  "ok vulkan.pacing 60\n"
                  "ok vulkan.resized 9216\n"
                  "rvkvkprobe: 12 passed, 0 failed\n"));
  CHECK_EQ(rig.report().failed(), 0u);
  CHECK_EQ(rig.vulkan().instance_api_version(), (std::uint32_t{1} << 22u) | (std::uint32_t{3} << 12u));
  /* Two instances: the first run's and the recreated one's, which the window
   * steps run on. */
  CHECK_EQ(rig.vulkan().calls(Call::create_instance), 2u);
  CHECK_EQ(rig.vulkan().presents(), VulkanProbe::frames + 1u);
  CHECK_EQ(rig.frames().reads(), 2u);
  CHECK_EQ(rig.frames().resizes(), 1u);
  CHECK_EQ(rig.vulkan().calls(Call::force_context_fault), 1u);
  CHECK_EQ(rig.vulkan().dispatched_groups(), VulkanProbe::elements / 16u);
  CHECK_EQ(rig.vulkan().drawn_vertices(), 3u);
  /* The draw step's shaders, the present steps' two swapchains' and the
   * compute's twice. */
  CHECK_EQ(rig.vulkan().shader_words(),
           2u * av::double_spv_words + 3u * (av::triangle_spv_words + av::solid_spv_words));
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
  av::ScriptedWindowFrame frames;
  { VulkanProbe probe(kmt, report, vulkan, images, frames, nullptr, nullptr);
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
  /* The same missing pixel is missing from the window the present put it on. */
  CHECK(rig.holds("FAIL vulkan.present status=4 4095\n"));
  CHECK_EQ(rig.report().failed(), 2u); }

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

/* Every window-system step, and what makes each of them fail: no surface, a
 * window that shows something other than the frame, a present the driver did
 * not pace, and a resize the swapchain did not notice. */
TEST(vulkan_probe, a_surface_the_driver_refuses_ends_the_window_steps)
{ ProbeRig rig;
  rig.vulkan().fail(Call::create_win32_surface, 1u, VK_ERROR_NATIVE_WINDOW_IN_USE_KHR);
  rig.run();
  CHECK(rig.holds("FAIL vulkan.surface status=7"));
  CHECK(!rig.holds("vulkan.present"));
  CHECK_EQ(rig.report().failed(), 1u); }

/* The surface must describe the window it was made for: an extent that is not
 * the window's is a driver that cannot be presented through. */
TEST(vulkan_probe, a_surface_of_another_size_than_the_window_fails_the_step)
{ ProbeRig rig;
  rig.vulkan().hold_the_surface_extent(window_side / 2u, window_side);
  rig.run();
  CHECK(rig.holds("FAIL vulkan.surface status=3 0"));
  CHECK_EQ(rig.report().failed(), 1u); }

TEST(vulkan_probe, a_swapchain_the_driver_refuses_fails_the_present_step)
{ ProbeRig rig;
  rig.vulkan().fail(Call::create_swapchain, 1u, VK_ERROR_OUT_OF_DEVICE_MEMORY);
  rig.run();
  CHECK(rig.holds("FAIL vulkan.present status=7"));
  CHECK(!rig.holds("vulkan.resized"));
  CHECK_EQ(rig.report().failed(), 1u); }

/* The window is what the step judges: a frame that did not arrive whole is a
 * failure however well the presents themselves went. */
TEST(vulkan_probe, a_window_missing_a_pixel_fails_the_present_step)
{ ProbeRig rig;
  rig.vulkan().misdraw();
  rig.run();
  CHECK(rig.holds("FAIL vulkan.present status=4 4095"));
  CHECK_EQ(rig.vulkan().presents(), VulkanProbe::frames); }

TEST(vulkan_probe, a_window_that_cannot_be_read_fails_the_present_step)
{ ProbeRig rig;
  rig.frames().refuse_read();
  rig.run();
  CHECK(rig.holds("FAIL vulkan.present status=3 0"));
  CHECK_EQ(rig.frames().reads(), 0u); }

/* Unpaced presents: sixty frames in less time than sixty vertical blanks. */
TEST(vulkan_probe, frames_the_driver_did_not_pace_fail_the_pacing_step)
{ ProbeRig rig;
  rig.unpace();
  rig.run();
  CHECK(rig.holds("FAIL vulkan.pacing status=4 0"));
  CHECK(rig.holds("ok vulkan.present 4096")); }

/* The pacing step times a blank on an adapter and device of its own: without
 * one there is nothing to compare sixty frames against, and the step measures
 * nothing rather than judging the driver on a period it never had. */
TEST(vulkan_probe, a_pacing_step_that_cannot_find_the_adapter_measures_nothing)
{ ProbeRig rig;
  /* The probe's LUID step enumerates first and the pacing step second: it is
   * the second of the two that must find nothing. */
  rig.kmt().fail_at(MockKmt::Call::enumerate_adapters, 2u, 0xC0000001u);
  rig.run();
  CHECK(rig.holds("ok vulkan.present 4096\n"));
  CHECK(rig.holds("FAIL vulkan.pacing status=3 0\n"));
  /* The steps after it are unharmed: only the pacing needed the adapter. */
  CHECK(rig.holds("ok vulkan.resized 9216\n"));
  CHECK_EQ(rig.report().failed(), 1u); }

TEST(vulkan_probe, a_pacing_step_without_a_device_measures_nothing)
{ ProbeRig rig;
  /* The rig made its own device already; the next one is the pacing step's. */
  rig.kmt().fail_at(MockKmt::Call::create_device, 1u, 0xC0000001u);
  rig.run();
  CHECK(rig.holds("FAIL vulkan.pacing status=3 0\n"));
  /* Nothing was waited on: a device that was never made cannot be. */
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::wait_for_vertical_blank), VulkanProbe::frames + 1u);
  CHECK_EQ(rig.report().failed(), 1u); }

/* The period is two blanks apart: neither wait may be refused, and the frames
 * the driver did pace are not judged against a period that was never timed. */
TEST(vulkan_probe, a_blank_the_driver_refuses_measures_no_pacing)
{ ProbeRig rig;
  rig.kmt().fail_at(MockKmt::Call::wait_for_vertical_blank, VulkanProbe::frames + 1u, 0xC0000001u);
  rig.run();
  CHECK(rig.holds("FAIL vulkan.pacing status=3 0\n"));
  /* The second wait is never asked for once the first was refused. */
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::wait_for_vertical_blank), VulkanProbe::frames + 2u); }

TEST(vulkan_probe, a_second_blank_that_never_comes_measures_no_pacing)
{ ProbeRig rig;
  rig.kmt().fail_at(MockKmt::Call::wait_for_vertical_blank, VulkanProbe::frames + 2u, 0xC0000001u);
  rig.run();
  CHECK(rig.holds("FAIL vulkan.pacing status=3 0\n"));
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::wait_for_vertical_blank), VulkanProbe::frames + 3u); }

/* A resized window: the swapchain is out of date until the application makes
 * another, and the new one presents at the new size. */
TEST(vulkan_probe, a_swapchain_that_ignores_a_resize_fails_the_resized_step)
{ ProbeRig rig;
  rig.frames().refuse_resize();
  rig.run();
  CHECK(rig.holds("FAIL vulkan.resized status=3 0"));
  CHECK(rig.holds("ok vulkan.present 4096")); }

/* The resized window must be what the swapchain notices: any other answer to
 * the stale present is reported as the one the driver gave. */
TEST(vulkan_probe, a_stale_present_that_is_not_out_of_date_fails_the_resized_step)
{ ProbeRig rig;
  rig.vulkan().fail(Call::acquire_next_image, VulkanProbe::frames + 1u, VK_ERROR_SURFACE_LOST_KHR);
  rig.run();
  CHECK(rig.holds("FAIL vulkan.resized status=4 3294967296\n"));
  CHECK(rig.holds("ok vulkan.present 4096\n"));
  /* No second swapchain is made for a window whose loss was not a resize. */
  CHECK_EQ(rig.vulkan().calls(Call::create_swapchain), 1u); }

/* The swapchain made for the new size is the step's whole point: it must be
 * made, and it must present. */
TEST(vulkan_probe, a_swapchain_the_resize_cannot_replace_fails_the_resized_step)
{ ProbeRig rig;
  rig.vulkan().fail(Call::create_swapchain, 2u, VK_ERROR_OUT_OF_DEVICE_MEMORY);
  rig.run();
  CHECK(rig.holds("FAIL vulkan.resized status=7 4294967294\n"));
  CHECK(rig.holds("ok vulkan.present 4096\n"));
  /* The window was never read again: there was no frame at the new size. */
  CHECK_EQ(rig.frames().reads(), 1u); }

TEST(vulkan_probe, a_frame_the_new_swapchain_cannot_present_fails_the_resized_step)
{ ProbeRig rig;
  rig.vulkan().fail(Call::queue_present, VulkanProbe::frames + 1u, VK_ERROR_DEVICE_LOST);
  rig.run();
  CHECK(rig.holds("FAIL vulkan.resized status=7 4294967292\n"));
  CHECK_EQ(rig.vulkan().calls(Call::create_swapchain), 2u);
  CHECK_EQ(rig.frames().reads(), 1u); }

/* With no window there are no window-system steps, and the rest of the probe
 * is unchanged: a probe on a machine with no desktop still proves the GPU. */
TEST(vulkan_probe, no_window_means_no_window_steps)
{ ProbeRig rig;
  rig.run_without_a_window();
  CHECK(rig.holds("ok vulkan.recreated 256"));
  CHECK(!rig.holds("vulkan.surface"));
  CHECK(!rig.holds("vulkan.present"));
  CHECK_EQ(rig.report().failed(), 0u); }
