#include "guest/probe/probe_device.hpp"
#include "guest/probe/probe_outcome.hpp"
#include "guest/probe/triangle_pipeline.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/guest/scripted_vulkan.hpp"

#include <cstdint>

namespace av {

struct TrianglePipelineProbe {
    static VkPipelineLayout forget_layout(TrianglePipeline &pipeline)
    {
        const VkPipelineLayout layout = pipeline.layout_;
        pipeline.layout_ = VK_NULL_HANDLE;
        return layout;
    }
};

}

using av::ProbeDevice;
using av::ProbeOutcome;
using av::ScriptedVulkan;
using av::TrianglePipeline;
using av::TrianglePipelineProbe;
using Call = av::ScriptedVulkan::Call;

namespace {

constexpr std::uint32_t width = 64u;
constexpr std::uint32_t height = 48u;

void make(ProbeDevice &device)
{
    CHECK_EQ(device.make_instance(), VK_SUCCESS);
    CHECK_EQ(device.list_physical_devices(), VK_SUCCESS);
    CHECK(device.choose(1u));
    CHECK(device.choose_family());
    CHECK_EQ(device.make_device(), VK_SUCCESS);
}

VkRenderPass render_pass(ProbeDevice &device)
{
    VkAttachmentDescription colour = {};
    colour.format = VK_FORMAT_R8G8B8A8_UNORM;
    colour.samples = VK_SAMPLE_COUNT_1_BIT;
    colour.finalLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    VkAttachmentReference reference = {0u, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkSubpassDescription  subpass = {};
    subpass.colorAttachmentCount = 1u;
    subpass.pColorAttachments = &reference;
    VkRenderPassCreateInfo pass = {};
    pass.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    pass.attachmentCount = 1u;
    pass.pAttachments = &colour;
    pass.subpassCount = 1u;
    pass.pSubpasses = &subpass;
    VkRenderPass made = VK_NULL_HANDLE;
    CHECK_EQ(device.vulkan().create_render_pass(device.device(), &pass, &made), VK_SUCCESS);
    return made;
}

}

TEST(triangle_pipeline, is_made_over_both_shaders_and_a_layout_and_released_whole)
{ ScriptedVulkan vulkan;
  {
      ProbeDevice device(vulkan);
      make(device);
      const VkRenderPass pass = render_pass(device);
      TrianglePipeline   pipeline(device);
      CHECK(pipeline.pipeline() == VK_NULL_HANDLE);
      CHECK(pipeline.invariants_hold());
      ProbeOutcome outcome;
      CHECK(pipeline.make(outcome, pass, width, height));
      CHECK(pipeline.pipeline() != VK_NULL_HANDLE);
      CHECK(pipeline.invariants_hold());
      CHECK_EQ(vulkan.calls(Call::create_shader_module), 2u);
      pipeline.release();
      CHECK(pipeline.pipeline() == VK_NULL_HANDLE);
      pipeline.release();
      vulkan.destroy_render_pass(device.device(), pass);
  }
  CHECK_EQ(vulkan.live(), 0u);
  CHECK_EQ(vulkan.misuses(), 0u); }

TEST(triangle_pipeline, a_refusal_at_any_step_fails_the_make_and_its_destructor_cleans_up)
{ const Call calls[] = {Call::create_shader_module, Call::create_pipeline_layout, Call::create_graphics_pipeline};
  for (const Call call : calls) {
      ScriptedVulkan vulkan;
      {
          ProbeDevice device(vulkan);
          make(device);
          const VkRenderPass pass = render_pass(device);
          {
              TrianglePipeline pipeline(device);
              vulkan.fail(call, 1u, VK_ERROR_OUT_OF_HOST_MEMORY);
              ProbeOutcome outcome;
              CHECK(!pipeline.make(outcome, pass, width, height));
              CHECK(pipeline.pipeline() == VK_NULL_HANDLE);
          }
          vulkan.destroy_render_pass(device.device(), pass);
      }
      CHECK_EQ(vulkan.live(), 0u);
  }
  ScriptedVulkan vulkan;
  {
      ProbeDevice device(vulkan);
      make(device);
      const VkRenderPass pass = render_pass(device);
      {
          TrianglePipeline pipeline(device);
          vulkan.fail(Call::create_shader_module, 2u, VK_ERROR_OUT_OF_HOST_MEMORY);
          ProbeOutcome outcome;
          CHECK(!pipeline.make(outcome, pass, width, height));
          CHECK_EQ(vulkan.calls(Call::create_pipeline_layout), 0u);
      }
      vulkan.destroy_render_pass(device.device(), pass);
  }
  CHECK_EQ(vulkan.live(), 0u); }

TEST(triangle_pipeline, a_pipeline_without_its_layout_breaks_the_invariant)
{ ScriptedVulkan vulkan;
  ProbeDevice device(vulkan);
  make(device);
  const VkRenderPass pass = render_pass(device);
  TrianglePipeline   pipeline(device);
  ProbeOutcome       outcome;
  CHECK(pipeline.make(outcome, pass, width, height));
  const VkPipelineLayout layout = TrianglePipelineProbe::forget_layout(pipeline);
  CHECK(!pipeline.invariants_hold());
  vulkan.destroy_pipeline_layout(device.device(), layout);
  vulkan.destroy_render_pass(device.device(), pass); }
