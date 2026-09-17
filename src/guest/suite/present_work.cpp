#include "guest/suite/present_work.hpp"

#include "core/trace/function_scope.hpp"
#include "guest/suite/bmp_image.hpp"
#include "guest/suite/image_sink.hpp"
#include "guest/suite/probe_commands.hpp"
#include "guest/suite/probe_device.hpp"
#include "guest/suite/probe_outcome.hpp"
#include "guest/suite/shaders/solid_spv.hpp"
#include "guest/suite/shaders/triangle_spv.hpp"
#include "guest/suite/vulkan_entry.hpp"
#include "guest/suite/window_frame.hpp"

namespace av {

namespace {

constexpr uint32_t pixel_bytes = 4u;
constexpr uint32_t triangle_vertices = 3u;
/* The window's format: what a desktop's DIB holds, so the driver's present
 * copies it out without turning the channels around. */
constexpr VkFormat window_format = VK_FORMAT_B8G8R8A8_UNORM;

}  // namespace

PresentWork::PresentWork(ProbeDevice &device, WindowFrame &windows, ImageSink &pictures, void *module, void *window)
    : device_(device), windows_(windows), pictures_(pictures), module_(module), window_(window),
      surface_(VK_NULL_HANDLE), swapchain_(VK_NULL_HANDLE), format_(window_format), width_(0u), height_(0u), images_{},
      views_{}, framebuffers_{}, count_(0u), render_pass_(VK_NULL_HANDLE), vertex_(VK_NULL_HANDLE),
      fragment_(VK_NULL_HANDLE), layout_(VK_NULL_HANDLE), pipeline_(VK_NULL_HANDLE), presented_(0u), frame_{}
{
    AV_SCOPE("PresentWork::PresentWork");
}

PresentWork::~PresentWork()
{
    AV_SCOPE("PresentWork::~PresentWork");
    release_swapchain();
    if (surface_ != VK_NULL_HANDLE) device_.vulkan().destroy_surface(device_.instance(), surface_);
}

bool PresentWork::make_surface(ProbeOutcome &outcome)
{
    AV_SCOPE("PresentWork::make_surface");
    VulkanEntry &vulkan = device_.vulkan();
    if (!outcome.accept(vulkan.create_win32_surface(device_.instance(), module_, window_, &surface_))) return false;
    VkSurfaceCapabilitiesKHR capabilities = {};
    if (!outcome.accept(vulkan.surface_capabilities(device_.physical_device(), surface_, &capabilities))) return false;
    uint32_t width = 0u;
    uint32_t height = 0u;
    if (!windows_.size(window_, width, height)) {
        outcome.missing();
        return false;
    }
    if (capabilities.currentExtent.width != width || capabilities.currentExtent.height != height ||
        capabilities.minImageCount > images) {
        outcome.missing();
        return false;
    }
    width_ = capabilities.currentExtent.width;
    height_ = capabilities.currentExtent.height;
    uint32_t           format_count = 0u;
    VkSurfaceFormatKHR formats[8] = {};
    format_count = 8u;
    if (!outcome.accept(vulkan.surface_formats(device_.physical_device(), surface_, &format_count, formats)))
        return false;
    bool offers_format = false;
    for (uint32_t index = 0u; index < format_count; index++)
        offers_format = offers_format || (formats[index].format == window_format &&
                                          formats[index].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR);
    uint32_t         mode_count = 8u;
    VkPresentModeKHR modes[8] = {};
    if (!outcome.accept(vulkan.surface_present_modes(device_.physical_device(), surface_, &mode_count, modes)))
        return false;
    bool fifo = false;
    bool immediate = false;
    for (uint32_t index = 0u; index < mode_count; index++) {
        fifo = fifo || modes[index] == VK_PRESENT_MODE_FIFO_KHR;
        immediate = immediate || modes[index] == VK_PRESENT_MODE_IMMEDIATE_KHR;
    }
    if (!offers_format || !fifo || !immediate) {
        outcome.missing();
        return false;
    }
    return true;
}

bool PresentWork::make_swapchain(VkPresentModeKHR mode, ProbeOutcome &outcome)
{
    AV_SCOPE("PresentWork::make_swapchain");
    VulkanEntry &vulkan = device_.vulkan();
    uint32_t     width = 0u;
    uint32_t     height = 0u;
    if (!windows_.size(window_, width, height) || width > max_side || height > max_side) {
        outcome.missing();
        return false;
    }
    VkSwapchainCreateInfoKHR info = {};
    info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    info.surface = surface_;
    info.minImageCount = images;
    info.imageFormat = window_format;
    info.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    info.imageExtent = {width, height};
    info.imageArrayLayers = 1u;
    info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    info.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    info.presentMode = mode;
    info.clipped = VK_TRUE;
    /* The swapchain this one replaces, as an application names it: the driver
     * retires it, and what it holds is still there until it is destroyed - so
     * the new swapchain's memory is found while the old one's is still in
     * place, which is what a resize does. */
    info.oldSwapchain = swapchain_;
    VkSwapchainKHR made = VK_NULL_HANDLE;
    if (!outcome.accept(vulkan.create_swapchain(device_.device(), &info, &made))) return false;
    release_swapchain();
    swapchain_ = made;
    width_ = width;
    height_ = height;
    uint32_t count = images;
    if (!outcome.accept(vulkan.swapchain_images(device_.device(), swapchain_, &count, images_))) return false;
    if (count != images) {
        outcome.missing();
        return false;
    }
    count_ = count;
    return make_render_pass(outcome) && make_shaders(outcome) && make_pipeline(outcome) && make_framebuffers(outcome);
}

/* The swapchain's images are left where a present takes them from. */
bool PresentWork::make_render_pass(ProbeOutcome &outcome)
{
    AV_SCOPE("PresentWork::make_render_pass");
    VkAttachmentDescription attachment = {};
    attachment.format = format_;
    attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
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
    return outcome.accept(device_.vulkan().create_render_pass(device_.device(), &pass, &render_pass_));
}

bool PresentWork::make_shaders(ProbeOutcome &outcome)
{
    AV_SCOPE("PresentWork::make_shaders");
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

bool PresentWork::make_pipeline(ProbeOutcome &outcome)
{
    AV_SCOPE("PresentWork::make_pipeline");
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
    const VkViewport viewport = {0.0f, 0.0f, static_cast<float>(width_), static_cast<float>(height_), 0.0f, 1.0f};
    const VkRect2D   scissor = {{0, 0}, {width_, height_}};
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

bool PresentWork::make_framebuffers(ProbeOutcome &outcome)
{
    AV_SCOPE("PresentWork::make_framebuffers");
    VulkanEntry &vulkan = device_.vulkan();
    for (uint32_t index = 0u; index < count_; index++) {
        VkImageViewCreateInfo view = {};
        view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view.image = images_[index];
        view.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view.format = format_;
        view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        view.subresourceRange.levelCount = 1u;
        view.subresourceRange.layerCount = 1u;
        if (!outcome.accept(vulkan.create_image_view(device_.device(), &view, &views_[index]))) return false;
        VkFramebufferCreateInfo framebuffer = {};
        framebuffer.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebuffer.renderPass = render_pass_;
        framebuffer.attachmentCount = 1u;
        framebuffer.pAttachments = &views_[index];
        framebuffer.width = width_;
        framebuffer.height = height_;
        framebuffer.layers = 1u;
        if (!outcome.accept(vulkan.create_framebuffer(device_.device(), &framebuffer, &framebuffers_[index])))
            return false;
    }
    return true;
}

VkResult PresentWork::present_frames(uint32_t count, ProbeOutcome &outcome)
{
    AV_SCOPE("PresentWork::present_frames");
    for (uint32_t frame = 0u; frame < count; frame++) {
        uint32_t       index = 0u;
        const VkResult acquired =
            device_.vulkan().acquire_next_image(device_.device(), swapchain_, acquire_timeout_ns, &index);
        if (acquired != VK_SUCCESS) return acquired;
        const VkResult presented = draw_and_present(index, outcome);
        if (presented != VK_SUCCESS) return presented;
        presented_++;
    }
    return VK_SUCCESS;
}

/* One frame: the triangle into the acquired image, then the present that puts
 * it on the window. */
VkResult PresentWork::draw_and_present(uint32_t index, ProbeOutcome &outcome)
{
    AV_SCOPE("PresentWork::draw_and_present");
    VulkanEntry &vulkan = device_.vulkan();
    {
        ProbeCommands commands(device_);
        if (!commands.begin(outcome)) return VK_ERROR_INITIALIZATION_FAILED;
        const VkClearValue    black = {};
        VkRenderPassBeginInfo pass = {};
        pass.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        pass.renderPass = render_pass_;
        pass.framebuffer = framebuffers_[index];
        pass.renderArea = {{0, 0}, {width_, height_}};
        pass.clearValueCount = 1u;
        pass.pClearValues = &black;
        vulkan.cmd_begin_render_pass(commands.buffer(), &pass);
        vulkan.cmd_bind_pipeline(commands.buffer(), VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
        vulkan.cmd_draw(commands.buffer(), triangle_vertices);
        vulkan.cmd_end_render_pass(commands.buffer());
        if (!commands.run(outcome)) return VK_ERROR_INITIALIZATION_FAILED;
    }
    VkPresentInfoKHR present = {};
    present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present.swapchainCount = 1u;
    present.pSwapchains = &swapchain_;
    present.pImageIndices = &index;
    return vulkan.queue_present(device_.queue(), &present);
}

uint32_t PresentWork::coloured_on_the_window(const char *name, ProbeOutcome &outcome)
{
    AV_SCOPE("PresentWork::coloured_on_the_window");
    if (!windows_.read(window_, width_, height_, frame_)) {
        outcome.missing();
        return 0u;
    }
    /* A window's DIB is bottom row first, which is how it is kept. */
    pictures_.save(name, BmpImage(frame_, width_, height_, BmpImage::Rows::bottom_first, BmpImage::Order::blue_first));
    uint32_t matched = 0u;
    for (uint32_t pixel = 0u; pixel < width_ * height_; pixel++) {
        const uint8_t *const bytes = &frame_[pixel * pixel_bytes];
        /* The alpha a window keeps is the desktop's, not the frame's. */
        bool                 same = true;
        for (uint32_t channel = 0u; channel < 3u; channel++) same = same && bytes[channel] == window_colour[channel];
        matched += same ? 1u : 0u;
    }
    return matched;
}

/* Everything of the swapchain that was there before this one. */
void PresentWork::release_swapchain()
{
    AV_SCOPE("PresentWork::release_swapchain");
    VulkanEntry &vulkan = device_.vulkan();
    for (uint32_t index = 0u; index < images; index++) {
        vulkan.destroy_framebuffer(device_.device(), framebuffers_[index]);
        vulkan.destroy_image_view(device_.device(), views_[index]);
        framebuffers_[index] = VK_NULL_HANDLE;
        views_[index] = VK_NULL_HANDLE;
        images_[index] = VK_NULL_HANDLE;
    }
    release_pipeline();
    if (swapchain_ != VK_NULL_HANDLE) vulkan.destroy_swapchain(device_.device(), swapchain_);
    swapchain_ = VK_NULL_HANDLE;
    count_ = 0u;
}

void PresentWork::release_pipeline()
{
    AV_SCOPE("PresentWork::release_pipeline");
    VulkanEntry &vulkan = device_.vulkan();
    vulkan.destroy_pipeline(device_.device(), pipeline_);
    vulkan.destroy_pipeline_layout(device_.device(), layout_);
    vulkan.destroy_shader_module(device_.device(), fragment_);
    vulkan.destroy_shader_module(device_.device(), vertex_);
    vulkan.destroy_render_pass(device_.device(), render_pass_);
    pipeline_ = VK_NULL_HANDLE;
    layout_ = VK_NULL_HANDLE;
    fragment_ = VK_NULL_HANDLE;
    vertex_ = VK_NULL_HANDLE;
    render_pass_ = VK_NULL_HANDLE;
}

uint32_t PresentWork::width() const
{
    AV_SCOPE("PresentWork::width");
    return width_;
}

uint32_t PresentWork::height() const
{
    AV_SCOPE("PresentWork::height");
    return height_;
}

uint32_t PresentWork::presented() const
{
    AV_SCOPE("PresentWork::presented");
    return presented_;
}

bool PresentWork::invariants_hold() const
{
    AV_SCOPE("PresentWork::invariants_hold");
    if (count_ > images) return false;
    if (swapchain_ != VK_NULL_HANDLE && surface_ == VK_NULL_HANDLE) return false;
    if (swapchain_ == VK_NULL_HANDLE && count_ != 0u) return false;
    for (uint32_t index = 0u; index < images; index++)
        if (framebuffers_[index] != VK_NULL_HANDLE && views_[index] == VK_NULL_HANDLE) return false;
    return width_ <= max_side && height_ <= max_side;
}

}  // namespace av
