/* The probe's present work (feature 046) on a scripted GPU and a scripted
 * window: the surface it makes, the swapchain of the window's size, the frames
 * it draws and presents, and what it reads back off the window - with every
 * refusal each step can meet. */
#include "guest/suite/bmp_image.hpp"
#include "guest/suite/guest_report.hpp"
#include "guest/suite/present_work.hpp"
#include "guest/suite/probe_device.hpp"
#include "guest/suite/probe_outcome.hpp"
#include "guest/suite/scenario_steps.hpp"
#include "guest/suite/window_frame.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "harness/guest/scripted_image_sink.hpp"
#include "harness/guest/scripted_vulkan.hpp"
#include "harness/guest/scripted_window_frame.hpp"

#include <cstdint>
#include <cstring>

namespace av {

/* The states no legitimate sequence can put the work in, which is what makes
 * each clause of the invariant worth stating. */
struct PresentWorkProbe {
    static void more_images_than_there_is_room_for(PresentWork &work) { work.count_ = PresentWork::images + 1u; }
    static void a_swapchain_without_its_surface(PresentWork &work) { work.surface_ = VK_NULL_HANDLE; }
    static void images_with_no_swapchain(PresentWork &work) { work.count_ = 1u; }
    static void a_framebuffer_without_its_view(PresentWork &work) { work.views_[0] = VK_NULL_HANDLE; }
    static void wider_than_the_frame_it_keeps(PresentWork &work) { work.width_ = PresentWork::max_side + 1u; }
    static void taller_than_the_frame_it_keeps(PresentWork &work) { work.height_ = PresentWork::max_side + 1u; }
};

}  // namespace av

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

/* A device on the scripted GPU, a window it presents into, and the work. */
class PresentRig {
public:
    PresentRig() : device_(vulkan_), report_("rvkvkprobe"), steps_(kmt_, report_, "vulkan")
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
    /* The work over this rig; a window of nullptr is no window at all. */
    PresentWork          work() { return PresentWork(device_, frames_, pictures_, module_handle, window_handle); }

    /* The line `step` would put in the guest's report: what the runner reads,
     * and the only place a refusal says which one it was. */
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
    av::GuestReport       report_;
    av::ScenarioSteps     steps_;
};

}  // namespace

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

/* A surface the driver refuses, a window that cannot be measured, a surface of
 * another size than its window, and one whose formats or modes are not this
 * driver's: each ends the work. */
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
  /* A surface whose queries say it is lost, and one offering neither the
   * format nor the present mode this driver presents in. */
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

/* What the surface offers is judged clause by clause, and the line each
 * refusal writes says which kind it was: a driver's error, or nothing the
 * probe can present through. */
TEST(present_work, a_surface_offering_less_than_a_present_needs_is_refused)
{ /* The window answers the surface's extent and is then gone: the size the
   * surface must agree with cannot be had. */
  { PresentRig   rig;
    PresentWork  work = rig.work();
    ProbeOutcome outcome;
    rig.frames().refuse_size_after(1u);
    CHECK(!work.make_surface(outcome));
    CHECK(std::strcmp(rig.said(outcome, "surface"), "FAIL vulkan.surface status=3 0\n") == 0);
    CHECK_EQ(work.height(), 0u); }
  /* A surface as wide as its window but half as tall. */
  { PresentRig   rig;
    PresentWork  work = rig.work();
    ProbeOutcome outcome;
    rig.vulkan().hold_the_surface_extent(side, side / 2u);
    CHECK(!work.make_surface(outcome));
    CHECK(std::strcmp(rig.said(outcome, "surface"), "FAIL vulkan.surface status=3 0\n") == 0); }
  /* A driver wanting more images than the work keeps room for. */
  { PresentRig   rig;
    PresentWork  work = rig.work();
    ProbeOutcome outcome;
    rig.vulkan().ask_for_images(PresentWork::images + 1u);
    CHECK(!work.make_surface(outcome));
    CHECK(std::strcmp(rig.said(outcome, "surface"), "FAIL vulkan.surface status=3 0\n") == 0);
    /* Nothing of a surface that was refused is kept. */
    CHECK_EQ(work.width(), 0u); }
  /* The formats answered and the modes lost: the driver's error, not a
   * surface that offers the wrong thing. */
  { PresentRig   rig;
    PresentWork  work = rig.work();
    ProbeOutcome outcome;
    rig.vulkan().lose_the_present_modes();
    CHECK(!work.make_surface(outcome));
    CHECK(std::strcmp(rig.said(outcome, "surface"), "FAIL vulkan.surface status=7 3294967296\n") == 0); }
  /* Both modes are needed: the paced one for the present step and the unpaced
   * one for the resize, and a driver offering only one of them is refused. */
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
  /* Its own release leaves the surface, so another swapchain may be made. */
  work.release_swapchain();
  CHECK(work.invariants_hold());
  CHECK(work.make_swapchain(VK_PRESENT_MODE_IMMEDIATE_KHR, outcome));
  CHECK_EQ(rig.vulkan().calls(Call::create_swapchain), 2u); }

