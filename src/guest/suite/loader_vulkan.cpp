#include "guest/suite/loader_vulkan.hpp"

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
};

constexpr const char force_context_fault_name[] = "vkRvkForceContextFault";

}  // namespace

LoaderVulkan::LoaderVulkan(GetInstanceProcAddr get_instance_proc_addr)
    : get_instance_proc_addr_(get_instance_proc_addr), functions_{}
{
    static_assert(sizeof names / sizeof names[0] == function_count);
}

VkResult LoaderVulkan::create_instance(const VkInstanceCreateInfo *info, VkInstance *instance)
{
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
    using Force = VkResult (VKAPI_PTR *)(VkPhysicalDevice);
    const auto force = reinterpret_cast<Force>(get_instance_proc_addr_(instance, force_context_fault_name));
    return force != nullptr ? force(physical_device) : VK_ERROR_EXTENSION_NOT_PRESENT;
}

void LoaderVulkan::destroy_instance(VkInstance instance)
{
    resolved<void (VKAPI_PTR *)(VkInstance, const VkAllocationCallbacks *)>(Function::destroy_instance)(instance, nullptr);
}

VkResult LoaderVulkan::enumerate_physical_devices(VkInstance instance, uint32_t *count, VkPhysicalDevice *physical_devices)
{
    return resolved<VkResult (VKAPI_PTR *)(VkInstance, uint32_t *, VkPhysicalDevice *)>(Function::enumerate_physical_devices)(instance, count, physical_devices);
}

void LoaderVulkan::physical_device_properties2(VkPhysicalDevice physical_device, VkPhysicalDeviceProperties2 *properties)
{
    resolved<void (VKAPI_PTR *)(VkPhysicalDevice, VkPhysicalDeviceProperties2 *)>(Function::physical_device_properties2)(physical_device, properties);
}

void LoaderVulkan::queue_family_properties(VkPhysicalDevice physical_device, uint32_t *count, VkQueueFamilyProperties *properties)
{
    resolved<void (VKAPI_PTR *)(VkPhysicalDevice, uint32_t *, VkQueueFamilyProperties *)>(Function::queue_family_properties)(physical_device, count, properties);
}

void LoaderVulkan::memory_properties(VkPhysicalDevice physical_device, VkPhysicalDeviceMemoryProperties *properties)
{
    resolved<void (VKAPI_PTR *)(VkPhysicalDevice, VkPhysicalDeviceMemoryProperties *)>(Function::memory_properties)(physical_device, properties);
}

VkResult LoaderVulkan::create_device(VkPhysicalDevice physical_device, const VkDeviceCreateInfo *info, VkDevice *device)
{
    return resolved<VkResult (VKAPI_PTR *)(VkPhysicalDevice, const VkDeviceCreateInfo *, const VkAllocationCallbacks *, VkDevice *)>(Function::create_device)(physical_device, info, nullptr, device);
}

void LoaderVulkan::destroy_device(VkDevice device)
{
    resolved<void (VKAPI_PTR *)(VkDevice, const VkAllocationCallbacks *)>(Function::destroy_device)(device, nullptr);
}

VkResult LoaderVulkan::device_wait_idle(VkDevice device)
{
    return resolved<VkResult (VKAPI_PTR *)(VkDevice)>(Function::device_wait_idle)(device);
}

void LoaderVulkan::device_queue(VkDevice device, uint32_t family, uint32_t index, VkQueue *queue)
{
    resolved<void (VKAPI_PTR *)(VkDevice, uint32_t, uint32_t, VkQueue *)>(Function::device_queue)(device, family, index, queue);
}

VkResult LoaderVulkan::create_buffer(VkDevice device, const VkBufferCreateInfo *info, VkBuffer *buffer)
{
    return resolved<VkResult (VKAPI_PTR *)(VkDevice, const VkBufferCreateInfo *, const VkAllocationCallbacks *, VkBuffer *)>(Function::create_buffer)(device, info, nullptr, buffer);
}

