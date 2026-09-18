/* The GL probe over a scripted GL (feature 050, T038, T041): every step's
 * line as the runner reads it, the version and renderer as notes, the draw
 * read back after the swap, a texture read across a share, a context refused
 * to a second thread, the ARB errors, a forced loss and a context made after
 * it; a
 * failure at each call is reported, stops the run where nothing past it can
 * run, and leaves no context or object behind. */
#include "guest/suite/gl_probe.hpp"
#include "guest/suite/guest_report.hpp"
#include "harness/guest/scripted_image_sink.hpp"
#include "guest/suite/adapter_scenario.hpp"
#include "guest/kmt/pci_identity.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "harness/guest/scripted_gl.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>

using av::ScriptedImageSink;
using av::GlProbe;
using av::GuestReport;
using av::MockKmt;
using av::ScriptedGl;
using Call = av::ScriptedGl::Call;

namespace {

class ProbeRig {
public:
    explicit ProbeRig(std::uint32_t width = 8u, std::uint32_t height = 4u) : gl_(width, height), report_("rvkglprobe")
    {
        /* The rvkgpu the probe's own steps find, and the adapter a paced swap
         * waits on, as the ICD's does (feature 051). */
        const av::PciIdentity rvk(av::AdapterScenario::virtio_vendor, av::AdapterScenario::virtio_gpu_device,
                                  av::AdapterScenario::virtio_vendor, av::AdapterScenario::rvk_subsystem);
        kmt_.add_adapter(rvk, av::AdapterScenario::wddm_3_2);
        std::uint32_t adapters[2] = {};
        std::uint32_t found = 0u;
        CHECK_STATUS(kmt_.enumerate_adapters(adapters, 2u, found), av::Status::ok);
        adapter_ = adapters[0];
        CHECK_STATUS(kmt_.create_device(adapter_, device_), av::Status::ok);
        gl_.pace_with(kmt_, adapter_, device_);
    }
    ~ProbeRig()
    {
        CHECK_STATUS(kmt_.destroy_device(device_), av::Status::ok);
        CHECK_STATUS(kmt_.close_adapter(adapter_), av::Status::ok);
        CHECK_EQ(gl_.live_contexts(), 0u);
        CHECK_EQ(gl_.live_objects(), 0u);
        CHECK_EQ(gl_.misuses(), 0u);
        CHECK(report_.invariants_hold());
    }

    ScriptedGl  &gl() { return gl_; }
    MockKmt     &kmt() { return kmt_; }
    /* The rvkgpu this run can find: without it nothing times a blank. */
    void         forget_adapter() { kmt_.fail_at(MockKmt::Call::enumerate_adapters, 1u, MockKmt::invalid_parameter); }
    GuestReport &report() { return report_; }
    const ScriptedImageSink &images() const { return images_; }
    void         run()
    {
        GlProbe probe(kmt_, report_, gl_, images_);
        probe.run();
        report_.finish();
    }
    bool holds(const char *line) const { return std::strstr(report_.text(), line) != nullptr; }

private:
    ScriptedImageSink images_;
    MockKmt     kmt_;
    std::uint32_t adapter_ = 0u;
    std::uint32_t device_ = 0u;
    ScriptedGl  gl_;
    GuestReport report_;
};

}  // namespace

