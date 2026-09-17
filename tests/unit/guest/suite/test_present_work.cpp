/* The probe's present work (feature 046) on a scripted GPU and a scripted
 * window: the surface it makes, the swapchain of the window's size, the frames
 * it draws and presents, and what it reads back off the window - with every
 * refusal each step can meet. */
#include "guest/suite/bmp_image.hpp"
#include "guest/suite/present_work.hpp"
#include "guest/suite/probe_device.hpp"
#include "guest/suite/probe_outcome.hpp"
#include "guest/suite/window_frame.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/guest/scripted_image_sink.hpp"
#include "harness/guest/scripted_vulkan.hpp"
#include "harness/guest/scripted_window_frame.hpp"

#include <cstdint>

using av::PresentWork;
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
    PresentRig() : device_(vulkan_)
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

private:
    ScriptedVulkan        vulkan_;
    ScriptedWindowFrame   frames_;
    av::ScriptedImageSink pictures_;
    ProbeDevice           device_;
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
  /* A window larger than any frame it keeps, and one that cannot be measured. */
  PresentRig   rig;
  PresentWork  work = rig.work();
  ProbeOutcome outcome;
  CHECK(work.make_surface(outcome));
  CHECK(rig.frames().resize(window_handle, PresentWork::max_side + 8u, side));
  CHECK(!work.make_swapchain(VK_PRESENT_MODE_FIFO_KHR, outcome));
  rig.frames().refuse_size();
  CHECK(!work.make_swapchain(VK_PRESENT_MODE_FIFO_KHR, outcome)); }

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
