#include "guest/probe/triangle_pipeline.hpp"

#include "core/trace/function_scope.hpp"
#include "guest/probe/probe_device.hpp"
#include "guest/probe/probe_outcome.hpp"
#include "guest/probe/shaders/solid_spv.hpp"
#include "guest/probe/shaders/triangle_spv.hpp"
#include "guest/probe/vulkan_entry.hpp"

namespace av {

TrianglePipeline::TrianglePipeline(ProbeDevice &device)
    : device_(device), vertex_(VK_NULL_HANDLE), fragment_(VK_NULL_HANDLE), layout_(VK_NULL_HANDLE),
      pipeline_(VK_NULL_HANDLE)
{
    AV_SCOPE("TrianglePipeline::TrianglePipeline");
}

TrianglePipeline::~TrianglePipeline()
{
    AV_SCOPE("TrianglePipeline::~TrianglePipeline");
    release();
}

/* No vertex input, one viewport over the attachment, nothing culled or
 * blended. */
bool TrianglePipeline::make(ProbeOutcome &outcome, VkRenderPass pass, uint32_t width, uint32_t height)
{
    AV_SCOPE("TrianglePipeline::make");
    if (!make_shaders(outcome)) return false;
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
    const VkViewport viewport = {0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f};
    const VkRect2D   scissor = {{0, 0}, {width, height}};
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
    pipeline.renderPass = pass;
    return outcome.accept(device_.vulkan().create_graphics_pipeline(device_.device(), &pipeline, &pipeline_));
}

bool TrianglePipeline::make_shaders(ProbeOutcome &outcome)
{
    AV_SCOPE("TrianglePipeline::make_shaders");
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

VkPipeline TrianglePipeline::pipeline() const
{
    AV_SCOPE("TrianglePipeline::pipeline");
    return pipeline_;
}

void TrianglePipeline::release()
{
    AV_SCOPE("TrianglePipeline::release");
    VulkanEntry   &vulkan = device_.vulkan();
    const VkDevice device = device_.device();
    vulkan.destroy_pipeline(device, pipeline_);
    vulkan.destroy_pipeline_layout(device, layout_);
    vulkan.destroy_shader_module(device, fragment_);
    vulkan.destroy_shader_module(device, vertex_);
    pipeline_ = VK_NULL_HANDLE;
    layout_ = VK_NULL_HANDLE;
    fragment_ = VK_NULL_HANDLE;
    vertex_ = VK_NULL_HANDLE;
}

bool TrianglePipeline::invariants_hold() const
{
    AV_SCOPE("TrianglePipeline::invariants_hold");
    return pipeline_ == VK_NULL_HANDLE ||
           (layout_ != VK_NULL_HANDLE && vertex_ != VK_NULL_HANDLE && fragment_ != VK_NULL_HANDLE);
}

}  // namespace av