void LoaderVulkan::destroy_buffer(VkDevice device, VkBuffer buffer)
{
    resolved<void (VKAPI_PTR *)(VkDevice, VkBuffer, const VkAllocationCallbacks *)>(Function::destroy_buffer)(device, buffer, nullptr);
}

void LoaderVulkan::buffer_memory_requirements(VkDevice device, VkBuffer buffer, VkMemoryRequirements *requirements)
{
    resolved<void (VKAPI_PTR *)(VkDevice, VkBuffer, VkMemoryRequirements *)>(Function::buffer_memory_requirements)(device, buffer, requirements);
}

VkResult LoaderVulkan::bind_buffer_memory(VkDevice device, VkBuffer buffer, VkDeviceMemory memory)
{
    return resolved<VkResult (VKAPI_PTR *)(VkDevice, VkBuffer, VkDeviceMemory, VkDeviceSize)>(Function::bind_buffer_memory)(device, buffer, memory, 0u);
}

VkResult LoaderVulkan::create_image(VkDevice device, const VkImageCreateInfo *info, VkImage *image)
{
    return resolved<VkResult (VKAPI_PTR *)(VkDevice, const VkImageCreateInfo *, const VkAllocationCallbacks *, VkImage *)>(Function::create_image)(device, info, nullptr, image);
}

void LoaderVulkan::destroy_image(VkDevice device, VkImage image)
{
    resolved<void (VKAPI_PTR *)(VkDevice, VkImage, const VkAllocationCallbacks *)>(Function::destroy_image)(device, image, nullptr);
}

void LoaderVulkan::image_memory_requirements(VkDevice device, VkImage image, VkMemoryRequirements *requirements)
{
    resolved<void (VKAPI_PTR *)(VkDevice, VkImage, VkMemoryRequirements *)>(Function::image_memory_requirements)(device, image, requirements);
}

VkResult LoaderVulkan::bind_image_memory(VkDevice device, VkImage image, VkDeviceMemory memory)
{
    return resolved<VkResult (VKAPI_PTR *)(VkDevice, VkImage, VkDeviceMemory, VkDeviceSize)>(Function::bind_image_memory)(device, image, memory, 0u);
}

VkResult LoaderVulkan::allocate_memory(VkDevice device, const VkMemoryAllocateInfo *info, VkDeviceMemory *memory)
{
    return resolved<VkResult (VKAPI_PTR *)(VkDevice, const VkMemoryAllocateInfo *, const VkAllocationCallbacks *, VkDeviceMemory *)>(Function::allocate_memory)(device, info, nullptr, memory);
}

void LoaderVulkan::free_memory(VkDevice device, VkDeviceMemory memory)
{
    resolved<void (VKAPI_PTR *)(VkDevice, VkDeviceMemory, const VkAllocationCallbacks *)>(Function::free_memory)(device, memory, nullptr);
}

VkResult LoaderVulkan::map_memory(VkDevice device, VkDeviceMemory memory, void **data)
{
    return resolved<VkResult (VKAPI_PTR *)(VkDevice, VkDeviceMemory, VkDeviceSize, VkDeviceSize, VkMemoryMapFlags, void **)>(Function::map_memory)(device, memory, 0u, VK_WHOLE_SIZE, 0u, data);
}

VkResult LoaderVulkan::create_image_view(VkDevice device, const VkImageViewCreateInfo *info, VkImageView *view)
{
    return resolved<VkResult (VKAPI_PTR *)(VkDevice, const VkImageViewCreateInfo *, const VkAllocationCallbacks *, VkImageView *)>(Function::create_image_view)(device, info, nullptr, view);
}

void LoaderVulkan::destroy_image_view(VkDevice device, VkImageView view)
{
    resolved<void (VKAPI_PTR *)(VkDevice, VkImageView, const VkAllocationCallbacks *)>(Function::destroy_image_view)(device, view, nullptr);
}

