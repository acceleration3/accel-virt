#ifndef AV_VULKAN_ENTRY_HPP
#define AV_VULKAN_ENTRY_HPP

#include <stdint.h>

#include <vulkan/vulkan_core.h>

namespace av {

class VulkanEntry {
public:
    VulkanEntry(const VulkanEntry &) = delete;
    VulkanEntry &operator=(const VulkanEntry &) = delete;

    virtual VkResult create_instance(const VkInstanceCreateInfo *info, VkInstance *instance) = 0;
    virtual void     destroy_instance(VkInstance instance) = 0;
    virtual VkResult enumerate_physical_devices(VkInstance instance, uint32_t *count,
                                                VkPhysicalDevice *physical_devices) = 0;
    virtual void     physical_device_properties2(VkPhysicalDevice physical_device,
                                                 VkPhysicalDeviceProperties2 *properties) = 0;
    virtual void     queue_family_properties(VkPhysicalDevice physical_device, uint32_t *count,
                                             VkQueueFamilyProperties *properties) = 0;
    virtual void     memory_properties(VkPhysicalDevice physical_device,
                                       VkPhysicalDeviceMemoryProperties *properties) = 0;
    virtual VkResult create_device(VkPhysicalDevice physical_device, const VkDeviceCreateInfo *info,
                                   VkDevice *device) = 0;
    virtual VkResult force_context_fault(VkDevice device) = 0;
    virtual uint32_t last_kernel_status(VkDevice device) = 0;

    virtual void     destroy_device(VkDevice device) = 0;
    virtual VkResult device_wait_idle(VkDevice device) = 0;
    virtual void     device_queue(VkDevice device, uint32_t family, uint32_t index, VkQueue *queue) = 0;
    virtual VkResult create_buffer(VkDevice device, const VkBufferCreateInfo *info, VkBuffer *buffer) = 0;
    virtual void     destroy_buffer(VkDevice device, VkBuffer buffer) = 0;
    virtual void     buffer_memory_requirements(VkDevice device, VkBuffer buffer,
                                                VkMemoryRequirements *requirements) = 0;
    virtual VkResult bind_buffer_memory(VkDevice device, VkBuffer buffer, VkDeviceMemory memory) = 0;
    virtual VkResult create_image(VkDevice device, const VkImageCreateInfo *info, VkImage *image) = 0;
    virtual void     destroy_image(VkDevice device, VkImage image) = 0;
    virtual void     image_memory_requirements(VkDevice device, VkImage image,
                                               VkMemoryRequirements *requirements) = 0;
    virtual VkResult bind_image_memory(VkDevice device, VkImage image, VkDeviceMemory memory) = 0;
    virtual VkResult allocate_memory(VkDevice device, const VkMemoryAllocateInfo *info,
                                     VkDeviceMemory *memory) = 0;
    virtual void     free_memory(VkDevice device, VkDeviceMemory memory) = 0;
    virtual VkResult enumerate_device_extensions(VkPhysicalDevice physical_device, uint32_t *count,
                                                 VkExtensionProperties *properties) = 0;
    virtual VkResult memory_win32_handle(VkDevice device, VkDeviceMemory memory, void **handle) = 0;
    virtual VkResult map_memory(VkDevice device, VkDeviceMemory memory, void **data) = 0;
    virtual VkResult create_image_view(VkDevice device, const VkImageViewCreateInfo *info,
                                       VkImageView *view) = 0;
    virtual void     destroy_image_view(VkDevice device, VkImageView view) = 0;
    virtual VkResult create_shader_module(VkDevice device, const VkShaderModuleCreateInfo *info,
                                          VkShaderModule *module) = 0;
    virtual void     destroy_shader_module(VkDevice device, VkShaderModule module) = 0;
    virtual VkResult create_descriptor_set_layout(VkDevice device, const VkDescriptorSetLayoutCreateInfo *info,
                                                  VkDescriptorSetLayout *layout) = 0;
    virtual void     destroy_descriptor_set_layout(VkDevice device, VkDescriptorSetLayout layout) = 0;
    virtual VkResult create_pipeline_layout(VkDevice device, const VkPipelineLayoutCreateInfo *info,
                                            VkPipelineLayout *layout) = 0;
    virtual void     destroy_pipeline_layout(VkDevice device, VkPipelineLayout layout) = 0;
    virtual VkResult create_compute_pipeline(VkDevice device, const VkComputePipelineCreateInfo *info,
                                             VkPipeline *pipeline) = 0;
    virtual VkResult create_graphics_pipeline(VkDevice device, const VkGraphicsPipelineCreateInfo *info,
                                              VkPipeline *pipeline) = 0;
    virtual void     destroy_pipeline(VkDevice device, VkPipeline pipeline) = 0;
    virtual VkResult create_descriptor_pool(VkDevice device, const VkDescriptorPoolCreateInfo *info,
                                            VkDescriptorPool *pool) = 0;
    virtual void     destroy_descriptor_pool(VkDevice device, VkDescriptorPool pool) = 0;
    virtual VkResult allocate_descriptor_set(VkDevice device, const VkDescriptorSetAllocateInfo *info,
                                             VkDescriptorSet *set) = 0;
    virtual void     update_descriptor_set(VkDevice device, const VkWriteDescriptorSet *write) = 0;
    virtual VkResult create_render_pass(VkDevice device, const VkRenderPassCreateInfo *info,
                                        VkRenderPass *render_pass) = 0;
    virtual void     destroy_render_pass(VkDevice device, VkRenderPass render_pass) = 0;
    virtual VkResult create_framebuffer(VkDevice device, const VkFramebufferCreateInfo *info,
                                        VkFramebuffer *framebuffer) = 0;
    virtual void     destroy_framebuffer(VkDevice device, VkFramebuffer framebuffer) = 0;
    virtual VkResult create_command_pool(VkDevice device, const VkCommandPoolCreateInfo *info,
                                         VkCommandPool *pool) = 0;
    virtual void     destroy_command_pool(VkDevice device, VkCommandPool pool) = 0;
    virtual VkResult allocate_command_buffer(VkDevice device, const VkCommandBufferAllocateInfo *info,
                                             VkCommandBuffer *command_buffer) = 0;
    virtual VkResult create_fence(VkDevice device, const VkFenceCreateInfo *info, VkFence *fence) = 0;
    virtual void     destroy_fence(VkDevice device, VkFence fence) = 0;
    virtual VkResult wait_for_fence(VkDevice device, VkFence fence, uint64_t timeout) = 0;

