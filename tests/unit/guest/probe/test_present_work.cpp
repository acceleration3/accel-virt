#include "guest/probe/bmp_image.hpp"
#include "guest/suite/guest_report.hpp"
#include "guest/probe/present_work.hpp"
#include "guest/probe/probe_device.hpp"
#include "guest/probe/probe_outcome.hpp"
#include "guest/suite/scenario_steps.hpp"
#include "guest/probe/window_frame.hpp"
#include "harness/platform/mock_platform.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "harness/guest/scripted_image_sink.hpp"
#include "harness/guest/scripted_vulkan.hpp"
#include "harness/guest/scripted_window_frame.hpp"

#include <cstdint>
#include <cstring>

namespace av {
struct PresentWorkProbe {
    static void more_images_than_there_is_room_for(PresentWork &work) { work.count_ = PresentWork::images + 1u; }
    static void a_swapchain_without_its_surface(PresentWork &work) { work.surface_ = VK_NULL_HANDLE; }
    static void images_with_no_swapchain(PresentWork &work) { work.count_ = 1u; }
    static void a_framebuffer_without_its_view(PresentWork &work) { work.views_[0] = VK_NULL_HANDLE; }
    static void wider_than_the_frame_it_keeps(PresentWork &work) { work.width_ = PresentWork::max_side + 1u; }
    static void taller_than_the_frame_it_keeps(PresentWork &work) { work.height_ = PresentWork::max_side + 1u; }
};

}

using av::PresentWork;
using av::PresentWorkProbe;
using av::ProbeDevice;
using av::ProbeOutcome;
using av::ScriptedVulkan;
using av::ScriptedWindowFrame;
using av::WindowFrame;
using Call = av::ScriptedVulkan::Call;

namespace {

constexpr std::uint32_t side = PresentWork::side;
constexpr std::uint32_t pixels = side * side;
void *const             window_handle = reinterpret_cast<void *>(0x7770u);
void *const             module_handle = reinterpret_cast<void *>(0x7771u);

class PresentRig {
public:
    PresentRig() : device_(vulkan_), report_(report_memory_, "avvkprobe"), steps_(kmt_, report_, "vulkan")
    {
        frames_.set_window(window_handle, side, side);
        vulkan_.present_into(frames_);
        CHECK_EQ(device_.make_instance(), VK_SUCCESS);
        CHECK_EQ(device_.list_physical_devices(), VK_SUCCESS);
        CHECK(device_.choose(1u));
        CHECK(device_.choose_family());
        CHECK_EQ(device_.make_device(), VK_SUCCESS);
    }

    ScriptedVulkan      &vulkan() { return vulkan_; }
    ScriptedWindowFrame &frames() { return frames_; }
    ProbeDevice         &device() { return device_; }
    av::ScriptedImageSink &pictures() { return pictures_; }
    PresentWork          work() { return PresentWork(device_, frames_, pictures_, module_handle, window_handle); }

    const char *said(const ProbeOutcome &outcome, const char *step)
    {
        outcome.report(steps_, step);
        return report_.text();
    }

private:
    ScriptedVulkan        vulkan_;
    ScriptedWindowFrame   frames_;
    av::ScriptedImageSink pictures_;
    ProbeDevice           device_;
    av::MockKmt           kmt_;
    av::MockPlatform report_memory_;
    av::GuestReport       report_{report_memory_};
    av::ScenarioSteps     steps_;
};

}

TEST(present_work, a_surface_describes_the_window_it_was_made_for)
{ PresentRig   rig;
  PresentWork  work = rig.work();
  ProbeOutcome outcome;
  CHECK(work.invariants_hold());
  CHECK(work.make_surface(outcome));
  CHECK(work.invariants_hold());
  CHECK_EQ(work.width(), side);
  CHECK_EQ(work.height(), side);
  CHECK_EQ(work.presented(), 0u); }

