#include "guest/probe/draw_work.hpp"

#include "core/trace/function_scope.hpp"
#include "guest/probe/bmp_image.hpp"
#include "guest/probe/image_sink.hpp"
#include "guest/probe/probe_device.hpp"
#include "guest/probe/probe_colour.hpp"
#include "guest/probe/probe_outcome.hpp"
#include "guest/probe/vulkan_entry.hpp"

namespace av {

namespace {

constexpr VkDeviceSize pixel_bytes = 4u;
constexpr uint32_t     triangle_vertices = 3u;

}

DrawWork::DrawWork(ProbeDevice &device, ImageSink &images)
    : device_(device), images_(images), image_(device), readback_(device), commands_(device), render_pass_(VK_NULL_HANDLE),
      framebuffer_(VK_NULL_HANDLE), triangle_(device)
{
    AV_SCOPE("DrawWork::DrawWork");
}

DrawWork::~DrawWork()
{
    AV_SCOPE("DrawWork::~DrawWork");
    VulkanEntry   &vulkan = device_.vulkan();
    const VkDevice device = device_.device();
    triangle_.release();
    vulkan.destroy_framebuffer(device, framebuffer_);
    vulkan.destroy_render_pass(device, render_pass_);
}

void DrawWork::run(ProbeOutcome &outcome)
{
    AV_SCOPE("DrawWork::run");
    if (!image_.make(extent, outcome)) return;
    if (!readback_.make(pixels * pixel_bytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT, outcome)) return;
    if (!make_render_pass(outcome) || !triangle_.make(outcome, render_pass_, extent, extent)) return;
    if (!commands_.begin(outcome)) return;
    record();
    if (!commands_.run(outcome)) return;
    images_.save("vulkan-draw", BmpImage(readback_.bytes(), extent, extent, BmpImage::Rows::top_first));
    outcome.measure(coloured(), pixels);
}

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
    vulkan.cmd_bind_pipeline(buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, triangle_.pipeline());
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
    return ProbeColour(colour, sizeof colour).matched(readback_.bytes(), pixels, static_cast<uint32_t>(pixel_bytes));
}

bool DrawWork::invariants_hold() const
{
    AV_SCOPE("DrawWork::invariants_hold");
    if (framebuffer_ != VK_NULL_HANDLE && render_pass_ == VK_NULL_HANDLE) return false;
    return triangle_.invariants_hold();
}

}