VkResult LoaderVulkan::create_shader_module(VkDevice device, const VkShaderModuleCreateInfo *info, VkShaderModule *module)
{
    return resolved<VkResult (VKAPI_PTR *)(VkDevice, const VkShaderModuleCreateInfo *, const VkAllocationCallbacks *, VkShaderModule *)>(Function::create_shader_module)(device, info, nullptr, module);
}

void LoaderVulkan::destroy_shader_module(VkDevice device, VkShaderModule module)
{
    resolved<void (VKAPI_PTR *)(VkDevice, VkShaderModule, const VkAllocationCallbacks *)>(Function::destroy_shader_module)(device, module, nullptr);
}

VkResult LoaderVulkan::create_descriptor_set_layout(VkDevice device, const VkDescriptorSetLayoutCreateInfo *info, VkDescriptorSetLayout *layout)
{
    return resolved<VkResult (VKAPI_PTR *)(VkDevice, const VkDescriptorSetLayoutCreateInfo *, const VkAllocationCallbacks *, VkDescriptorSetLayout *)>(Function::create_descriptor_set_layout)(device, info, nullptr, layout);
}

void LoaderVulkan::destroy_descriptor_set_layout(VkDevice device, VkDescriptorSetLayout layout)
{
    resolved<void (VKAPI_PTR *)(VkDevice, VkDescriptorSetLayout, const VkAllocationCallbacks *)>(Function::destroy_descriptor_set_layout)(device, layout, nullptr);
}

VkResult LoaderVulkan::create_pipeline_layout(VkDevice device, const VkPipelineLayoutCreateInfo *info, VkPipelineLayout *layout)
{
    return resolved<VkResult (VKAPI_PTR *)(VkDevice, const VkPipelineLayoutCreateInfo *, const VkAllocationCallbacks *, VkPipelineLayout *)>(Function::create_pipeline_layout)(device, info, nullptr, layout);
}

void LoaderVulkan::destroy_pipeline_layout(VkDevice device, VkPipelineLayout layout)
{
    resolved<void (VKAPI_PTR *)(VkDevice, VkPipelineLayout, const VkAllocationCallbacks *)>(Function::destroy_pipeline_layout)(device, layout, nullptr);
}

VkResult LoaderVulkan::create_compute_pipeline(VkDevice device, const VkComputePipelineCreateInfo *info, VkPipeline *pipeline)
{
    return resolved<VkResult (VKAPI_PTR *)(VkDevice, VkPipelineCache, uint32_t, const VkComputePipelineCreateInfo *, const VkAllocationCallbacks *, VkPipeline *)>(Function::create_compute_pipelines)(device, VK_NULL_HANDLE, 1u, info, nullptr, pipeline);
}

VkResult LoaderVulkan::create_graphics_pipeline(VkDevice device, const VkGraphicsPipelineCreateInfo *info, VkPipeline *pipeline)
{
    return resolved<VkResult (VKAPI_PTR *)(VkDevice, VkPipelineCache, uint32_t, const VkGraphicsPipelineCreateInfo *, const VkAllocationCallbacks *, VkPipeline *)>(Function::create_graphics_pipelines)(device, VK_NULL_HANDLE, 1u, info, nullptr, pipeline);
}

void LoaderVulkan::destroy_pipeline(VkDevice device, VkPipeline pipeline)
{
    resolved<void (VKAPI_PTR *)(VkDevice, VkPipeline, const VkAllocationCallbacks *)>(Function::destroy_pipeline)(device, pipeline, nullptr);
}

VkResult LoaderVulkan::create_descriptor_pool(VkDevice device, const VkDescriptorPoolCreateInfo *info, VkDescriptorPool *pool)
{
    return resolved<VkResult (VKAPI_PTR *)(VkDevice, const VkDescriptorPoolCreateInfo *, const VkAllocationCallbacks *, VkDescriptorPool *)>(Function::create_descriptor_pool)(device, info, nullptr, pool);
}

void LoaderVulkan::destroy_descriptor_pool(VkDevice device, VkDescriptorPool pool)
{
    resolved<void (VKAPI_PTR *)(VkDevice, VkDescriptorPool, const VkAllocationCallbacks *)>(Function::destroy_descriptor_pool)(device, pool, nullptr);
}

