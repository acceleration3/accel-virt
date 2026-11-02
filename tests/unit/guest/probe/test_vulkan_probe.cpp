#include "core/support/status.hpp"
#include "guest/kmt/pci_identity.hpp"
#include "guest/suite/adapter_scenario.hpp"
#include "guest/kmt/adapter_search.hpp"
#include "guest/kmt/avgpu_adapters.hpp"
#include "guest/suite/guest_report.hpp"
#include "guest/probe/probe_device.hpp"
#include "guest/probe/shaders/double_spv.hpp"
#include "guest/probe/shaders/solid_spv.hpp"
#include "guest/probe/shaders/triangle_spv.hpp"
#include "guest/probe/vulkan_probe.hpp"
#include "harness/platform/mock_platform.hpp"
#include "harness/guest/scripted_image_sink.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "harness/guest/scripted_vulkan.hpp"
#include "harness/guest/scripted_window_frame.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>

using av::AdapterScenario;
using av::AdapterSearch;
using av::AvgpuAdapters;
using av::GuestReport;
using av::MockKmt;
using av::PciIdentity;
using av::ScriptedVulkan;
using av::VulkanProbe;
using Call = av::ScriptedVulkan::Call;

namespace {

const PciIdentity av(PciIdentity::virtio_vendor, AvgpuAdapters::virtio_gpu_device,
                      PciIdentity::virtio_vendor, AvgpuAdapters::av_subsystem);
constexpr std::uint64_t adapter_luid = 0x0000000200001234ull;

constexpr std::uint32_t window_side = VulkanProbe::extent;
void *const window_handle = reinterpret_cast<void *>(0x7770u);
void *const module_handle = reinterpret_cast<void *>(0x7771u);

class ProbeRig {
public:
    ProbeRig() : report_(report_memory_, "avvkprobe")
    {
        kmt_.add_adapter(av, AdapterScenario::wddm_3_2);
        kmt_.set_adapter_luid(0u, adapter_luid);
        vulkan_.set_luid(0u, adapter_luid, true);
        frames_.set_window(window_handle, window_side, window_side);
        vulkan_.present_into(frames_);
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

    void unpace() { vulkan_.present_unpaced(); }

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
    av::MockPlatform report_memory_;
    GuestReport             report_;
    std::uint32_t           adapter_ = 0u;
    std::uint32_t           device_ = 0u;
};

}

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
                  "avvkprobe: 12 passed, 0 failed\n"));
  CHECK_EQ(rig.report().failed(), 0u);
  CHECK_EQ(rig.vulkan().instance_api_version(), (std::uint32_t{1} << 22u) | (std::uint32_t{3} << 12u));
  CHECK_EQ(rig.vulkan().calls(Call::create_instance), 2u);
  CHECK_EQ(rig.vulkan().presents(), VulkanProbe::frames + 1u);
  CHECK_EQ(rig.frames().reads(), 2u);
  CHECK_EQ(rig.frames().resizes(), 1u);
  CHECK_EQ(rig.vulkan().calls(Call::force_context_fault), 1u);
  CHECK_EQ(rig.vulkan().dispatched_groups(), VulkanProbe::elements / 16u);
  CHECK_EQ(rig.vulkan().drawn_vertices(), 3u);
  CHECK_EQ(rig.vulkan().shader_words(),
           2u * av::double_spv_words + 3u * (av::triangle_spv_words + av::solid_spv_words));
  CHECK_EQ(rig.vulkan().allocated_type(0u), 1u);
  CHECK_EQ(rig.vulkan().allocated_type(1u), 0u);
  CHECK_EQ(rig.vulkan().allocated_type(2u), 1u);
  CHECK_EQ(rig.vulkan().allocated_type(3u), 1u);
  if (rig.report().failed() != 0u) rig.print(); }

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