TEST(present_work, a_surface_that_does_not_describe_its_window_is_refused)
{ { PresentRig   rig;
    PresentWork  work = rig.work();
    ProbeOutcome outcome;
    rig.vulkan().fail(Call::create_win32_surface, 1u, VK_ERROR_NATIVE_WINDOW_IN_USE_KHR);
    CHECK(!work.make_surface(outcome)); }
  { PresentRig   rig;
    PresentWork  work = rig.work();
    ProbeOutcome outcome;
    rig.frames().refuse_size();
    CHECK(!work.make_surface(outcome)); }
  { PresentRig   rig;
    PresentWork  work = rig.work();
    ProbeOutcome outcome;
    rig.vulkan().hold_the_surface_extent(side / 2u, side);
    CHECK(!work.make_surface(outcome));
    CHECK(work.invariants_hold()); }
  { PresentRig   rig;
    PresentWork  work = rig.work();
    ProbeOutcome outcome;
    rig.vulkan().lose_the_surface_queries();
    CHECK(!work.make_surface(outcome)); }
  { PresentRig   rig;
    PresentWork  work = rig.work();
    ProbeOutcome outcome;
    rig.vulkan().offer_nothing_presentable();
    CHECK(!work.make_surface(outcome));
    CHECK(work.invariants_hold()); } }

TEST(present_work, a_surface_offering_less_than_a_present_needs_is_refused)
{
  { PresentRig   rig;
    PresentWork  work = rig.work();
    ProbeOutcome outcome;
    rig.frames().refuse_size_after(1u);
    CHECK(!work.make_surface(outcome));
    CHECK(std::strcmp(rig.said(outcome, "surface"), "FAIL vulkan.surface status=3 0\n") == 0);
    CHECK_EQ(work.height(), 0u); }
  { PresentRig   rig;
    PresentWork  work = rig.work();
    ProbeOutcome outcome;
    rig.vulkan().hold_the_surface_extent(side, side / 2u);
    CHECK(!work.make_surface(outcome));
    CHECK(std::strcmp(rig.said(outcome, "surface"), "FAIL vulkan.surface status=3 0\n") == 0); }
  { PresentRig   rig;
    PresentWork  work = rig.work();
    ProbeOutcome outcome;
    rig.vulkan().ask_for_images(PresentWork::images + 1u);
    CHECK(!work.make_surface(outcome));
    CHECK(std::strcmp(rig.said(outcome, "surface"), "FAIL vulkan.surface status=3 0\n") == 0);
    CHECK_EQ(work.width(), 0u); }
  { PresentRig   rig;
    PresentWork  work = rig.work();
    ProbeOutcome outcome;
    rig.vulkan().lose_the_present_modes();
    CHECK(!work.make_surface(outcome));
    CHECK(std::strcmp(rig.said(outcome, "surface"), "FAIL vulkan.surface status=7 3294967296\n") == 0); }
  { PresentRig   rig;
    PresentWork  work = rig.work();
    ProbeOutcome outcome;
    rig.vulkan().offer_only_the_mode(VK_PRESENT_MODE_FIFO_KHR);
    CHECK(!work.make_surface(outcome));
    CHECK(std::strcmp(rig.said(outcome, "surface"), "FAIL vulkan.surface status=3 0\n") == 0); }
  { PresentRig   rig;
    PresentWork  work = rig.work();
    ProbeOutcome outcome;
    rig.vulkan().offer_only_the_mode(VK_PRESENT_MODE_IMMEDIATE_KHR);
    CHECK(!work.make_surface(outcome));
    CHECK(std::strcmp(rig.said(outcome, "surface"), "FAIL vulkan.surface status=3 0\n") == 0); } }

TEST(present_work, a_swapchain_is_the_window_s_size_and_hands_out_its_images)
{ PresentRig   rig;
  PresentWork  work = rig.work();
  ProbeOutcome outcome;
  CHECK(work.make_surface(outcome));
  CHECK(work.make_swapchain(VK_PRESENT_MODE_FIFO_KHR, outcome));
  CHECK(work.invariants_hold());
  CHECK_EQ(work.width(), side);
  CHECK_EQ(rig.vulkan().calls(Call::create_swapchain), 1u);
  work.release_swapchain();
  CHECK(work.invariants_hold());
  CHECK(work.make_swapchain(VK_PRESENT_MODE_IMMEDIATE_KHR, outcome));
  CHECK_EQ(rig.vulkan().calls(Call::create_swapchain), 2u); }