/* A window that grew: the swapchain is made at the size the window has now. */
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
  /* The fragment shader is the second module made: it is what fails, and the
   * pipeline layout after it is never asked for. */
  { PresentRig   rig;
    PresentWork  work = rig.work();
    ProbeOutcome outcome;
    CHECK(work.make_surface(outcome));
    rig.vulkan().fail(Call::create_shader_module, 2u, VK_ERROR_OUT_OF_HOST_MEMORY);
    CHECK(!work.make_swapchain(VK_PRESENT_MODE_FIFO_KHR, outcome));
    CHECK_EQ(rig.vulkan().calls(Call::create_shader_module), 2u);
    CHECK_EQ(rig.vulkan().calls(Call::create_pipeline_layout), 0u);
    CHECK(work.invariants_hold()); }
  /* A window larger than any frame it keeps, in either direction, and one that
   * cannot be measured: none of them reaches the driver. */
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

/* The swapchain is made but its images are not the two the work has room for:
 * a driver's error is reported as one, and a count that is not two is a
 * driver the work cannot present through. */
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
    /* Nothing is built on images the work never got. */
    CHECK_EQ(rig.vulkan().calls(Call::create_image_view), 0u);
    CHECK(work.invariants_hold()); } }

/* Every clause of the invariant refuses the state it names. None of them is
 * reachable by presenting, so each is put there by hand. */
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
  /* A frame larger than the bytes kept for it, in either direction. */
  { PresentRig  rig;
    PresentWork work = rig.work();
    PresentWorkProbe::wider_than_the_frame_it_keeps(work);
    CHECK(!work.invariants_hold()); }
  { PresentRig  rig;
    PresentWork work = rig.work();
    PresentWorkProbe::taller_than_the_frame_it_keeps(work);
    CHECK(!work.invariants_hold()); } }

/* The frames reach the window: every pixel of it is the triangle's colour,
 * and the picture kept is what the window showed. */
TEST(present_work, the_frames_it_presents_are_what_the_window_shows)
{ PresentRig   rig;
  PresentWork  work = rig.work();
  ProbeOutcome outcome;
  CHECK(work.make_surface(outcome));
  CHECK(work.make_swapchain(VK_PRESENT_MODE_FIFO_KHR, outcome));
  CHECK_EQ(work.present_frames(3u, outcome), VK_SUCCESS);
  CHECK_EQ(work.presented(), 3u);
  CHECK_EQ(rig.vulkan().presents(), 3u);
  CHECK_EQ(work.coloured_on_the_window("vulkan-present", outcome), pixels);
  CHECK_EQ(rig.frames().reads(), 1u);
  /* The picture kept is the window's, and a BMP of its size. */
  CHECK(rig.pictures().bytes("vulkan-present") != nullptr);
  CHECK_EQ(rig.pictures().size("vulkan-present"), av::BmpImage::header_bytes + pixels * 3u);
  CHECK(work.invariants_hold()); }

/* What the window shows is what the step judges: one pixel short is short. */
TEST(present_work, a_window_that_is_not_the_frame_is_counted_pixel_by_pixel)
{ PresentRig   rig;
  PresentWork  work = rig.work();
  ProbeOutcome outcome;
  rig.vulkan().misdraw();
  CHECK(work.make_surface(outcome));
  CHECK(work.make_swapchain(VK_PRESENT_MODE_FIFO_KHR, outcome));
  CHECK_EQ(work.present_frames(1u, outcome), VK_SUCCESS);
  CHECK_EQ(work.coloured_on_the_window("vulkan-present", outcome), pixels - 1u);
  /* A window that cannot be read is no count at all. */
  rig.frames().refuse_read();
  CHECK_EQ(work.coloured_on_the_window("vulkan-present", outcome), 0u); }

/* Every refusal a frame can meet: the acquire, the recording, the submission
 * and the present itself. */
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

/* The window seam is what a probe sees of the desktop, whoever implements it. */
TEST(present_work, the_window_it_reads_is_the_seam_it_was_given)
{ ScriptedWindowFrame frames;
  WindowFrame        &seam = frames;
  frames.set_window(window_handle, 2u, 1u);
  std::uint32_t width = 0u;
  std::uint32_t height = 0u;
  CHECK(seam.size(window_handle, width, height));
  CHECK_EQ(width, 2u);
  CHECK_EQ(height, 1u);
  /* Another window is not this one. */
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
  /* A resize past what it keeps, and one of another window, do nothing. */
  CHECK(!seam.resize(window_handle, ScriptedWindowFrame::max_side + 1u, 2u));
  CHECK(!seam.resize(reinterpret_cast<void *>(0x99u), 4u, 2u));
  CHECK_EQ(frames.resizes(), 1u); }
