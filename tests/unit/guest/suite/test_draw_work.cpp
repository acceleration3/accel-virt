/* The probe's draw step over a scripted Vulkan: the image drawn and copied out
 * in one command buffer, three vertices, both shaders, and every pixel of the
 * copy compared with solid.frag's colour within a driver's rounding; a refusal
 * at every call is kept, and everything made is destroyed. */
#include "guest/suite/draw_work.hpp"
#include "guest/suite/guest_report.hpp"
#include "guest/suite/probe_device.hpp"
#include "guest/suite/probe_outcome.hpp"
#include "guest/suite/scenario_steps.hpp"
#include "guest/suite/shaders/solid_spv.hpp"
#include "guest/suite/shaders/triangle_spv.hpp"
#include "harness/guest/scripted_image_sink.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "harness/guest/scripted_vulkan.hpp"

#include <cstdint>
#include <cstring>

namespace av {

struct DrawWorkProbe {
    static void framebuffer_without_pass(DrawWork &work) { work.render_pass_ = VK_NULL_HANDLE; }
    static void pipeline_without_layout(DrawWork &work) { work.layout_ = VK_NULL_HANDLE; }
    /* The readback's mapping, to play a driver's rounding into. */
    static unsigned char *readback(DrawWork &work) { return work.readback_.bytes(); }
    static std::uint32_t coloured(const DrawWork &work) { return work.coloured(); }
};

}  // namespace av

using av::DrawWork;
using av::DrawWorkProbe;
using av::GuestReport;
using av::MockKmt;
using av::ProbeDevice;
using av::ProbeOutcome;
using av::ScenarioSteps;
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

class DrawRig {
public:
    DrawRig() : steps_(kmt_, report_, "vulkan") {}

    ScriptedVulkan &vulkan() { return vulkan_; }
    const av::ScriptedImageSink &images() const { return images_; }

    const char *run()
    {
        {
            ProbeDevice device(vulkan_);
            make(device);
            DrawWork     work(device, images_);
            ProbeOutcome outcome;
            CHECK(work.invariants_hold());
            work.run(outcome);
            CHECK(work.invariants_hold());
            outcome.report(steps_, "draw");
        }
        CHECK_EQ(vulkan_.live(), 0u);
        CHECK_EQ(vulkan_.misuses(), 0u);
        return report_.text();
    }

private:
    ScriptedVulkan         vulkan_;
    av::ScriptedImageSink images_;
    MockKmt        kmt_;
    GuestReport    report_;
    ScenarioSteps  steps_;
};

}  // namespace

TEST(draw_work, every_pixel_comes_back_the_colour)
{ DrawRig rig;
  CHECK(std::strcmp(rig.run(), "ok vulkan.draw 4096\n") == 0);
  CHECK_EQ(rig.vulkan().drawn_vertices(), 3u);
  CHECK_EQ(rig.vulkan().shader_words(), av::triangle_spv_words + av::solid_spv_words);
  CHECK_EQ(rig.vulkan().calls(Call::queue_submit), 1u);
  /* The image in the device-local type, the readback in the host-visible one. */
  CHECK_EQ(rig.vulkan().allocated_type(0u), 0u);
  CHECK_EQ(rig.vulkan().allocated_type(1u), 1u);
  /* The picture of the 64x64 render: 192 bytes a row, the colour in BGR. */
  CHECK_EQ(rig.images().saved(), 1u);
  CHECK_EQ(rig.images().size("vulkan-draw"), 54u + 64u * 192u);
  const std::uint8_t *const drawn = rig.images().bytes("vulkan-draw");
  CHECK(drawn != nullptr);
  if (drawn != nullptr) {
      const std::uint8_t bgr[3] = {DrawWork::colour[2], DrawWork::colour[1], DrawWork::colour[0]};
      CHECK(std::memcmp(drawn + 54u, bgr, 3u) == 0);
      CHECK(std::memcmp(drawn + 54u + 64u * 192u - 3u, bgr, 3u) == 0);
  } }

/* A render that fails before its copy is made leaves no picture. */
TEST(draw_work, a_draw_that_never_ran_saves_no_picture)
{ ScriptedVulkan vulkan;
  vulkan.fail(Call::create_render_pass, 1u, VK_ERROR_OUT_OF_HOST_MEMORY);
  ProbeDevice device(vulkan);
  make(device);
  av::ScriptedImageSink images;
  { DrawWork work(device, images);
    ProbeOutcome outcome;
    work.run(outcome); }
  CHECK_EQ(images.saved(), 0u); }