TEST(present_work, a_swapchain_follows_the_window_s_new_size)
{ PresentRig   rig;
  PresentWork  work = rig.work();
  ProbeOutcome outcome;
  CHECK(work.make_surface(outcome));
  CHECK(work.make_swapchain(VK_PRESENT_MODE_FIFO_KHR, outcome));
  CHECK(rig.frames().resize(window_handle, side + 8u, side + 4u));
  CHECK(work.make_swapchain(VK_PRESENT_MODE_FIFO_KHR, outcome));
  CHECK_EQ(work.width(), side + 8u);
  CHECK_EQ(work.height(), side + 4u);
  CHECK(work.invariants_hold()); }

TEST(present_work, a_swapchain_that_cannot_be_made_is_the_driver_s_refusal)
{ const Call steps[] = {Call::create_swapchain, Call::create_render_pass, Call::create_shader_module,
                        Call::create_pipeline_layout, Call::create_graphics_pipeline, Call::create_image_view,
                        Call::create_framebuffer};
  for (const Call step : steps) {
      PresentRig   rig;
      PresentWork  work = rig.work();
      ProbeOutcome outcome;
      CHECK(work.make_surface(outcome));
      rig.vulkan().fail(step, 1u, VK_ERROR_OUT_OF_DEVICE_MEMORY);
      CHECK(!work.make_swapchain(VK_PRESENT_MODE_FIFO_KHR, outcome));
      CHECK(work.invariants_hold());
  }
  { PresentRig   rig;
    PresentWork  work = rig.work();
    ProbeOutcome outcome;
    CHECK(work.make_surface(outcome));
    rig.vulkan().fail(Call::create_shader_module, 2u, VK_ERROR_OUT_OF_HOST_MEMORY);
    CHECK(!work.make_swapchain(VK_PRESENT_MODE_FIFO_KHR, outcome));
    CHECK_EQ(rig.vulkan().calls(Call::create_shader_module), 2u);
    CHECK_EQ(rig.vulkan().calls(Call::create_pipeline_layout), 0u);
    CHECK(work.invariants_hold()); }
  PresentRig   rig;
  PresentWork  work = rig.work();
  ProbeOutcome outcome;
  CHECK(work.make_surface(outcome));
  CHECK(rig.frames().resize(window_handle, PresentWork::max_side + 8u, side));
  CHECK(!work.make_swapchain(VK_PRESENT_MODE_FIFO_KHR, outcome));
  CHECK(rig.frames().resize(window_handle, side, PresentWork::max_side + 8u));
  CHECK(!work.make_swapchain(VK_PRESENT_MODE_FIFO_KHR, outcome));
  rig.frames().refuse_size();
  CHECK(!work.make_swapchain(VK_PRESENT_MODE_FIFO_KHR, outcome));
  CHECK_EQ(rig.vulkan().calls(Call::create_swapchain), 0u);
  CHECK(work.invariants_hold()); }

TEST(present_work, a_swapchain_whose_images_cannot_be_had_is_refused)
{ { PresentRig   rig;
    PresentWork  work = rig.work();
    ProbeOutcome outcome;
    CHECK(work.make_surface(outcome));
    rig.vulkan().lose_the_swapchain_images();
    CHECK(!work.make_swapchain(VK_PRESENT_MODE_FIFO_KHR, outcome));
    CHECK(std::strcmp(rig.said(outcome, "present"), "FAIL vulkan.present status=7 4294967294\n") == 0);
    CHECK_EQ(rig.vulkan().calls(Call::create_render_pass), 0u);
    CHECK(work.invariants_hold()); }
  { PresentRig   rig;
    PresentWork  work = rig.work();
    ProbeOutcome outcome;
    CHECK(work.make_surface(outcome));
    rig.vulkan().hand_out_one_image();
    CHECK(!work.make_swapchain(VK_PRESENT_MODE_FIFO_KHR, outcome));
    CHECK(std::strcmp(rig.said(outcome, "present"), "FAIL vulkan.present status=3 0\n") == 0);
    CHECK_EQ(rig.vulkan().calls(Call::create_image_view), 0u);
    CHECK(work.invariants_hold()); } }