TEST(vulkan_probe, a_luid_differing_only_in_its_high_part_is_not_the_adapter)
{ ProbeRig rig;
  rig.vulkan().set_luid(0u, adapter_luid + (std::uint64_t{1} << 32u), true);
  rig.run();
  CHECK(rig.reads("ok vulkan.instance\n"
                  "ok vulkan.adapter_luid 4660\n"
                  "FAIL vulkan.physical_device status=3 1\n"
                  "avvkprobe: 2 passed, 1 failed\n")); }

TEST(vulkan_probe, no_device_with_the_adapter_s_luid_ends_the_run)
{ ProbeRig rig;
  rig.vulkan().set_physical_devices(4u);
  rig.vulkan().set_luid(0u, 7u, true);
  rig.run();
  CHECK(rig.holds("FAIL vulkan.physical_device status=3 4\n"));
  CHECK_EQ(rig.vulkan().calls(Call::create_device), 0u);
  CHECK_EQ(rig.report().failed(), 1u);
  CHECK_EQ(rig.report().passed(), 2u); }

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

TEST(vulkan_probe, no_adapter_ends_the_run)
{ MockKmt kmt;
  ScriptedVulkan vulkan;
  av::MockPlatform report_memory;
  GuestReport report(report_memory, "avvkprobe");
  av::ScriptedImageSink images;
  av::ScriptedWindowFrame frames;
  { VulkanProbe probe(kmt, report, vulkan, images, frames, nullptr, nullptr);
    probe.run(); }
  report.finish();
  CHECK(std::strcmp(report.text(), "ok vulkan.instance\n"
                                   "FAIL vulkan.adapter_luid status=3 0\n"
                                   "avvkprobe: 1 passed, 1 failed\n") == 0);
  CHECK_EQ(vulkan.live(), 0u);
  CHECK_EQ(vulkan.calls(Call::enumerate_physical_devices), 0u);
  CHECK_EQ(kmt.open_objects(), 0u); }

TEST(vulkan_probe, an_unanswered_luid_ends_the_run)
{ ProbeRig rig;
  rig.kmt().fail_at(MockKmt::Call::adapter_luid, 2u, 0xC0000001u);
  rig.run();
  CHECK(rig.holds("FAIL vulkan.adapter_luid status="));
  CHECK_EQ(rig.report().failed(), 1u);
  CHECK_EQ(rig.vulkan().calls(Call::enumerate_physical_devices), 0u); }

TEST(vulkan_probe, a_refused_instance_is_the_only_line)
{ ProbeRig rig;
  rig.vulkan().fail(Call::create_instance, 1u, VK_ERROR_INCOMPATIBLE_DRIVER);
  rig.run();
  CHECK(rig.reads("FAIL vulkan.instance status=7 4294967287\n"
                  "avvkprobe: 0 passed, 1 failed\n")); }

TEST(vulkan_probe, no_queue_family_for_both_fails_the_device)
{ ProbeRig rig;
  rig.vulkan().set_queue_families(2u, VK_QUEUE_COMPUTE_BIT);
  rig.run();
  CHECK(rig.holds("FAIL vulkan.device status=3 0\n"));
  CHECK_EQ(rig.report().failed(), 1u);
  CHECK_EQ(rig.vulkan().calls(Call::create_device), 0u); }

TEST(vulkan_probe, no_queue_family_at_all_fails_the_device)
{ ProbeRig rig;
  rig.vulkan().set_queue_families(0u, VK_QUEUE_COMPUTE_BIT);
  rig.run();
  CHECK(rig.holds("FAIL vulkan.device status=3 0\n")); }

TEST(vulkan_probe, a_wrong_element_fails_the_compute)
{ ProbeRig rig;
  rig.vulkan().miscompute();
  rig.run();
  CHECK(rig.holds("FAIL vulkan.compute status=4 255\n"));
  CHECK(rig.holds("ok vulkan.draw 4096\n"));
  CHECK(rig.holds("ok vulkan.lost\n"));
  CHECK(rig.holds("FAIL vulkan.recreated status=4 255\n"));
  CHECK_EQ(rig.report().failed(), 2u); }