VkResult LoaderVulkan::allocate_descriptor_set(VkDevice device, const VkDescriptorSetAllocateInfo *info, VkDescriptorSet *set)
{
    return resolved<VkResult (VKAPI_PTR *)(VkDevice, const VkDescriptorSetAllocateInfo *, VkDescriptorSet *)>(Function::allocate_descriptor_sets)(device, info, set);
}

void LoaderVulkan::update_descriptor_set(VkDevice device, const VkWriteDescriptorSet *write)
{
    resolved<void (VKAPI_PTR *)(VkDevice, uint32_t, const VkWriteDescriptorSet *, uint32_t, const VkCopyDescriptorSet *)>(Function::update_descriptor_sets)(device, 1u, write, 0u, nullptr);
}

VkResult LoaderVulkan::create_render_pass(VkDevice device, const VkRenderPassCreateInfo *info, VkRenderPass *render_pass)
{
    return resolved<VkResult (VKAPI_PTR *)(VkDevice, const VkRenderPassCreateInfo *, const VkAllocationCallbacks *, VkRenderPass *)>(Function::create_render_pass)(device, info, nullptr, render_pass);
}

void LoaderVulkan::destroy_render_pass(VkDevice device, VkRenderPass render_pass)
{
    resolved<void (VKAPI_PTR *)(VkDevice, VkRenderPass, const VkAllocationCallbacks *)>(Function::destroy_render_pass)(device, render_pass, nullptr);
}

VkResult LoaderVulkan::create_framebuffer(VkDevice device, const VkFramebufferCreateInfo *info, VkFramebuffer *framebuffer)
{
    return resolved<VkResult (VKAPI_PTR *)(VkDevice, const VkFramebufferCreateInfo *, const VkAllocationCallbacks *, VkFramebuffer *)>(Function::create_framebuffer)(device, info, nullptr, framebuffer);
}

void LoaderVulkan::destroy_framebuffer(VkDevice device, VkFramebuffer framebuffer)
{
    resolved<void (VKAPI_PTR *)(VkDevice, VkFramebuffer, const VkAllocationCallbacks *)>(Function::destroy_framebuffer)(device, framebuffer, nullptr);
}

VkResult LoaderVulkan::create_command_pool(VkDevice device, const VkCommandPoolCreateInfo *info, VkCommandPool *pool)
{
    return resolved<VkResult (VKAPI_PTR *)(VkDevice, const VkCommandPoolCreateInfo *, const VkAllocationCallbacks *, VkCommandPool *)>(Function::create_command_pool)(device, info, nullptr, pool);
}

void LoaderVulkan::destroy_command_pool(VkDevice device, VkCommandPool pool)
{
    resolved<void (VKAPI_PTR *)(VkDevice, VkCommandPool, const VkAllocationCallbacks *)>(Function::destroy_command_pool)(device, pool, nullptr);
}

VkResult LoaderVulkan::allocate_command_buffer(VkDevice device, const VkCommandBufferAllocateInfo *info, VkCommandBuffer *command_buffer)
{
    return resolved<VkResult (VKAPI_PTR *)(VkDevice, const VkCommandBufferAllocateInfo *, VkCommandBuffer *)>(Function::allocate_command_buffers)(device, info, command_buffer);
}

VkResult LoaderVulkan::create_fence(VkDevice device, const VkFenceCreateInfo *info, VkFence *fence)
{
    return resolved<VkResult (VKAPI_PTR *)(VkDevice, const VkFenceCreateInfo *, const VkAllocationCallbacks *, VkFence *)>(Function::create_fence)(device, info, nullptr, fence);
}

void LoaderVulkan::destroy_fence(VkDevice device, VkFence fence)
{
    resolved<void (VKAPI_PTR *)(VkDevice, VkFence, const VkAllocationCallbacks *)>(Function::destroy_fence)(device, fence, nullptr);
}