TEST(gl_probe, every_step_passes_on_a_gl_that_does_what_it_is_asked)
{ ProbeRig rig;
  rig.run();
  CHECK(std::strcmp(rig.report().text(), "ok opengl.pixel_format 3\n"
                                         "ok opengl.context\n"
                                         "note opengl.version 4.6.0 scripted\n"
                                         "note opengl.renderer scripted gpu\n"
                                         "ok opengl.version 46\n"
                                         "ok opengl.draw 32\n"
                                         "ok opengl.share 4\n"
                                         "ok opengl.busy 170\n"
                                         "ok opengl.compat 32\n"
                                         "ok opengl.interval 60\n"
                                         "ok opengl.resized 4\n"
                                         "ok opengl.attribs 8342\n"
                                         "ok opengl.lost 33365\n"
                                         "ok opengl.recreated 32\n"
                                         "rvkglprobe: 12 passed, 0 failed\n") == 0);
  /* The two draws, the compatibility frame, the interval step's two runs of
   * sixty, and the resize's two (feature 051). */
  CHECK_EQ(rig.gl().calls(Call::swap_buffers), 2u + 1u + GlProbe::paced_frames + 2u);
  /* Every swap at the default interval of one blank, and the interval step's
   * sixty paced ones; its sixty unpaced ones waited for nothing. */
  CHECK_EQ(rig.gl().paced_blanks(), GlProbe::paced_frames + 5u);
  CHECK_EQ(rig.gl().calls(Call::force_fault), 1u);
  /* A picture of each render the probe judged: the two 8x4 draws, 24 bytes a
   * row, and the 2x2 shared texture, each pixel the colour it read back. */
  CHECK_EQ(rig.images().saved(), 4u);
  CHECK_EQ(rig.images().size("opengl-draw"), 54u + 4u * 24u);
  CHECK_EQ(rig.images().size("opengl-recreated"), 54u + 4u * 24u);
  CHECK_EQ(rig.images().size("opengl-share"), 54u + 2u * 8u);
  const std::uint8_t *const drawn = rig.images().bytes("opengl-draw");
  CHECK(drawn != nullptr);
  if (drawn != nullptr) {
      const std::uint8_t bgr[3] = {GlProbe::colour[2], GlProbe::colour[1], GlProbe::colour[0]};
      CHECK(std::memcmp(drawn + 54u, bgr, 3u) == 0);
      CHECK(std::memcmp(drawn + 54u + 3u * 23u, bgr, 3u) == 0);
  } }

TEST(gl_probe, no_format_or_no_context_ends_the_run)
{ const struct {
      Call          call;
      const char   *expected;
  } cases[] = {
      {Call::choose_pixel_format, "FAIL opengl.pixel_format status=3 123\nrvkglprobe: 0 passed, 1 failed\n"},
      {Call::set_pixel_format, "FAIL opengl.pixel_format status=3 123\nrvkglprobe: 0 passed, 1 failed\n"},
      {Call::create_context, "ok opengl.pixel_format 3\nFAIL opengl.context status=7 123\n"
                             "rvkglprobe: 1 passed, 1 failed\n"},
      {Call::make_current, "ok opengl.pixel_format 3\nFAIL opengl.context status=7 123\n"
                           "rvkglprobe: 1 passed, 1 failed\n"},
  };
  for (const auto &row : cases) {
    ProbeRig rig;
    rig.gl().fail_at(row.call, 1u);
    rig.run();
    CHECK(std::strcmp(rig.report().text(), row.expected) == 0);
  } }

TEST(gl_probe, a_version_outside_3_0_to_4_6_or_no_renderer_fails_the_version_step)
{ const struct {
      const char *version;
      const char *renderer;
      const char *line;
  } cases[] = {
      {"4.7 core", "gpu", "FAIL opengl.version status=6 47\n"},
      {"2.1 Mesa", "gpu", "FAIL opengl.version status=6 21\n"},
      {"5.0", "gpu", "FAIL opengl.version status=6 50\n"},
      {"4", "gpu", "FAIL opengl.version status=6 40\n"},
      {"4.", "gpu", "FAIL opengl.version status=6 40\n"},
      {"x.y", "gpu", "FAIL opengl.version status=6 0\n"},
      {nullptr, "gpu", "FAIL opengl.version status=6 0\n"},
      {"3.0", "", "FAIL opengl.version status=6 30\n"},
      {"3.3", nullptr, "FAIL opengl.version status=6 33\n"},
      {"12345.6", "gpu", "FAIL opengl.version status=6 1230\n"},
      {"3.0", "gpu", "ok opengl.version 30\n"},
      {"4.0", "gpu", "ok opengl.version 40\n"},
  };
  for (const auto &row : cases) {
    ProbeRig rig;
    rig.gl().set_strings(row.version, row.renderer);
    rig.run();
    CHECK(rig.holds(row.line));
    CHECK(rig.holds("ok opengl.attribs"));
  }
  ProbeRig named;
  named.gl().set_strings("4.6\r\nsecond line", nullptr);
  named.run();
  CHECK(named.holds("note opengl.version 4.6\nnote opengl.renderer (none)\n")); }