TEST(present_work, an_invariant_refuses_each_state_it_names)
{ { PresentRig   rig;
    PresentWork  work = rig.work();
    ProbeOutcome outcome;
    CHECK(work.make_surface(outcome));
    CHECK(work.make_swapchain(VK_PRESENT_MODE_FIFO_KHR, outcome));
    PresentWorkProbe::more_images_than_there_is_room_for(work);
    CHECK(!work.invariants_hold()); }
  { PresentRig   rig;
    PresentWork  work = rig.work();
    ProbeOutcome outcome;
    CHECK(work.make_surface(outcome));
    CHECK(work.make_swapchain(VK_PRESENT_MODE_FIFO_KHR, outcome));
    PresentWorkProbe::a_swapchain_without_its_surface(work);
    CHECK(!work.invariants_hold()); }
  { PresentRig   rig;
    PresentWork  work = rig.work();
    ProbeOutcome outcome;
    CHECK(work.make_surface(outcome));
    CHECK(work.make_swapchain(VK_PRESENT_MODE_FIFO_KHR, outcome));
    work.release_swapchain();
    PresentWorkProbe::images_with_no_swapchain(work);
    CHECK(!work.invariants_hold()); }
  { PresentRig   rig;
    PresentWork  work = rig.work();
    ProbeOutcome outcome;
    CHECK(work.make_surface(outcome));
    CHECK(work.make_swapchain(VK_PRESENT_MODE_FIFO_KHR, outcome));
    PresentWorkProbe::a_framebuffer_without_its_view(work);
    CHECK(!work.invariants_hold()); }
  { PresentRig  rig;
    PresentWork work = rig.work();
    PresentWorkProbe::wider_than_the_frame_it_keeps(work);
    CHECK(!work.invariants_hold()); }
  { PresentRig  rig;
    PresentWork work = rig.work();
    PresentWorkProbe::taller_than_the_frame_it_keeps(work);
    CHECK(!work.invariants_hold()); } }

TEST(present_work, the_frames_it_presents_are_what_the_window_shows)
{ PresentRig   rig;
  PresentWork  work = rig.work();
  ProbeOutcome outcome;
  CHECK(work.make_surface(outcome));
  CHECK(work.make_swapchain(VK_PRESENT_MODE_FIFO_KHR, outcome));
  CHECK_EQ(work.present_frames(3u, outcome), VK_SUCCESS);
  CHECK_EQ(work.presented(), 3u);
  CHECK_EQ(rig.vulkan().presents(), 3u);
  CHECK_EQ(work.drawn_on_the_window("vulkan-present", outcome), pixels);
  CHECK_EQ(rig.frames().reads(), 1u);
  CHECK(rig.pictures().bytes("vulkan-present") != nullptr);
  CHECK_EQ(rig.pictures().size("vulkan-present"), av::BmpImage::header_bytes + pixels * 3u);
  CHECK(work.invariants_hold()); }

TEST(present_work, a_window_that_is_not_the_frame_is_counted_pixel_by_pixel)
{ PresentRig   rig;
  PresentWork  work = rig.work();
  ProbeOutcome outcome;
  rig.vulkan().misdraw();
  CHECK(work.make_surface(outcome));
  CHECK(work.make_swapchain(VK_PRESENT_MODE_FIFO_KHR, outcome));
  CHECK_EQ(work.present_frames(1u, outcome), VK_SUCCESS);
  CHECK_EQ(work.drawn_on_the_window("vulkan-present", outcome), pixels - 1u);
  rig.frames().refuse_read();
  CHECK_EQ(work.drawn_on_the_window("vulkan-present", outcome), 0u); }