VkResult LoaderVulkan::wait_for_fence(VkDevice device, VkFence fence, uint64_t timeout)
{
    return resolved<VkResult (VKAPI_PTR *)(VkDevice, uint32_t, const VkFence *, VkBool32, uint64_t)>(Function::wait_for_fences)(device, 1u, &fence, VK_TRUE, timeout);
}

VkResult LoaderVulkan::begin_command_buffer(VkCommandBuffer command_buffer, const VkCommandBufferBeginInfo *info)
{
    return resolved<VkResult (VKAPI_PTR *)(VkCommandBuffer, const VkCommandBufferBeginInfo *)>(Function::begin_command_buffer)(command_buffer, info);
}

VkResult LoaderVulkan::end_command_buffer(VkCommandBuffer command_buffer)
{
    return resolved<VkResult (VKAPI_PTR *)(VkCommandBuffer)>(Function::end_command_buffer)(command_buffer);
}

void LoaderVulkan::cmd_bind_pipeline(VkCommandBuffer command_buffer, VkPipelineBindPoint point, VkPipeline pipeline)
{
    resolved<void (VKAPI_PTR *)(VkCommandBuffer, VkPipelineBindPoint, VkPipeline)>(Function::cmd_bind_pipeline)(command_buffer, point, pipeline);
}

void LoaderVulkan::cmd_bind_compute_set(VkCommandBuffer command_buffer, VkPipelineLayout layout, VkDescriptorSet set)
{
    resolved<void (VKAPI_PTR *)(VkCommandBuffer, VkPipelineBindPoint, VkPipelineLayout, uint32_t, uint32_t, const VkDescriptorSet *, uint32_t, const uint32_t *)>(Function::cmd_bind_descriptor_sets)(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, layout, 0u, 1u, &set, 0u, nullptr);
}

void LoaderVulkan::cmd_dispatch(VkCommandBuffer command_buffer, uint32_t groups_x)
{
    resolved<void (VKAPI_PTR *)(VkCommandBuffer, uint32_t, uint32_t, uint32_t)>(Function::cmd_dispatch)(command_buffer, groups_x, 1u, 1u);
}

void LoaderVulkan::cmd_begin_render_pass(VkCommandBuffer command_buffer, const VkRenderPassBeginInfo *info)
{
    resolved<void (VKAPI_PTR *)(VkCommandBuffer, const VkRenderPassBeginInfo *, VkSubpassContents)>(Function::cmd_begin_render_pass)(command_buffer, info, VK_SUBPASS_CONTENTS_INLINE);
}

void LoaderVulkan::cmd_end_render_pass(VkCommandBuffer command_buffer)
{
    resolved<void (VKAPI_PTR *)(VkCommandBuffer)>(Function::cmd_end_render_pass)(command_buffer);
}

void LoaderVulkan::cmd_draw(VkCommandBuffer command_buffer, uint32_t vertices)
{
    resolved<void (VKAPI_PTR *)(VkCommandBuffer, uint32_t, uint32_t, uint32_t, uint32_t)>(Function::cmd_draw)(command_buffer, vertices, 1u, 0u, 0u);
}

void LoaderVulkan::cmd_copy_image_to_buffer(VkCommandBuffer command_buffer, VkImage image, VkImageLayout layout, VkBuffer buffer, const VkBufferImageCopy *region)
{
    resolved<void (VKAPI_PTR *)(VkCommandBuffer, VkImage, VkImageLayout, VkBuffer, uint32_t, const VkBufferImageCopy *)>(Function::cmd_copy_image_to_buffer)(command_buffer, image, layout, buffer, 1u, region);
}

VkResult LoaderVulkan::queue_submit(VkQueue queue, const VkSubmitInfo *submit, VkFence fence)
{
    return resolved<VkResult (VKAPI_PTR *)(VkQueue, uint32_t, const VkSubmitInfo *, VkFence)>(Function::queue_submit)(queue, 1u, submit, fence);
}

}  // namespace av