TEST(gl_probe, a_draw_that_went_wrong_is_counted_and_fails)
{ const struct {
      Call          call;
      std::int32_t  shift;
      std::uint32_t error;
      const char   *line;
  } cases[] = {
      {Call::count, 1, 0u, "ok opengl.draw 32\n"},
      {Call::count, -1, 0u, "ok opengl.draw 32\n"},
      {Call::count, 2, 0u, "FAIL opengl.draw status=7 0\n"},
      {Call::count, -2, 0u, "FAIL opengl.draw status=7 0\n"},
      {Call::count, 0, 0x0502u, "FAIL opengl.draw status=7 32\n"},
      {Call::swap_buffers, 0, 0u, "FAIL opengl.draw status=7 32\n"},
      {Call::create_shader, 0, 0u, "FAIL opengl.draw status=7 0\n"},
      {Call::compile_shader, 0, 0u, "FAIL opengl.draw status=7 0\n"},
      {Call::create_program, 0, 0u, "FAIL opengl.draw status=7 0\n"},
      {Call::link_program, 0, 0u, "FAIL opengl.draw status=7 0\n"},
  };
  for (const auto &row : cases) {
    ProbeRig rig;
    if (row.call != Call::count) rig.gl().fail_at(row.call, 1u);
    rig.gl().shift_colour(row.shift);
    rig.gl().set_error(row.error);
    rig.run();
    CHECK(rig.holds(row.line));
    CHECK(rig.holds("ok opengl.share 4\n"));
  }
  /* The second shader failing. */
  const Call second_shader[] = {Call::create_shader, Call::compile_shader};
  for (const Call call : second_shader) {
    ProbeRig rig;
    rig.gl().fail_at(call, 2u);
    rig.run();
    CHECK(rig.holds("FAIL opengl.draw status=7 0\n"));
  }
  /* A window larger than the read reads its corner. */
  /* A window of nothing has nothing drawn. */
  ProbeRig empty(0u, 0u);
  empty.run();
  CHECK(empty.holds("FAIL opengl.draw status=7 0\n"));
  ProbeRig large(100u, 70u);
  large.run();
  CHECK(large.holds("ok opengl.draw 4096\n")); }

TEST(gl_probe, a_share_that_cannot_be_made_or_does_not_share_fails)
{ const struct {
      Call          call;
      std::uint32_t nth;
      const char   *line;
  } cases[] = {
      {Call::create_context, 2u, "FAIL opengl.share status=7 123\n"},
      {Call::create_context, 3u, "FAIL opengl.share status=7 123\n"},
      {Call::share_lists, 1u, "FAIL opengl.share status=7 123\n"},
      {Call::make_current, 2u, "FAIL opengl.share status=7 123\n"},
      {Call::make_current, 3u, "FAIL opengl.share status=7 123\n"},
  };
  for (const auto &row : cases) {
    ProbeRig rig;
    rig.gl().fail_at(row.call, row.nth);
    rig.run();
    CHECK(rig.holds(row.line));
    CHECK(rig.holds("ok opengl.attribs"));
  }
  ProbeRig isolated;
  isolated.gl().isolate_textures();
  isolated.run();
  CHECK(isolated.holds("FAIL opengl.share status=7 0\n")); }

TEST(gl_probe, attributes_the_icd_does_not_refuse_fail_the_attribs_step)
{ ProbeRig versions;
  versions.gl().accept_versions();
  versions.run();
  CHECK(versions.holds("FAIL opengl.attribs status=7 0\n"));
  ProbeRig profiles;
  profiles.gl().accept_profiles();
  profiles.run();
  CHECK(profiles.holds("FAIL opengl.attribs status=7 0\n"));
  /* Refused for another reason. */
  ProbeRig other;
  other.gl().accept_versions();
  other.gl().fail_at(Call::create_context, 4u);
  other.run();
  CHECK(other.holds("FAIL opengl.attribs status=7 123\n")); }

/* busy (T041): the context current here is refused to another thread with
 * ERROR_BUSY; one that takes it, or refuses it for another reason, fails. */
TEST(gl_probe, a_context_another_thread_may_take_fails_the_busy_step)
{ ProbeRig taken;
  taken.gl().let_other_threads_take_contexts();
  taken.run();
  CHECK(taken.holds("FAIL opengl.busy status=7 0\n"));
  CHECK(taken.holds("ok opengl.lost 33365\n"));
  ProbeRig other;
  other.gl().fail_at(Call::make_current_elsewhere, 1u);
  other.run();
  CHECK(other.holds("FAIL opengl.busy status=7 123\n"));
  CHECK(other.holds("ok opengl.recreated 32\n")); }