TEST(present_work, a_window_one_off_in_a_channel_is_the_colour_and_two_off_is_not)
{ PresentRig   rig;
  PresentWork  work = rig.work();
  ProbeOutcome outcome;
  CHECK(work.make_surface(outcome));
  CHECK(work.make_swapchain(VK_PRESENT_MODE_FIFO_KHR, outcome));
  CHECK_EQ(work.present_frames(1u, outcome), VK_SUCCESS);
  const std::uint8_t *const drawn = PresentWork::window_colour;
  const std::uint8_t rounded_down[4] = {drawn[0], static_cast<std::uint8_t>(drawn[1] - 1u), drawn[2], 0u};
  rig.frames().show(rounded_down);
  CHECK_EQ(work.drawn_on_the_window("vulkan-present", outcome), pixels);
  for (std::uint32_t channel = 0u; channel < 3u; channel++) {
      std::uint8_t off[4] = {drawn[0], drawn[1], drawn[2], 255u};
      off[channel] = static_cast<std::uint8_t>(drawn[channel] + 1u);
      rig.frames().show(off);
      CHECK_EQ(work.drawn_on_the_window("vulkan-present", outcome), pixels);
      off[channel] = static_cast<std::uint8_t>(drawn[channel] + 2u);
      rig.frames().show(off);
      CHECK_EQ(work.drawn_on_the_window("vulkan-present", outcome), 0u);
      off[channel] = static_cast<std::uint8_t>(drawn[channel] - 2u);
      rig.frames().show(off);
      CHECK_EQ(work.drawn_on_the_window("vulkan-present", outcome), 0u);
  } }

TEST(present_work, a_frame_larger_than_the_triangle_is_the_background_past_it)
{ PresentRig   rig;
  PresentWork  work = rig.work();
  ProbeOutcome outcome;
  constexpr std::uint32_t grown = side + side / 2u;
  CHECK(work.make_surface(outcome));
  CHECK(work.make_swapchain(VK_PRESENT_MODE_FIFO_KHR, outcome));
  CHECK_EQ(work.present_frames(1u, outcome), VK_SUCCESS);
  CHECK(rig.frames().resize(window_handle, grown, grown));
  CHECK(work.make_swapchain(VK_PRESENT_MODE_FIFO_KHR, outcome));
  CHECK_EQ(work.present_frames(1u, outcome), VK_SUCCESS);
  CHECK_EQ(work.drawn_on_the_window("vulkan-resized", outcome), grown * grown);
  CHECK(work.invariants_hold());
  const std::uint8_t triangle[4] = {PresentWork::window_colour[0], PresentWork::window_colour[1],
                                    PresentWork::window_colour[2], 255u};
  rig.frames().show(triangle);
  CHECK_EQ(work.drawn_on_the_window("vulkan-resized", outcome), pixels);
  const std::uint8_t cleared[4] = {PresentWork::window_background[0], PresentWork::window_background[1],
                                   PresentWork::window_background[2], 255u};
  rig.frames().show(cleared);
  CHECK_EQ(work.drawn_on_the_window("vulkan-resized", outcome), grown * grown - pixels); }

TEST(present_work, a_window_narrower_than_the_triangle_shows_the_triangle_as_far_as_it_goes)
{ PresentRig   rig;
  PresentWork  work = rig.work();
  ProbeOutcome outcome;
  CHECK(work.make_surface(outcome));
  CHECK(work.make_swapchain(VK_PRESENT_MODE_FIFO_KHR, outcome));
  CHECK_EQ(work.present_frames(1u, outcome), VK_SUCCESS);
  CHECK(rig.frames().resize(window_handle, side / 2u, side));
  CHECK(work.make_swapchain(VK_PRESENT_MODE_FIFO_KHR, outcome));
  CHECK_EQ(work.present_frames(1u, outcome), VK_SUCCESS);
  CHECK_EQ(work.drawn_on_the_window("vulkan-narrow", outcome), side / 2u * side);
  CHECK(work.invariants_hold()); }

TEST(present_work, a_window_still_showing_the_frame_from_before_it_grew_is_not_the_frame)
{ PresentRig   rig;
  PresentWork  work = rig.work();
  ProbeOutcome outcome;
  constexpr std::uint32_t grown = side + side / 2u;
  CHECK(work.make_surface(outcome));
  CHECK(work.make_swapchain(VK_PRESENT_MODE_FIFO_KHR, outcome));
  CHECK_EQ(work.present_frames(1u, outcome), VK_SUCCESS);
  rig.vulkan().withhold_presents_from(2u);
  CHECK(rig.frames().resize(window_handle, grown, grown));
  CHECK(work.make_swapchain(VK_PRESENT_MODE_FIFO_KHR, outcome));
  CHECK_EQ(work.present_frames(1u, outcome), VK_SUCCESS);
  CHECK_EQ(rig.vulkan().presents(), 2u);
  CHECK_EQ(work.drawn_on_the_window("vulkan-resized", outcome), pixels); }

