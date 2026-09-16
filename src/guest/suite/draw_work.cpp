#include "guest/suite/draw_work.hpp"

#include "core/trace/function_scope.hpp"
#include "guest/suite/probe_device.hpp"
#include "guest/suite/probe_outcome.hpp"
#include "guest/suite/shaders/solid_spv.hpp"
#include "guest/suite/shaders/triangle_spv.hpp"
#include "guest/suite/vulkan_entry.hpp"

namespace av {

namespace {

constexpr VkDeviceSize pixel_bytes = 4u;
constexpr uint32_t     triangle_vertices = 3u;
constexpr float        full_extent = static_cast<float>(DrawWork::extent);

}  // namespace

DrawWork::DrawWork(ProbeDevice &device)
    : device_(device), image_(device), readback_(device), commands_(device), render_pass_(VK_NULL_HANDLE),
      framebuffer_(VK_NULL_HANDLE), vertex_(VK_NULL_HANDLE), fragment_(VK_NULL_HANDLE), layout_(VK_NULL_HANDLE),
      pipeline_(VK_NULL_HANDLE)
      {
          AV_SCOPE("DrawWork::DrawWork");
      }

/* Vulkan makes destroying VK_NULL_HANDLE a no-op. */
DrawWork::~DrawWork()
{
    AV_SCOPE("DrawWork::~DrawWork");
    VulkanEntry   &vulkan = device_.vulkan();
    const VkDevice device = device_.device();
    vulkan.destroy_pipeline(device, pipeline_);
    vulkan.destroy_pipeline_layout(device, layout_);
    vulkan.destroy_shader_module(device, fragment_);
    vulkan.destroy_shader_module(device, vertex_);
    vulkan.destroy_framebuffer(device, framebuffer_);
    vulkan.destroy_render_pass(device, render_pass_);
}

void DrawWork::run(ProbeOutcome &outcome)
{
    AV_SCOPE("DrawWork::run");
    if (!image_.make(extent, outcome)) return;
    if (!readback_.make(pixels * pixel_bytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT, outcome)) return;
    if (!make_render_pass(outcome) || !make_shaders(outcome) || !make_pipeline(outcome)) return;
    if (!commands_.begin(outcome)) return;
    record();
    if (!commands_.run(outcome)) return;
    outcome.measure(coloured(), pixels);
}

/* Cleared, drawn, and left ready to be copied out. */
bool DrawWork::make_render_pass(ProbeOutcome &outcome)
{
    AV_SCOPE("DrawWork::make_render_pass");
    VkAttachmentDescription attachment = {};
    attachment.format = VK_FORMAT_R8G8B8A8_UNORM;
    attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachment.finalLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    const VkAttachmentReference colour_reference = {0u, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1u;
    subpass.pColorAttachments = &colour_reference;
    VkRenderPassCreateInfo pass = {};
    pass.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    pass.attachmentCount = 1u;
    pass.pAttachments = &attachment;
    pass.subpassCount = 1u;
    pass.pSubpasses = &subpass;
    VulkanEntry &vulkan = device_.vulkan();
    if (!outcome.accept(vulkan.create_render_pass(device_.device(), &pass, &render_pass_))) return false;
    const VkImageView view = image_.view();
    VkFramebufferCreateInfo framebuffer = {};
    framebuffer.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebuffer.renderPass = render_pass_;
    framebuffer.attachmentCount = 1u;
    framebuffer.pAttachments = &view;
    framebuffer.width = extent;
    framebuffer.height = extent;
    framebuffer.layers = 1u;
    return outcome.accept(vulkan.create_framebuffer(device_.device(), &framebuffer, &framebuffer_));
}

bool DrawWork::make_shaders(ProbeOutcome &outcome)
{
    AV_SCOPE("DrawWork::make_shaders");
    VulkanEntry &vulkan = device_.vulkan();
    VkShaderModuleCreateInfo vertex = {};
    vertex.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    vertex.codeSize = sizeof triangle_spv;
    vertex.pCode = triangle_spv;
    if (!outcome.accept(vulkan.create_shader_module(device_.device(), &vertex, &vertex_))) return false;
    VkShaderModuleCreateInfo fragment = {};
    fragment.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    fragment.codeSize = sizeof solid_spv;
    fragment.pCode = solid_spv;
    if (!outcome.accept(vulkan.create_shader_module(device_.device(), &fragment, &fragment_))) return false;
    VkPipelineLayoutCreateInfo layout = {};
    layout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    return outcome.accept(vulkan.create_pipeline_layout(device_.device(), &layout, &layout_));
}

/* No vertex input, one viewport over the image, nothing culled or blended. */
bool DrawWork::make_pipeline(ProbeOutcome &outcome)
{
    AV_SCOPE("DrawWork::make_pipeline");
    VkPipelineShaderStageCreateInfo stages[2] = {};
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vertex_;
    stages[0].pName = "main";
    stages[1] = stages[0];
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = fragment_;
    VkPipelineVertexInputStateCreateInfo input = {};
    input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    VkPipelineInputAssemblyStateCreateInfo assembly = {};
    assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    const VkViewport viewport = {0.0f, 0.0f, full_extent, full_extent, 0.0f, 1.0f};
    const VkRect2D   scissor = {{0, 0}, {extent, extent}};
    VkPipelineViewportStateCreateInfo viewports = {};
    viewports.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewports.viewportCount = 1u;
    viewports.pViewports = &viewport;
    viewports.scissorCount = 1u;
    viewports.pScissors = &scissor;
    VkPipelineRasterizationStateCreateInfo raster = {};
    raster.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    raster.polygonMode = VK_POLYGON_MODE_FILL;
    raster.cullMode = VK_CULL_MODE_NONE;
    raster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    raster.lineWidth = 1.0f;
    VkPipelineMultisampleStateCreateInfo multisample = {};
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    VkPipelineColorBlendAttachmentState blend = {};
    blend.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
                           VK_COLOR_COMPONENT_A_BIT;
    VkPipelineColorBlendStateCreateInfo blending = {};
    blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    blending.attachmentCount = 1u;
    blending.pAttachments = &blend;
    VkGraphicsPipelineCreateInfo pipeline = {};
    pipeline.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline.stageCount = 2u;
    pipeline.pStages = stages;
    pipeline.pVertexInputState = &input;
    pipeline.pInputAssemblyState = &assembly;
    pipeline.pViewportState = &viewports;
    pipeline.pRasterizationState = &raster;
    pipeline.pMultisampleState = &multisample;
    pipeline.pColorBlendState = &blending;
    pipeline.layout = layout_;
    pipeline.renderPass = render_pass_;
    return outcome.accept(device_.vulkan().create_graphics_pipeline(device_.device(), &pipeline, &pipeline_));
}

void DrawWork::record()
{
    AV_SCOPE("DrawWork::record");
    VulkanEntry          &vulkan = device_.vulkan();
    const VkCommandBuffer buffer = commands_.buffer();
    const VkClearValue    black = {};
    VkRenderPassBeginInfo pass = {};
    pass.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    pass.renderPass = render_pass_;
    pass.framebuffer = framebuffer_;
    pass.renderArea = {{0, 0}, {extent, extent}};
    pass.clearValueCount = 1u;
    pass.pClearValues = &black;
    vulkan.cmd_begin_render_pass(buffer, &pass);
    vulkan.cmd_bind_pipeline(buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
    vulkan.cmd_draw(buffer, triangle_vertices);
    vulkan.cmd_end_render_pass(buffer);
    VkBufferImageCopy region = {};
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.layerCount = 1u;
    region.imageExtent = {extent, extent, 1u};
    vulkan.cmd_copy_image_to_buffer(buffer, image_.image(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                    readback_.buffer(), &region);
}

uint32_t DrawWork::coloured() const
{
    AV_SCOPE("DrawWork::coloured");
    uint32_t matched = 0u;
    for (uint32_t pixel = 0u; pixel < pixels; pixel++) {
        const unsigned char *const at = readback_.bytes() + pixel * pixel_bytes;
        bool                       close = true;
        for (uint32_t channel = 0u; channel < pixel_bytes; channel++) {
            const int difference = static_cast<int>(at[channel]) - static_cast<int>(colour[channel]);
            close = close && difference >= -1 && difference <= 1;
        }
        matched += close ? 1u : 0u;
    }
    return matched;
}

bool DrawWork::invariants_hold() const
{
    AV_SCOPE("DrawWork::invariants_hold");
    if (framebuffer_ != VK_NULL_HANDLE && render_pass_ == VK_NULL_HANDLE) return false;
    return pipeline_ == VK_NULL_HANDLE || layout_ != VK_NULL_HANDLE;
}

}  // namespace av