/* lost (T041): a robust context faulted on purpose reports an unknown reset,
 * GL_CONTEXT_LOST once and then nothing; anything else fails the step, and
 * the run goes on to recreate a context. */
TEST(gl_probe, a_loss_that_is_not_reported_as_the_arb_says_fails_the_lost_step)
{ ProbeRig no_hook;
  no_hook.gl().hide_fault_hook();
  no_hook.run();
  CHECK(no_hook.holds("FAIL opengl.lost status=7 0\n"));
  CHECK(no_hook.holds("ok opengl.recreated 32\n"));
  ProbeRig survives;
  survives.gl().survive_faults();
  survives.run();
  CHECK(survives.holds("FAIL opengl.lost status=7 0\n"));
  ProbeRig forgets;
  forgets.gl().forget_resets();
  forgets.run();
  CHECK(forgets.holds("FAIL opengl.lost status=7 0\n"));
  ProbeRig unreported;
  unreported.gl().leave_losses_unreported();
  unreported.run();
  CHECK(unreported.holds("FAIL opengl.lost status=7 33365\n"));
  ProbeRig repeated;
  repeated.gl().report_losses_forever();
  repeated.run();
  CHECK(repeated.holds("FAIL opengl.lost status=7 33365\n"));
  /* The robust context cannot be made, or made current. */
  ProbeRig uncreated;
  uncreated.gl().fail_at(Call::create_context, 4u);
  uncreated.run();
  CHECK(uncreated.holds("FAIL opengl.lost status=7 123\n"));
  CHECK(uncreated.holds("ok opengl.recreated 32\n"));
  ProbeRig uncurrent;
  uncurrent.gl().fail_at(Call::make_current, 5u);
  uncurrent.run();
  CHECK(uncurrent.holds("FAIL opengl.lost status=7 123\n")); }

/* recreated (T041): a context made after the loss draws again; one that
 * cannot be made or made current, or does not draw, fails. */
/* Feature 051: a host with no compatibility profile is not this driver's
 * failure - the step says which profile it got and stops. */
TEST(gl_probe, a_core_only_host_fails_the_compat_step_with_its_mask)
{ ProbeRig core;
  core.gl().set_profile_mask(1);
  core.run();
  CHECK(core.holds("note opengl.compat the host granted no compatibility profile\n"));
  CHECK(core.holds("FAIL opengl.compat status=6 1\n"));
  /* And what it could not draw is not saved as a frame anybody judged. */
  CHECK_EQ(core.images().saved(), 3u); }

/* A compatibility context whose display list will not compile, and one whose
 * swap fails, are failures of the step with what they drew. */
TEST(gl_probe, a_legacy_draw_that_went_wrong_fails_the_compat_step)
{ ProbeRig unlisted;
  /* The draw's swap is the first; the compatibility frame's is the second. */
  unlisted.gl().fail_at(Call::swap_buffers, 2u);
  unlisted.run();
  CHECK(unlisted.holds("ok opengl.draw 32\n"));
  CHECK(unlisted.holds("FAIL opengl.compat status=7 32\n")); }

/* An ICD with no WGL_EXT_swap_control has no interval to honour, and an
 * adapter that answers no vertical blank cannot be timed against one. */
TEST(gl_probe, an_icd_without_swap_control_or_an_adapter_without_blanks_ends_the_interval_step)
{ ProbeRig unpaced;
  unpaced.gl().hide_swap_control();
  unpaced.run();
  CHECK(unpaced.holds("FAIL opengl.interval status=6 0\n"));

  ProbeRig blind;
  /* The draw's swap and the compatibility frame's each waited for a blank at
   * the default interval, so the step's own timing wait is the third. */
  blind.kmt().fail_at(av::MockKmt::Call::wait_for_vertical_blank, 3u, av::MockKmt::invalid_parameter);
  blind.run();
  CHECK(blind.holds("FAIL opengl.interval status=3 0\n")); }

/* A window that cannot be resized leaves the step unproven rather than
 * failing what the driver did. */
/* A compatibility context whose display list will not compile draws nothing,
 * and a GL that takes an interval and then reports another is not one the step
 * can time. */