TEST(present_work, a_frame_that_cannot_be_drawn_or_presented_answers_the_driver_s_error)
{ { PresentRig   rig;
    PresentWork  work = rig.work();
    ProbeOutcome outcome;
    CHECK(work.make_surface(outcome));
    CHECK(work.make_swapchain(VK_PRESENT_MODE_FIFO_KHR, outcome));
    rig.vulkan().fail(Call::acquire_next_image, 1u, VK_ERROR_OUT_OF_DATE_KHR);
    CHECK_EQ(work.present_frames(1u, outcome), VK_ERROR_OUT_OF_DATE_KHR);
    CHECK_EQ(work.presented(), 0u); }
  { PresentRig   rig;
    PresentWork  work = rig.work();
    ProbeOutcome outcome;
    CHECK(work.make_surface(outcome));
    CHECK(work.make_swapchain(VK_PRESENT_MODE_FIFO_KHR, outcome));
    rig.vulkan().fail(Call::create_command_pool, 1u, VK_ERROR_OUT_OF_HOST_MEMORY);
    CHECK_EQ(work.present_frames(1u, outcome), VK_ERROR_INITIALIZATION_FAILED); }
  { PresentRig   rig;
    PresentWork  work = rig.work();
    ProbeOutcome outcome;
    CHECK(work.make_surface(outcome));
    CHECK(work.make_swapchain(VK_PRESENT_MODE_FIFO_KHR, outcome));
    rig.vulkan().fail(Call::queue_submit, 1u, VK_ERROR_DEVICE_LOST);
    CHECK_EQ(work.present_frames(1u, outcome), VK_ERROR_INITIALIZATION_FAILED); }
  { PresentRig   rig;
    PresentWork  work = rig.work();
    ProbeOutcome outcome;
    CHECK(work.make_surface(outcome));
    CHECK(work.make_swapchain(VK_PRESENT_MODE_FIFO_KHR, outcome));
    rig.vulkan().fail(Call::queue_present, 1u, VK_ERROR_SURFACE_LOST_KHR);
    CHECK_EQ(work.present_frames(2u, outcome), VK_ERROR_SURFACE_LOST_KHR);
    CHECK_EQ(work.presented(), 0u);
    CHECK(work.invariants_hold()); } }

TEST(present_work, the_window_it_reads_is_the_seam_it_was_given)
{ ScriptedWindowFrame frames;
  WindowFrame        &seam = frames;
  frames.set_window(window_handle, 2u, 1u);
  std::uint32_t width = 0u;
  std::uint32_t height = 0u;
  CHECK(seam.size(window_handle, width, height));
  CHECK_EQ(width, 2u);
  CHECK_EQ(height, 1u);
  CHECK(!seam.size(reinterpret_cast<void *>(0x99u), width, height));
  const std::uint8_t blue[4] = {0xFFu, 0x10u, 0x20u, 0xFFu};
  frames.show(blue);
  std::uint8_t read_back[2u * 4u] = {};
  CHECK(seam.read(window_handle, 2u, 1u, read_back));
  CHECK_EQ(read_back[0], 0xFFu);
  CHECK_EQ(read_back[6], 0x20u);
  CHECK(!seam.read(window_handle, 4u, 1u, read_back));
  CHECK(seam.resize(window_handle, 4u, 2u));
  CHECK_EQ(frames.resizes(), 1u);
  CHECK(seam.size(window_handle, width, height));
  CHECK_EQ(width, 4u);
  CHECK(!seam.resize(window_handle, ScriptedWindowFrame::max_side + 1u, 2u));
  CHECK(!seam.resize(reinterpret_cast<void *>(0x99u), 4u, 2u));
  CHECK_EQ(frames.resizes(), 1u); }