TEST(draw_work, a_black_pixel_is_counted_out)
{ DrawRig rig;
  rig.vulkan().misdraw();
  CHECK(std::strcmp(rig.run(), "FAIL vulkan.draw status=4 4095\n") == 0); }

/* 127.5 rounds either way, and every channel may be one off; two off is not
 * the colour, whichever channel it is in. */
TEST(draw_work, a_channel_one_off_is_the_colour_and_two_off_is_not)
{ ScriptedVulkan vulkan;
  ProbeDevice device(vulkan);
  make(device);
  av::ScriptedImageSink images;
  DrawWork work(device, images);
  ProbeOutcome outcome;
  work.run(outcome);
  unsigned char *const bytes = DrawWorkProbe::readback(work);
  bytes[0] = 63u;
  bytes[1] = 127u;
  bytes[2] = 192u;
  bytes[7] = 254u;
  CHECK_EQ(DrawWorkProbe::coloured(work), DrawWork::pixels);
  for (std::uint32_t channel = 0u; channel < 4u; channel++) {
      const unsigned char kept = bytes[8u + channel];
      bytes[8u + channel] = static_cast<unsigned char>(DrawWork::colour[channel] + 2u);
      CHECK_EQ(DrawWorkProbe::coloured(work), DrawWork::pixels - 1u);
      bytes[8u + channel] = static_cast<unsigned char>(DrawWork::colour[channel] - 2u);
      CHECK_EQ(DrawWorkProbe::coloured(work), DrawWork::pixels - 1u);
      bytes[8u + channel] = kept;
  } }

TEST(draw_work, a_submission_that_never_completes_times_out)
{ DrawRig rig;
  rig.vulkan().hold_fences();
  CHECK(std::strcmp(rig.run(), "FAIL vulkan.draw status=7 2\n") == 0); }

TEST(draw_work, a_refusal_at_each_call_is_reported)
{ const Call calls[] = {Call::create_image,        Call::create_image_view,      Call::create_buffer,
                        Call::map_memory,          Call::create_render_pass,     Call::create_framebuffer,
                        Call::create_shader_module, Call::create_pipeline_layout, Call::create_graphics_pipeline,
                        Call::create_command_pool, Call::end_command_buffer};
  for (const Call call : calls) {
      DrawRig rig;
      rig.vulkan().fail(call, 1u, VK_ERROR_OUT_OF_HOST_MEMORY);
      CHECK(std::strcmp(rig.run(), "FAIL vulkan.draw status=7 4294967295\n") == 0);
  }
  DrawRig second;
  second.vulkan().fail(Call::create_shader_module, 2u, VK_ERROR_OUT_OF_HOST_MEMORY);
  CHECK(std::strcmp(second.run(), "FAIL vulkan.draw status=7 4294967295\n") == 0);
  CHECK_EQ(second.vulkan().calls(Call::create_pipeline_layout), 0u); }

TEST(draw_work, an_image_or_readback_with_no_type_is_missing)
{ DrawRig image;
  image.vulkan().set_allowed_types(0x3u, 0x0u);
  CHECK(std::strcmp(image.run(), "FAIL vulkan.draw status=3 0\n") == 0);
  DrawRig readback;
  readback.vulkan().set_allowed_types(0x1u, 0x3u);
  CHECK(std::strcmp(readback.run(), "FAIL vulkan.draw status=3 0\n") == 0);
  CHECK_EQ(readback.vulkan().calls(Call::create_render_pass), 0u); }

TEST(draw_work, invariants_refuse_objects_without_what_they_stand_on)
{ ScriptedVulkan vulkan;
  ProbeDevice device(vulkan);
  make(device);
  av::ScriptedImageSink images;
  DrawWork one(device, images);
  ProbeOutcome outcome;
  one.run(outcome);
  DrawWorkProbe::framebuffer_without_pass(one);
  CHECK(!one.invariants_hold());
  DrawWork two(device, images);
  two.run(outcome);
  DrawWorkProbe::pipeline_without_layout(two);
  CHECK(!two.invariants_hold()); }