TEST(gl_probe, a_list_that_will_not_compile_or_an_interval_that_does_not_stick_ends_its_step)
{ ProbeRig listless;
  listless.gl().refuse_lists();
  listless.run();
  CHECK(listless.holds("FAIL opengl.compat status=7 0\n"));

  ProbeRig forgetful;
  forgetful.gl().forget_intervals();
  forgetful.run();
  CHECK(forgetful.holds("FAIL opengl.interval status=6 0\n")); }

/* A driver that did not hold the frames: sixty swaps took no blanks at all. */
TEST(gl_probe, frames_the_driver_did_not_pace_fail_the_interval_step)
{ ProbeRig loose;
  loose.gl().stop_pacing();
  loose.run();
  CHECK(loose.holds("FAIL opengl.interval status=4 0\n")); }

/* An adapter this cannot open a device on, and one whose second blank never
 * arrives, are both a step with nothing to time against. */
TEST(gl_probe, an_adapter_that_cannot_be_timed_ends_the_interval_step)
{ ProbeRig undeviced;
  /* The probe's own device for the timing is the second this run makes. */
  undeviced.kmt().fail_at(av::MockKmt::Call::create_device, 1u, av::MockKmt::invalid_parameter);
  undeviced.run();
  CHECK(undeviced.holds("FAIL opengl.interval status=3 0\n"));

  ProbeRig once;
  /* The draw's and the compatibility frame's blanks, the step's first, then
   * the one it times against. */
  once.kmt().fail_at(av::MockKmt::Call::wait_for_vertical_blank, 4u, av::MockKmt::invalid_parameter);
  once.run();
  CHECK(once.holds("FAIL opengl.interval status=3 0\n")); }

/* A swap the driver refused after the window changed size, and a run with no
 * rvkgpu to time a blank against. */
TEST(gl_probe, a_swap_after_the_resize_that_fails_fails_the_step)
{ ProbeRig unswapped;
  /* The draw's, the compatibility frame's, the interval step's sixty, then the
   * resize's first: the second is what this refuses. */
  unswapped.gl().fail_at(Call::swap_buffers, 2u + 1u + GlProbe::paced_frames + 1u);
  unswapped.run();
  CHECK(unswapped.holds("FAIL opengl.resized status=7 4\n")); }

TEST(gl_probe, a_run_with_no_rvkgpu_cannot_time_the_interval)
{ ProbeRig lonely;
  lonely.forget_adapter();
  lonely.run();
  CHECK(lonely.holds("FAIL opengl.interval status=3 0\n")); }

/* A window that says it is the size it was, however the resize went. */
TEST(gl_probe, a_window_that_keeps_its_size_fails_the_resized_step)
{ ProbeRig stubborn;
  stubborn.gl().keep_its_size();
  stubborn.run();
  CHECK(stubborn.holds("FAIL opengl.resized status=7 8\n")); }

TEST(gl_probe, a_window_that_cannot_be_resized_ends_the_resized_step)
{ ProbeRig fixed;
  fixed.gl().refuse_resizes();
  fixed.run();
  CHECK(fixed.holds("FAIL opengl.resized status=6 8\n"));

  /* A window one pixel wide cannot be narrowed at all. */
  ProbeRig narrow(1u, 1u);
  narrow.run();
  CHECK(narrow.holds("FAIL opengl.resized status=6 0\n")); }

TEST(gl_probe, a_context_that_cannot_be_made_again_fails_the_recreated_step)
{ ProbeRig uncreated;
  uncreated.gl().fail_at(Call::create_context, 5u);
  uncreated.run();
  CHECK(uncreated.holds("ok opengl.lost 33365\n"));
  CHECK(uncreated.holds("FAIL opengl.recreated status=7 123\n"));
  ProbeRig uncurrent;
  uncurrent.gl().fail_at(Call::make_current, 6u);
  uncurrent.run();
  CHECK(uncurrent.holds("FAIL opengl.recreated status=7 123\n"));
  ProbeRig undrawn;
  /* The draw's swap, the compatibility frame's, the interval step's sixty and
   * the resize's two, then the recreated draw's. */
  undrawn.gl().fail_at(Call::swap_buffers, 2u + 1u + GlProbe::paced_frames + 2u);
  undrawn.run();
  CHECK(undrawn.holds("ok opengl.draw 32\n"));
  CHECK(undrawn.holds("FAIL opengl.recreated status=7 32\n")); }
