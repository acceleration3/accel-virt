#include "guest/suite/loader_vulkan.hpp"

#include "core/trace/function_scope.hpp"
#include "icd/vulkan/win32_surface_create_record.hpp"

namespace av {

namespace {

/* In LoaderVulkan::Function order. */
constexpr const char *const names[] = {
    "vkDestroyInstance",
    "vkEnumeratePhysicalDevices",
    "vkGetPhysicalDeviceProperties2",
    "vkGetPhysicalDeviceQueueFamilyProperties",
    "vkGetPhysicalDeviceMemoryProperties",
    "vkCreateDevice",
    "vkDestroyDevice",
    "vkDeviceWaitIdle",
    "vkGetDeviceQueue",
    "vkCreateBuffer",
    "vkDestroyBuffer",
    "vkGetBufferMemoryRequirements",
    "vkBindBufferMemory",
    "vkCreateImage",
    "vkDestroyImage",
    "vkGetImageMemoryRequirements",
    "vkBindImageMemory",
    "vkAllocateMemory",
    "vkFreeMemory",
    "vkMapMemory",
    "vkCreateImageView",
    "vkDestroyImageView",
    "vkCreateShaderModule",
    "vkDestroyShaderModule",
    "vkCreateDescriptorSetLayout",
    "vkDestroyDescriptorSetLayout",
    "vkCreatePipelineLayout",
    "vkDestroyPipelineLayout",
    "vkCreateComputePipelines",
    "vkCreateGraphicsPipelines",
    "vkDestroyPipeline",
    "vkCreateDescriptorPool",
    "vkDestroyDescriptorPool",
    "vkAllocateDescriptorSets",
    "vkUpdateDescriptorSets",
    "vkCreateRenderPass",
    "vkDestroyRenderPass",
    "vkCreateFramebuffer",
    "vkDestroyFramebuffer",
    "vkCreateCommandPool",
    "vkDestroyCommandPool",
    "vkAllocateCommandBuffers",
    "vkCreateFence",
    "vkDestroyFence",
    "vkWaitForFences",
    "vkBeginCommandBuffer",
    "vkEndCommandBuffer",
    "vkCmdBindPipeline",
    "vkCmdBindDescriptorSets",
    "vkCmdDispatch",
    "vkCmdBeginRenderPass",
    "vkCmdEndRenderPass",
    "vkCmdDraw",
    "vkCmdCopyImageToBuffer",
    "vkQueueSubmit",
    "vkCreateWin32SurfaceKHR",
    "vkDestroySurfaceKHR",
    "vkGetPhysicalDeviceSurfaceCapabilitiesKHR",
    "vkGetPhysicalDeviceSurfaceFormatsKHR",
    "vkGetPhysicalDeviceSurfacePresentModesKHR",
    "vkCreateSwapchainKHR",
    "vkDestroySwapchainKHR",
    "vkGetSwapchainImagesKHR",
    "vkAcquireNextImageKHR",
    "vkQueuePresentKHR",
};

constexpr const char force_context_fault_name[] = "vkRvkForceContextFault";

}  // namespace

LoaderVulkan::LoaderVulkan(GetInstanceProcAddr get_instance_proc_addr)
    : get_instance_proc_addr_(get_instance_proc_addr), functions_{}
{
    AV_SCOPE("LoaderVulkan::LoaderVulkan");
    static_assert(sizeof names / sizeof names[0] == function_count);
}

VkResult LoaderVulkan::create_instance(const VkInstanceCreateInfo *info, VkInstance *instance)
{
    AV_SCOPE("LoaderVulkan::create_instance");
    using Create = VkResult (VKAPI_PTR *)(const VkInstanceCreateInfo *, const VkAllocationCallbacks *, VkInstance *);
    const auto     create = reinterpret_cast<Create>(get_instance_proc_addr_(VK_NULL_HANDLE, "vkCreateInstance"));
    const VkResult created = create(info, nullptr, instance);
    if (created != VK_SUCCESS) return created;
    for (uint32_t index = 0u; index < function_count; index++)
        functions_[index] = get_instance_proc_addr_(*instance, names[index]);
    return VK_SUCCESS;
}

VkResult LoaderVulkan::force_context_fault(VkInstance instance, VkPhysicalDevice physical_device)
{
    AV_SCOPE("LoaderVulkan::force_context_fault");
    using Force = VkResult (VKAPI_PTR *)(VkPhysicalDevice);
    const auto force = reinterpret_cast<Force>(get_instance_proc_addr_(instance, force_context_fault_name));
    return force != nullptr ? force(physical_device) : VK_ERROR_EXTENSION_NOT_PRESENT;
}

void LoaderVulkan::destroy_instance(VkInstance instance)
{
    AV_SCOPE("LoaderVulkan::destroy_instance");
    resolved<void (VKAPI_PTR *)(VkInstance, const VkAllocationCallbacks *)>(Function::destroy_instance)(instance, nullptr);
}

VkResult LoaderVulkan::enumerate_physical_devices(VkInstance instance, uint32_t *count, VkPhysicalDevice *physical_devices)
{
    AV_SCOPE("LoaderVulkan::enumerate_physical_devices");
    return resolved<VkResult (VKAPI_PTR *)(VkInstance, uint32_t *, VkPhysicalDevice *)>(Function::enumerate_physical_devices)(instance, count, physical_devices);
}

void LoaderVulkan::physical_device_properties2(VkPhysicalDevice physical_device, VkPhysicalDeviceProperties2 *properties)
{
    AV_SCOPE("LoaderVulkan::physical_device_properties2");
    resolved<void (VKAPI_PTR *)(VkPhysicalDevice, VkPhysicalDeviceProperties2 *)>(Function::physical_device_properties2)(physical_device, properties);
}

void LoaderVulkan::queue_family_properties(VkPhysicalDevice physical_device, uint32_t *count, VkQueueFamilyProperties *properties)
{
    AV_SCOPE("LoaderVulkan::queue_family_properties");
    resolved<void (VKAPI_PTR *)(VkPhysicalDevice, uint32_t *, VkQueueFamilyProperties *)>(Function::queue_family_properties)(physical_device, count, properties);
}

void LoaderVulkan::memory_properties(VkPhysicalDevice physical_device, VkPhysicalDeviceMemoryProperties *properties)
{
    AV_SCOPE("LoaderVulkan::memory_properties");
    resolved<void (VKAPI_PTR *)(VkPhysicalDevice, VkPhysicalDeviceMemoryProperties *)>(Function::memory_properties)(physical_device, properties);
}

VkResult LoaderVulkan::create_device(VkPhysicalDevice physical_device, const VkDeviceCreateInfo *info, VkDevice *device)
{
    AV_SCOPE("LoaderVulkan::create_device");
    return resolved<VkResult (VKAPI_PTR *)(VkPhysicalDevice, const VkDeviceCreateInfo *, const VkAllocationCallbacks *, VkDevice *)>(Function::create_device)(physical_device, info, nullptr, device);
}

void LoaderVulkan::destroy_device(VkDevice device)
{
    AV_SCOPE("LoaderVulkan::destroy_device");
    resolved<void (VKAPI_PTR *)(VkDevice, const VkAllocationCallbacks *)>(Function::destroy_device)(device, nullptr);
}

VkResult LoaderVulkan::device_wait_idle(VkDevice device)
{
    AV_SCOPE("LoaderVulkan::device_wait_idle");
    return resolved<VkResult (VKAPI_PTR *)(VkDevice)>(Function::device_wait_idle)(device);
}

void LoaderVulkan::device_queue(VkDevice device, uint32_t family, uint32_t index, VkQueue *queue)
{
    AV_SCOPE("LoaderVulkan::device_queue");
    resolved<void (VKAPI_PTR *)(VkDevice, uint32_t, uint32_t, VkQueue *)>(Function::device_queue)(device, family, index, queue);
}

VkResult LoaderVulkan::create_buffer(VkDevice device, const VkBufferCreateInfo *info, VkBuffer *buffer)
{
    AV_SCOPE("LoaderVulkan::create_buffer");
    return resolved<VkResult (VKAPI_PTR *)(VkDevice, const VkBufferCreateInfo *, const VkAllocationCallbacks *, VkBuffer *)>(Function::create_buffer)(device, info, nullptr, buffer);
}

void LoaderVulkan::destroy_buffer(VkDevice device, VkBuffer buffer)
{
    AV_SCOPE("LoaderVulkan::destroy_buffer");
    resolved<void (VKAPI_PTR *)(VkDevice, VkBuffer, const VkAllocationCallbacks *)>(Function::destroy_buffer)(device, buffer, nullptr);
}

void LoaderVulkan::buffer_memory_requirements(VkDevice device, VkBuffer buffer, VkMemoryRequirements *requirements)
{
    AV_SCOPE("LoaderVulkan::buffer_memory_requirements");
    resolved<void (VKAPI_PTR *)(VkDevice, VkBuffer, VkMemoryRequirements *)>(Function::buffer_memory_requirements)(device, buffer, requirements);
}

VkResult LoaderVulkan::bind_buffer_memory(VkDevice device, VkBuffer buffer, VkDeviceMemory memory)
{
    AV_SCOPE("LoaderVulkan::bind_buffer_memory");
    return resolved<VkResult (VKAPI_PTR *)(VkDevice, VkBuffer, VkDeviceMemory, VkDeviceSize)>(Function::bind_buffer_memory)(device, buffer, memory, 0u);
}

VkResult LoaderVulkan::create_image(VkDevice device, const VkImageCreateInfo *info, VkImage *image)
{
    AV_SCOPE("LoaderVulkan::create_image");
    return resolved<VkResult (VKAPI_PTR *)(VkDevice, const VkImageCreateInfo *, const VkAllocationCallbacks *, VkImage *)>(Function::create_image)(device, info, nullptr, image);
}

void LoaderVulkan::destroy_image(VkDevice device, VkImage image)
{
    AV_SCOPE("LoaderVulkan::destroy_image");
    resolved<void (VKAPI_PTR *)(VkDevice, VkImage, const VkAllocationCallbacks *)>(Function::destroy_image)(device, image, nullptr);
}

void LoaderVulkan::image_memory_requirements(VkDevice device, VkImage image, VkMemoryRequirements *requirements)
{
    AV_SCOPE("LoaderVulkan::image_memory_requirements");
    resolved<void (VKAPI_PTR *)(VkDevice, VkImage, VkMemoryRequirements *)>(Function::image_memory_requirements)(device, image, requirements);
}

VkResult LoaderVulkan::bind_image_memory(VkDevice device, VkImage image, VkDeviceMemory memory)
{
    AV_SCOPE("LoaderVulkan::bind_image_memory");
    return resolved<VkResult (VKAPI_PTR *)(VkDevice, VkImage, VkDeviceMemory, VkDeviceSize)>(Function::bind_image_memory)(device, image, memory, 0u);
}

VkResult LoaderVulkan::allocate_memory(VkDevice device, const VkMemoryAllocateInfo *info, VkDeviceMemory *memory)
{
    AV_SCOPE("LoaderVulkan::allocate_memory");
    return resolved<VkResult (VKAPI_PTR *)(VkDevice, const VkMemoryAllocateInfo *, const VkAllocationCallbacks *, VkDeviceMemory *)>(Function::allocate_memory)(device, info, nullptr, memory);
}

void LoaderVulkan::free_memory(VkDevice device, VkDeviceMemory memory)
{
    AV_SCOPE("LoaderVulkan::free_memory");
    resolved<void (VKAPI_PTR *)(VkDevice, VkDeviceMemory, const VkAllocationCallbacks *)>(Function::free_memory)(device, memory, nullptr);
}

VkResult LoaderVulkan::map_memory(VkDevice device, VkDeviceMemory memory, void **data)
{
    AV_SCOPE("LoaderVulkan::map_memory");
    return resolved<VkResult (VKAPI_PTR *)(VkDevice, VkDeviceMemory, VkDeviceSize, VkDeviceSize, VkMemoryMapFlags, void **)>(Function::map_memory)(device, memory, 0u, VK_WHOLE_SIZE, 0u, data);
}

VkResult LoaderVulkan::create_image_view(VkDevice device, const VkImageViewCreateInfo *info, VkImageView *view)
{
    AV_SCOPE("LoaderVulkan::create_image_view");
    return resolved<VkResult (VKAPI_PTR *)(VkDevice, const VkImageViewCreateInfo *, const VkAllocationCallbacks *, VkImageView *)>(Function::create_image_view)(device, info, nullptr, view);
}

void LoaderVulkan::destroy_image_view(VkDevice device, VkImageView view)
{
    AV_SCOPE("LoaderVulkan::destroy_image_view");
    resolved<void (VKAPI_PTR *)(VkDevice, VkImageView, const VkAllocationCallbacks *)>(Function::destroy_image_view)(device, view, nullptr);
}

VkResult LoaderVulkan::create_shader_module(VkDevice device, const VkShaderModuleCreateInfo *info, VkShaderModule *module)
{
    AV_SCOPE("LoaderVulkan::create_shader_module");
    return resolved<VkResult (VKAPI_PTR *)(VkDevice, const VkShaderModuleCreateInfo *, const VkAllocationCallbacks *, VkShaderModule *)>(Function::create_shader_module)(device, info, nullptr, module);
}

void LoaderVulkan::destroy_shader_module(VkDevice device, VkShaderModule module)
{
    AV_SCOPE("LoaderVulkan::destroy_shader_module");
    resolved<void (VKAPI_PTR *)(VkDevice, VkShaderModule, const VkAllocationCallbacks *)>(Function::destroy_shader_module)(device, module, nullptr);
}

VkResult LoaderVulkan::create_descriptor_set_layout(VkDevice device, const VkDescriptorSetLayoutCreateInfo *info, VkDescriptorSetLayout *layout)
{
    AV_SCOPE("LoaderVulkan::create_descriptor_set_layout");
    return resolved<VkResult (VKAPI_PTR *)(VkDevice, const VkDescriptorSetLayoutCreateInfo *, const VkAllocationCallbacks *, VkDescriptorSetLayout *)>(Function::create_descriptor_set_layout)(device, info, nullptr, layout);
}

void LoaderVulkan::destroy_descriptor_set_layout(VkDevice device, VkDescriptorSetLayout layout)
{
    AV_SCOPE("LoaderVulkan::destroy_descriptor_set_layout");
    resolved<void (VKAPI_PTR *)(VkDevice, VkDescriptorSetLayout, const VkAllocationCallbacks *)>(Function::destroy_descriptor_set_layout)(device, layout, nullptr);
}

VkResult LoaderVulkan::create_pipeline_layout(VkDevice device, const VkPipelineLayoutCreateInfo *info, VkPipelineLayout *layout)
{
    AV_SCOPE("LoaderVulkan::create_pipeline_layout");
    return resolved<VkResult (VKAPI_PTR *)(VkDevice, const VkPipelineLayoutCreateInfo *, const VkAllocationCallbacks *, VkPipelineLayout *)>(Function::create_pipeline_layout)(device, info, nullptr, layout);
}

void LoaderVulkan::destroy_pipeline_layout(VkDevice device, VkPipelineLayout layout)
{
    AV_SCOPE("LoaderVulkan::destroy_pipeline_layout");
    resolved<void (VKAPI_PTR *)(VkDevice, VkPipelineLayout, const VkAllocationCallbacks *)>(Function::destroy_pipeline_layout)(device, layout, nullptr);
}

VkResult LoaderVulkan::create_compute_pipeline(VkDevice device, const VkComputePipelineCreateInfo *info, VkPipeline *pipeline)
{
    AV_SCOPE("LoaderVulkan::create_compute_pipeline");
    return resolved<VkResult (VKAPI_PTR *)(VkDevice, VkPipelineCache, uint32_t, const VkComputePipelineCreateInfo *, const VkAllocationCallbacks *, VkPipeline *)>(Function::create_compute_pipelines)(device, VK_NULL_HANDLE, 1u, info, nullptr, pipeline);
}

VkResult LoaderVulkan::create_graphics_pipeline(VkDevice device, const VkGraphicsPipelineCreateInfo *info, VkPipeline *pipeline)
{
    AV_SCOPE("LoaderVulkan::create_graphics_pipeline");
    return resolved<VkResult (VKAPI_PTR *)(VkDevice, VkPipelineCache, uint32_t, const VkGraphicsPipelineCreateInfo *, const VkAllocationCallbacks *, VkPipeline *)>(Function::create_graphics_pipelines)(device, VK_NULL_HANDLE, 1u, info, nullptr, pipeline);
}

void LoaderVulkan::destroy_pipeline(VkDevice device, VkPipeline pipeline)
{
    AV_SCOPE("LoaderVulkan::destroy_pipeline");
    resolved<void (VKAPI_PTR *)(VkDevice, VkPipeline, const VkAllocationCallbacks *)>(Function::destroy_pipeline)(device, pipeline, nullptr);
}

VkResult LoaderVulkan::create_descriptor_pool(VkDevice device, const VkDescriptorPoolCreateInfo *info, VkDescriptorPool *pool)
{
    AV_SCOPE("LoaderVulkan::create_descriptor_pool");
    return resolved<VkResult (VKAPI_PTR *)(VkDevice, const VkDescriptorPoolCreateInfo *, const VkAllocationCallbacks *, VkDescriptorPool *)>(Function::create_descriptor_pool)(device, info, nullptr, pool);
}

void LoaderVulkan::destroy_descriptor_pool(VkDevice device, VkDescriptorPool pool)
{
    AV_SCOPE("LoaderVulkan::destroy_descriptor_pool");
    resolved<void (VKAPI_PTR *)(VkDevice, VkDescriptorPool, const VkAllocationCallbacks *)>(Function::destroy_descriptor_pool)(device, pool, nullptr);
}

VkResult LoaderVulkan::allocate_descriptor_set(VkDevice device, const VkDescriptorSetAllocateInfo *info, VkDescriptorSet *set)
{
    AV_SCOPE("LoaderVulkan::allocate_descriptor_set");
    return resolved<VkResult (VKAPI_PTR *)(VkDevice, const VkDescriptorSetAllocateInfo *, VkDescriptorSet *)>(Function::allocate_descriptor_sets)(device, info, set);
}

void LoaderVulkan::update_descriptor_set(VkDevice device, const VkWriteDescriptorSet *write)
{
    AV_SCOPE("LoaderVulkan::update_descriptor_set");
    resolved<void (VKAPI_PTR *)(VkDevice, uint32_t, const VkWriteDescriptorSet *, uint32_t, const VkCopyDescriptorSet *)>(Function::update_descriptor_sets)(device, 1u, write, 0u, nullptr);
}

VkResult LoaderVulkan::create_render_pass(VkDevice device, const VkRenderPassCreateInfo *info, VkRenderPass *render_pass)
{
    AV_SCOPE("LoaderVulkan::create_render_pass");
    return resolved<VkResult (VKAPI_PTR *)(VkDevice, const VkRenderPassCreateInfo *, const VkAllocationCallbacks *, VkRenderPass *)>(Function::create_render_pass)(device, info, nullptr, render_pass);
}

void LoaderVulkan::destroy_render_pass(VkDevice device, VkRenderPass render_pass)
{
    AV_SCOPE("LoaderVulkan::destroy_render_pass");
    resolved<void (VKAPI_PTR *)(VkDevice, VkRenderPass, const VkAllocationCallbacks *)>(Function::destroy_render_pass)(device, render_pass, nullptr);
}

VkResult LoaderVulkan::create_framebuffer(VkDevice device, const VkFramebufferCreateInfo *info, VkFramebuffer *framebuffer)
{
    AV_SCOPE("LoaderVulkan::create_framebuffer");
    return resolved<VkResult (VKAPI_PTR *)(VkDevice, const VkFramebufferCreateInfo *, const VkAllocationCallbacks *, VkFramebuffer *)>(Function::create_framebuffer)(device, info, nullptr, framebuffer);
}

void LoaderVulkan::destroy_framebuffer(VkDevice device, VkFramebuffer framebuffer)
{
    AV_SCOPE("LoaderVulkan::destroy_framebuffer");
    resolved<void (VKAPI_PTR *)(VkDevice, VkFramebuffer, const VkAllocationCallbacks *)>(Function::destroy_framebuffer)(device, framebuffer, nullptr);
}

VkResult LoaderVulkan::create_command_pool(VkDevice device, const VkCommandPoolCreateInfo *info, VkCommandPool *pool)
{
    AV_SCOPE("LoaderVulkan::create_command_pool");
    return resolved<VkResult (VKAPI_PTR *)(VkDevice, const VkCommandPoolCreateInfo *, const VkAllocationCallbacks *, VkCommandPool *)>(Function::create_command_pool)(device, info, nullptr, pool);
}

void LoaderVulkan::destroy_command_pool(VkDevice device, VkCommandPool pool)
{
    AV_SCOPE("LoaderVulkan::destroy_command_pool");
    resolved<void (VKAPI_PTR *)(VkDevice, VkCommandPool, const VkAllocationCallbacks *)>(Function::destroy_command_pool)(device, pool, nullptr);
}

VkResult LoaderVulkan::allocate_command_buffer(VkDevice device, const VkCommandBufferAllocateInfo *info, VkCommandBuffer *command_buffer)
{
    AV_SCOPE("LoaderVulkan::allocate_command_buffer");
    return resolved<VkResult (VKAPI_PTR *)(VkDevice, const VkCommandBufferAllocateInfo *, VkCommandBuffer *)>(Function::allocate_command_buffers)(device, info, command_buffer);
}

VkResult LoaderVulkan::create_fence(VkDevice device, const VkFenceCreateInfo *info, VkFence *fence)
{
    AV_SCOPE("LoaderVulkan::create_fence");
    return resolved<VkResult (VKAPI_PTR *)(VkDevice, const VkFenceCreateInfo *, const VkAllocationCallbacks *, VkFence *)>(Function::create_fence)(device, info, nullptr, fence);
}

void LoaderVulkan::destroy_fence(VkDevice device, VkFence fence)
{
    AV_SCOPE("LoaderVulkan::destroy_fence");
    resolved<void (VKAPI_PTR *)(VkDevice, VkFence, const VkAllocationCallbacks *)>(Function::destroy_fence)(device, fence, nullptr);
}

VkResult LoaderVulkan::wait_for_fence(VkDevice device, VkFence fence, uint64_t timeout)
{
    AV_SCOPE("LoaderVulkan::wait_for_fence");
    return resolved<VkResult (VKAPI_PTR *)(VkDevice, uint32_t, const VkFence *, VkBool32, uint64_t)>(Function::wait_for_fences)(device, 1u, &fence, VK_TRUE, timeout);
}

VkResult LoaderVulkan::begin_command_buffer(VkCommandBuffer command_buffer, const VkCommandBufferBeginInfo *info)
{
    AV_SCOPE("LoaderVulkan::begin_command_buffer");
    return resolved<VkResult (VKAPI_PTR *)(VkCommandBuffer, const VkCommandBufferBeginInfo *)>(Function::begin_command_buffer)(command_buffer, info);
}

VkResult LoaderVulkan::end_command_buffer(VkCommandBuffer command_buffer)
{
    AV_SCOPE("LoaderVulkan::end_command_buffer");
    return resolved<VkResult (VKAPI_PTR *)(VkCommandBuffer)>(Function::end_command_buffer)(command_buffer);
}

void LoaderVulkan::cmd_bind_pipeline(VkCommandBuffer command_buffer, VkPipelineBindPoint point, VkPipeline pipeline)
{
    AV_SCOPE("LoaderVulkan::cmd_bind_pipeline");
    resolved<void (VKAPI_PTR *)(VkCommandBuffer, VkPipelineBindPoint, VkPipeline)>(Function::cmd_bind_pipeline)(command_buffer, point, pipeline);
}

void LoaderVulkan::cmd_bind_compute_set(VkCommandBuffer command_buffer, VkPipelineLayout layout, VkDescriptorSet set)
{
    AV_SCOPE("LoaderVulkan::cmd_bind_compute_set");
    resolved<void (VKAPI_PTR *)(VkCommandBuffer, VkPipelineBindPoint, VkPipelineLayout, uint32_t, uint32_t, const VkDescriptorSet *, uint32_t, const uint32_t *)>(Function::cmd_bind_descriptor_sets)(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, layout, 0u, 1u, &set, 0u, nullptr);
}

void LoaderVulkan::cmd_dispatch(VkCommandBuffer command_buffer, uint32_t groups_x)
{
    AV_SCOPE("LoaderVulkan::cmd_dispatch");
    resolved<void (VKAPI_PTR *)(VkCommandBuffer, uint32_t, uint32_t, uint32_t)>(Function::cmd_dispatch)(command_buffer, groups_x, 1u, 1u);
}

void LoaderVulkan::cmd_begin_render_pass(VkCommandBuffer command_buffer, const VkRenderPassBeginInfo *info)
{
    AV_SCOPE("LoaderVulkan::cmd_begin_render_pass");
    resolved<void (VKAPI_PTR *)(VkCommandBuffer, const VkRenderPassBeginInfo *, VkSubpassContents)>(Function::cmd_begin_render_pass)(command_buffer, info, VK_SUBPASS_CONTENTS_INLINE);
}

void LoaderVulkan::cmd_end_render_pass(VkCommandBuffer command_buffer)
{
    AV_SCOPE("LoaderVulkan::cmd_end_render_pass");
    resolved<void (VKAPI_PTR *)(VkCommandBuffer)>(Function::cmd_end_render_pass)(command_buffer);
}

void LoaderVulkan::cmd_draw(VkCommandBuffer command_buffer, uint32_t vertices)
{
    AV_SCOPE("LoaderVulkan::cmd_draw");
    resolved<void (VKAPI_PTR *)(VkCommandBuffer, uint32_t, uint32_t, uint32_t, uint32_t)>(Function::cmd_draw)(command_buffer, vertices, 1u, 0u, 0u);
}

void LoaderVulkan::cmd_copy_image_to_buffer(VkCommandBuffer command_buffer, VkImage image, VkImageLayout layout, VkBuffer buffer, const VkBufferImageCopy *region)
{
    AV_SCOPE("LoaderVulkan::cmd_copy_image_to_buffer");
    resolved<void (VKAPI_PTR *)(VkCommandBuffer, VkImage, VkImageLayout, VkBuffer, uint32_t, const VkBufferImageCopy *)>(Function::cmd_copy_image_to_buffer)(command_buffer, image, layout, buffer, 1u, region);
}

VkResult LoaderVulkan::queue_submit(VkQueue queue, const VkSubmitInfo *submit, VkFence fence)
{
    AV_SCOPE("LoaderVulkan::queue_submit");
    return resolved<VkResult (VKAPI_PTR *)(VkQueue, uint32_t, const VkSubmitInfo *, VkFence)>(Function::queue_submit)(queue, 1u, submit, fence);
}

VkResult LoaderVulkan::create_win32_surface(VkInstance instance, void *module, void *window, VkSurfaceKHR *surface)
{
    AV_SCOPE("LoaderVulkan::create_win32_surface");
    icd::Win32SurfaceCreateRecord info = {};
    info.type_ = icd::win32_surface_create_info;
    info.module_ = module;
    info.window_ = window;
    using Create = VkResult (VKAPI_PTR *)(VkInstance, const icd::Win32SurfaceCreateRecord *,
                                          const VkAllocationCallbacks *, VkSurfaceKHR *);
    return resolved<Create>(Function::create_win32_surface)(instance, &info, nullptr, surface);
}

void LoaderVulkan::destroy_surface(VkInstance instance, VkSurfaceKHR surface)
{
    AV_SCOPE("LoaderVulkan::destroy_surface");
    resolved<void (VKAPI_PTR *)(VkInstance, VkSurfaceKHR, const VkAllocationCallbacks *)>(Function::destroy_surface)(
        instance, surface, nullptr);
}

VkResult LoaderVulkan::surface_capabilities(VkPhysicalDevice physical_device, VkSurfaceKHR surface,
                                            VkSurfaceCapabilitiesKHR *capabilities)
{
    AV_SCOPE("LoaderVulkan::surface_capabilities");
    using Capabilities = VkResult (VKAPI_PTR *)(VkPhysicalDevice, VkSurfaceKHR, VkSurfaceCapabilitiesKHR *);
    return resolved<Capabilities>(Function::surface_capabilities)(physical_device, surface, capabilities);
}

VkResult LoaderVulkan::surface_formats(VkPhysicalDevice physical_device, VkSurfaceKHR surface, uint32_t *count,
                                       VkSurfaceFormatKHR *formats)
{
    AV_SCOPE("LoaderVulkan::surface_formats");
    using Formats = VkResult (VKAPI_PTR *)(VkPhysicalDevice, VkSurfaceKHR, uint32_t *, VkSurfaceFormatKHR *);
    return resolved<Formats>(Function::surface_formats)(physical_device, surface, count, formats);
}

VkResult LoaderVulkan::surface_present_modes(VkPhysicalDevice physical_device, VkSurfaceKHR surface, uint32_t *count,
                                             VkPresentModeKHR *modes)
{
    AV_SCOPE("LoaderVulkan::surface_present_modes");
    using Modes = VkResult (VKAPI_PTR *)(VkPhysicalDevice, VkSurfaceKHR, uint32_t *, VkPresentModeKHR *);
    return resolved<Modes>(Function::surface_present_modes)(physical_device, surface, count, modes);
}

VkResult LoaderVulkan::create_swapchain(VkDevice device, const VkSwapchainCreateInfoKHR *info,
                                        VkSwapchainKHR *swapchain)
{
    AV_SCOPE("LoaderVulkan::create_swapchain");
    using Create = VkResult (VKAPI_PTR *)(VkDevice, const VkSwapchainCreateInfoKHR *, const VkAllocationCallbacks *,
                                          VkSwapchainKHR *);
    return resolved<Create>(Function::create_swapchain)(device, info, nullptr, swapchain);
}

void LoaderVulkan::destroy_swapchain(VkDevice device, VkSwapchainKHR swapchain)
{
    AV_SCOPE("LoaderVulkan::destroy_swapchain");
    resolved<void (VKAPI_PTR *)(VkDevice, VkSwapchainKHR, const VkAllocationCallbacks *)>(Function::destroy_swapchain)(
        device, swapchain, nullptr);
}

VkResult LoaderVulkan::swapchain_images(VkDevice device, VkSwapchainKHR swapchain, uint32_t *count, VkImage *images)
{
    AV_SCOPE("LoaderVulkan::swapchain_images");
    using Images = VkResult (VKAPI_PTR *)(VkDevice, VkSwapchainKHR, uint32_t *, VkImage *);
    return resolved<Images>(Function::swapchain_images)(device, swapchain, count, images);
}

VkResult LoaderVulkan::acquire_next_image(VkDevice device, VkSwapchainKHR swapchain, uint64_t timeout, uint32_t *index)
{
    AV_SCOPE("LoaderVulkan::acquire_next_image");
    using Acquire = VkResult (VKAPI_PTR *)(VkDevice, VkSwapchainKHR, uint64_t, VkSemaphore, VkFence, uint32_t *);
    return resolved<Acquire>(Function::acquire_next_image)(device, swapchain, timeout, VK_NULL_HANDLE, VK_NULL_HANDLE,
                                                           index);
}

VkResult LoaderVulkan::queue_present(VkQueue queue, const VkPresentInfoKHR *info)
{
    AV_SCOPE("LoaderVulkan::queue_present");
    return resolved<VkResult (VKAPI_PTR *)(VkQueue, const VkPresentInfoKHR *)>(Function::queue_present)(queue, info);
}

}  // namespace av