TEST(vulkan_probe, a_wrong_pixel_fails_the_draw)
{ ProbeRig rig;
  rig.vulkan().misdraw();
  rig.run();
  CHECK(rig.holds("FAIL vulkan.draw status=4 4095\n"));
  CHECK(rig.holds("FAIL vulkan.present status=4 4095\n"));
  CHECK_EQ(rig.report().failed(), 2u); }

TEST(vulkan_probe, no_host_visible_type_for_a_buffer_fails_the_work)
{ ProbeRig rig;
  rig.vulkan().set_allowed_types(0x1u, 0x2u);
  rig.run();
  CHECK(rig.holds("FAIL vulkan.compute status=3 0\n"));
  CHECK(rig.holds("FAIL vulkan.draw status=3 0\n"));
  CHECK(rig.holds("ok vulkan.lost\n"));
  CHECK(rig.holds("FAIL vulkan.recreated status=3 0\n"));
  CHECK_EQ(rig.vulkan().allocated_type(0u), 1u); }

TEST(vulkan_probe, a_host_visible_type_the_buffer_cannot_use_is_not_chosen)
{ ProbeRig rig;
  rig.vulkan().set_memory_types(3u);
  rig.vulkan().set_memory_type(2u, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
  rig.vulkan().set_allowed_types(0x4u, 0x1u);
  rig.run();
  CHECK_EQ(rig.report().failed(), 0u);
  CHECK_EQ(rig.vulkan().allocated_type(0u), 2u);
  CHECK_EQ(rig.vulkan().allocated_type(1u), 0u); }

TEST(vulkan_probe, an_image_with_no_type_fails_the_draw)
{ ProbeRig rig;
  rig.vulkan().set_allowed_types(0x3u, 0x0u);
  rig.run();
  CHECK(rig.holds("FAIL vulkan.draw status=3 0\n"));
  CHECK_EQ(rig.report().failed(), 1u); }

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

TEST(vulkan_probe, a_wait_that_times_out_fails_the_work)
{ ProbeRig rig;
  rig.vulkan().fail(Call::wait_for_fence, 1u, VK_TIMEOUT);
  rig.run();
  CHECK(rig.holds("FAIL vulkan.compute status=7 2\n"));
  CHECK(rig.holds("ok vulkan.draw 4096\n")); }

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

TEST(vulkan_probe, a_surface_the_driver_refuses_ends_the_window_steps)
{ ProbeRig rig;
  rig.vulkan().fail(Call::create_win32_surface, 1u, VK_ERROR_NATIVE_WINDOW_IN_USE_KHR);
  rig.run();
  CHECK(rig.holds("FAIL vulkan.surface status=7"));
  CHECK(!rig.holds("vulkan.present"));
  CHECK_EQ(rig.report().failed(), 1u); }

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

TEST(vulkan_probe, frames_the_driver_did_not_pace_fail_the_pacing_step)
{ ProbeRig rig;
  rig.unpace();
  rig.run();
  CHECK(rig.holds("FAIL vulkan.pacing status=4 0"));
  CHECK(rig.holds("ok vulkan.present 4096")); }

TEST(vulkan_probe, a_pacing_step_that_cannot_find_the_adapter_measures_nothing)
{ ProbeRig rig;
  rig.kmt().fail_at(MockKmt::Call::enumerate_adapters, 2u, 0xC0000001u);
  rig.run();
  CHECK(rig.holds("ok vulkan.present 4096\n"));
  CHECK(rig.holds("FAIL vulkan.pacing status=3 0\n"));
  CHECK(rig.holds("ok vulkan.resized 9216\n"));
  CHECK_EQ(rig.report().failed(), 1u); }

TEST(vulkan_probe, a_pacing_step_without_a_device_measures_nothing)
{ ProbeRig rig;
  rig.kmt().fail_at(MockKmt::Call::create_device, 1u, 0xC0000001u);
  rig.run();
  CHECK(rig.holds("FAIL vulkan.pacing status=3 0\n"));
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::wait_for_vertical_blank), VulkanProbe::frames + 1u);
  CHECK_EQ(rig.report().failed(), 1u); }