    virtual VkResult begin_command_buffer(VkCommandBuffer command_buffer, const VkCommandBufferBeginInfo *info) = 0;
    virtual VkResult end_command_buffer(VkCommandBuffer command_buffer) = 0;
    virtual void     cmd_bind_pipeline(VkCommandBuffer command_buffer, VkPipelineBindPoint point,
                                       VkPipeline pipeline) = 0;
    virtual void     cmd_bind_compute_set(VkCommandBuffer command_buffer, VkPipelineLayout layout,
                                          VkDescriptorSet set) = 0;
    virtual void     cmd_dispatch(VkCommandBuffer command_buffer, uint32_t groups_x) = 0;
    virtual void     cmd_begin_render_pass(VkCommandBuffer command_buffer, const VkRenderPassBeginInfo *info) = 0;
    virtual void     cmd_end_render_pass(VkCommandBuffer command_buffer) = 0;
    virtual void     cmd_draw(VkCommandBuffer command_buffer, uint32_t vertices) = 0;
    virtual void     cmd_copy_image_to_buffer(VkCommandBuffer command_buffer, VkImage image, VkImageLayout layout,
                                              VkBuffer buffer, const VkBufferImageCopy *region) = 0;
    virtual void     cmd_fill_buffer(VkCommandBuffer command_buffer, VkBuffer buffer, VkDeviceSize size,
                                     uint32_t value) = 0;

    virtual VkResult queue_submit(VkQueue queue, const VkSubmitInfo *submit, VkFence fence) = 0;

    virtual VkResult create_win32_surface(VkInstance instance, void *module, void *window,
                                          VkSurfaceKHR *surface) = 0;
    virtual void     destroy_surface(VkInstance instance, VkSurfaceKHR surface) = 0;
    virtual VkResult surface_capabilities(VkPhysicalDevice physical_device, VkSurfaceKHR surface,
                                          VkSurfaceCapabilitiesKHR *capabilities) = 0;
    virtual VkResult surface_formats(VkPhysicalDevice physical_device, VkSurfaceKHR surface, uint32_t *count,
                                     VkSurfaceFormatKHR *formats) = 0;
    virtual VkResult surface_present_modes(VkPhysicalDevice physical_device, VkSurfaceKHR surface, uint32_t *count,
                                           VkPresentModeKHR *modes) = 0;
    virtual VkResult create_swapchain(VkDevice device, const VkSwapchainCreateInfoKHR *info,
                                      VkSwapchainKHR *swapchain) = 0;
    virtual void     destroy_swapchain(VkDevice device, VkSwapchainKHR swapchain) = 0;
    virtual VkResult swapchain_images(VkDevice device, VkSwapchainKHR swapchain, uint32_t *count,
                                      VkImage *images) = 0;
    virtual VkResult acquire_next_image(VkDevice device, VkSwapchainKHR swapchain, uint64_t timeout,
                                        uint32_t *index) = 0;
    virtual VkResult queue_present(VkQueue queue, const VkPresentInfoKHR *info) = 0;


protected:
    VulkanEntry() = default;
    ~VulkanEntry() = default;
};

}

#endif