TEST(vulkan_probe, a_blank_the_driver_refuses_measures_no_pacing)
{ ProbeRig rig;
  rig.kmt().fail_at(MockKmt::Call::wait_for_vertical_blank, VulkanProbe::frames + 1u, 0xC0000001u);
  rig.run();
  CHECK(rig.holds("FAIL vulkan.pacing status=3 0\n"));
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::wait_for_vertical_blank), VulkanProbe::frames + 2u); }

TEST(vulkan_probe, a_second_blank_that_never_comes_measures_no_pacing)
{ ProbeRig rig;
  rig.kmt().fail_at(MockKmt::Call::wait_for_vertical_blank, VulkanProbe::frames + 2u, 0xC0000001u);
  rig.run();
  CHECK(rig.holds("FAIL vulkan.pacing status=3 0\n"));
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::wait_for_vertical_blank), VulkanProbe::frames + 3u); }

TEST(vulkan_probe, a_swapchain_that_ignores_a_resize_fails_the_resized_step)
{ ProbeRig rig;
  rig.frames().refuse_resize();
  rig.run();
  CHECK(rig.holds("FAIL vulkan.resized status=3 0"));
  CHECK(rig.holds("ok vulkan.present 4096")); }

TEST(vulkan_probe, a_stale_present_that_is_not_out_of_date_fails_the_resized_step)
{ ProbeRig rig;
  rig.vulkan().fail(Call::acquire_next_image, VulkanProbe::frames + 1u, VK_ERROR_SURFACE_LOST_KHR);
  rig.run();
  CHECK(rig.holds("FAIL vulkan.resized status=4 3294967296\n"));
  CHECK(rig.holds("ok vulkan.present 4096\n"));
  CHECK_EQ(rig.vulkan().calls(Call::create_swapchain), 1u); }

TEST(vulkan_probe, a_swapchain_the_resize_cannot_replace_fails_the_resized_step)
{ ProbeRig rig;
  rig.vulkan().fail(Call::create_swapchain, 2u, VK_ERROR_OUT_OF_DEVICE_MEMORY);
  rig.run();
  CHECK(rig.holds("FAIL vulkan.resized status=7 4294967294\n"));
  CHECK(rig.holds("ok vulkan.present 4096\n"));
  CHECK_EQ(rig.frames().reads(), 1u); }

TEST(vulkan_probe, a_frame_the_new_swapchain_cannot_present_fails_the_resized_step)
{ ProbeRig rig;
  rig.vulkan().fail(Call::queue_present, VulkanProbe::frames + 1u, VK_ERROR_DEVICE_LOST);
  rig.run();
  CHECK(rig.holds("FAIL vulkan.resized status=7 4294967292\n"));
  CHECK_EQ(rig.vulkan().calls(Call::create_swapchain), 2u);
  CHECK_EQ(rig.frames().reads(), 1u); }

TEST(vulkan_probe, a_window_still_showing_the_frame_from_before_the_resize_fails_the_resized_step)
{ ProbeRig rig;
  rig.vulkan().withhold_presents_from(VulkanProbe::frames + 1u);
  rig.run();
  CHECK(rig.holds("ok vulkan.present 4096\n"));
  CHECK(rig.holds("FAIL vulkan.resized status=4 4096\n"));
  CHECK_EQ(rig.vulkan().presents(), VulkanProbe::frames + 1u); }

TEST(vulkan_probe, no_window_means_no_window_steps)
{ ProbeRig rig;
  rig.run_without_a_window();
  CHECK(rig.holds("ok vulkan.recreated 256"));
  CHECK(!rig.holds("vulkan.surface"));
  CHECK(!rig.holds("vulkan.present"));
  CHECK_EQ(rig.report().failed(), 0u); }
