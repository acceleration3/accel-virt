/* The probe's Vulkan through one loader entry point (feature 045, T037): the
 * vkGetInstanceProcAddr it is given - vulkan-1.dll's in the guest, the ICD's
 * own on the full-stack rig - answers vkCreateInstance, and each instance made
 * answers the rest, resolved once as the instance is made. The Vulkan
 * specification has vkGetInstanceProcAddr answer every core command, so those
 * are called as answered; only the ICD's vkRvkForceContextFault, which no
 * loader has to know, is looked for. */
#ifndef AV_LOADER_VULKAN_HPP
#define AV_LOADER_VULKAN_HPP

#include "core/trace/function_scope.hpp"
#include "guest/suite/vulkan_entry.hpp"

#include <stdint.h>

#include <vulkan/vulkan_core.h>

namespace av {

class LoaderVulkan final : public VulkanEntry {
public:
    using GetInstanceProcAddr = PFN_vkVoidFunction (VKAPI_PTR *)(VkInstance instance, const char *name);

    explicit LoaderVulkan(GetInstanceProcAddr get_instance_proc_addr);
    LoaderVulkan(const LoaderVulkan &) = delete;
    LoaderVulkan &operator=(const LoaderVulkan &) = delete;

    VkResult create_instance(const VkInstanceCreateInfo *info, VkInstance *instance) override;
    void     destroy_instance(VkInstance instance) override;
    VkResult enumerate_physical_devices(VkInstance instance, uint32_t *count,
                                        VkPhysicalDevice *physical_devices) override;
    void     physical_device_properties2(VkPhysicalDevice physical_device,
                                         VkPhysicalDeviceProperties2 *properties) override;
    void     queue_family_properties(VkPhysicalDevice physical_device, uint32_t *count,
                                     VkQueueFamilyProperties *properties) override;
    void     memory_properties(VkPhysicalDevice physical_device, VkPhysicalDeviceMemoryProperties *properties) override;
    VkResult create_device(VkPhysicalDevice physical_device, const VkDeviceCreateInfo *info, VkDevice *device) override;
    VkResult force_context_fault(VkInstance instance, VkPhysicalDevice physical_device) override;

    void     destroy_device(VkDevice device) override;
    VkResult device_wait_idle(VkDevice device) override;
    void     device_queue(VkDevice device, uint32_t family, uint32_t index, VkQueue *queue) override;
    VkResult create_buffer(VkDevice device, const VkBufferCreateInfo *info, VkBuffer *buffer) override;
    void     destroy_buffer(VkDevice device, VkBuffer buffer) override;
    void     buffer_memory_requirements(VkDevice device, VkBuffer buffer, VkMemoryRequirements *requirements) override;
    VkResult bind_buffer_memory(VkDevice device, VkBuffer buffer, VkDeviceMemory memory) override;
    VkResult create_image(VkDevice device, const VkImageCreateInfo *info, VkImage *image) override;
    void     destroy_image(VkDevice device, VkImage image) override;
    void     image_memory_requirements(VkDevice device, VkImage image, VkMemoryRequirements *requirements) override;
    VkResult bind_image_memory(VkDevice device, VkImage image, VkDeviceMemory memory) override;
    VkResult allocate_memory(VkDevice device, const VkMemoryAllocateInfo *info, VkDeviceMemory *memory) override;
    void     free_memory(VkDevice device, VkDeviceMemory memory) override;
    VkResult enumerate_device_extensions(VkPhysicalDevice physical_device, uint32_t *count,
                                         VkExtensionProperties *properties) override;
    uint32_t last_kernel_status(VkInstance instance, VkPhysicalDevice physical_device) override;
    VkResult memory_win32_handle(VkDevice device, VkDeviceMemory memory, void **handle) override;
    VkResult map_memory(VkDevice device, VkDeviceMemory memory, void **data) override;
    VkResult create_image_view(VkDevice device, const VkImageViewCreateInfo *info, VkImageView *view) override;
    void     destroy_image_view(VkDevice device, VkImageView view) override;
    VkResult create_shader_module(VkDevice device, const VkShaderModuleCreateInfo *info,
                                  VkShaderModule *module) override;
    void     destroy_shader_module(VkDevice device, VkShaderModule module) override;
    VkResult create_descriptor_set_layout(VkDevice device, const VkDescriptorSetLayoutCreateInfo *info,
                                          VkDescriptorSetLayout *layout) override;
    void     destroy_descriptor_set_layout(VkDevice device, VkDescriptorSetLayout layout) override;
    VkResult create_pipeline_layout(VkDevice device, const VkPipelineLayoutCreateInfo *info,
                                    VkPipelineLayout *layout) override;
    void     destroy_pipeline_layout(VkDevice device, VkPipelineLayout layout) override;
    VkResult create_compute_pipeline(VkDevice device, const VkComputePipelineCreateInfo *info,
                                     VkPipeline *pipeline) override;
    VkResult create_graphics_pipeline(VkDevice device, const VkGraphicsPipelineCreateInfo *info,
                                      VkPipeline *pipeline) override;
    void     destroy_pipeline(VkDevice device, VkPipeline pipeline) override;
    VkResult create_descriptor_pool(VkDevice device, const VkDescriptorPoolCreateInfo *info,
                                    VkDescriptorPool *pool) override;
    void     destroy_descriptor_pool(VkDevice device, VkDescriptorPool pool) override;
    VkResult allocate_descriptor_set(VkDevice device, const VkDescriptorSetAllocateInfo *info,
                                     VkDescriptorSet *set) override;
    void     update_descriptor_set(VkDevice device, const VkWriteDescriptorSet *write) override;
    VkResult create_render_pass(VkDevice device, const VkRenderPassCreateInfo *info,
                                VkRenderPass *render_pass) override;
    void     destroy_render_pass(VkDevice device, VkRenderPass render_pass) override;
    VkResult create_framebuffer(VkDevice device, const VkFramebufferCreateInfo *info,
                                VkFramebuffer *framebuffer) override;
    void     destroy_framebuffer(VkDevice device, VkFramebuffer framebuffer) override;
    VkResult create_command_pool(VkDevice device, const VkCommandPoolCreateInfo *info, VkCommandPool *pool) override;
    void     destroy_command_pool(VkDevice device, VkCommandPool pool) override;
    VkResult allocate_command_buffer(VkDevice device, const VkCommandBufferAllocateInfo *info,
                                     VkCommandBuffer *command_buffer) override;
    VkResult create_fence(VkDevice device, const VkFenceCreateInfo *info, VkFence *fence) override;
    void     destroy_fence(VkDevice device, VkFence fence) override;
    VkResult wait_for_fence(VkDevice device, VkFence fence, uint64_t timeout) override;

    VkResult begin_command_buffer(VkCommandBuffer command_buffer, const VkCommandBufferBeginInfo *info) override;
    VkResult end_command_buffer(VkCommandBuffer command_buffer) override;
    void     cmd_bind_pipeline(VkCommandBuffer command_buffer, VkPipelineBindPoint point, VkPipeline pipeline) override;
    void     cmd_bind_compute_set(VkCommandBuffer command_buffer, VkPipelineLayout layout,
                                  VkDescriptorSet set) override;
    void     cmd_dispatch(VkCommandBuffer command_buffer, uint32_t groups_x) override;
    void     cmd_begin_render_pass(VkCommandBuffer command_buffer, const VkRenderPassBeginInfo *info) override;
    void     cmd_end_render_pass(VkCommandBuffer command_buffer) override;
    void     cmd_draw(VkCommandBuffer command_buffer, uint32_t vertices) override;
    void     cmd_copy_image_to_buffer(VkCommandBuffer command_buffer, VkImage image, VkImageLayout layout,
                                      VkBuffer buffer, const VkBufferImageCopy *region) override;

    VkResult queue_submit(VkQueue queue, const VkSubmitInfo *submit, VkFence fence) override;

    VkResult create_win32_surface(VkInstance instance, void *module, void *window, VkSurfaceKHR *surface) override;
    void     destroy_surface(VkInstance instance, VkSurfaceKHR surface) override;
    VkResult surface_capabilities(VkPhysicalDevice physical_device, VkSurfaceKHR surface,
                                  VkSurfaceCapabilitiesKHR *capabilities) override;
    VkResult surface_formats(VkPhysicalDevice physical_device, VkSurfaceKHR surface, uint32_t *count,
                             VkSurfaceFormatKHR *formats) override;
    VkResult surface_present_modes(VkPhysicalDevice physical_device, VkSurfaceKHR surface, uint32_t *count,
                                   VkPresentModeKHR *modes) override;
    VkResult create_swapchain(VkDevice device, const VkSwapchainCreateInfoKHR *info,
                              VkSwapchainKHR *swapchain) override;
    void     destroy_swapchain(VkDevice device, VkSwapchainKHR swapchain) override;
    VkResult swapchain_images(VkDevice device, VkSwapchainKHR swapchain, uint32_t *count, VkImage *images) override;
    VkResult acquire_next_image(VkDevice device, VkSwapchainKHR swapchain, uint64_t timeout,
                                uint32_t *index) override;
    VkResult queue_present(VkQueue queue, const VkPresentInfoKHR *info) override;

private:
    /* One per function resolved through an instance, in `names` order. */
    enum class Function : uint32_t {
        destroy_instance,
        enumerate_physical_devices,
        physical_device_properties2,
        queue_family_properties,
        memory_properties,
        create_device,
        destroy_device,
        device_wait_idle,
        device_queue,
        create_buffer,
        destroy_buffer,
        buffer_memory_requirements,
        bind_buffer_memory,
        create_image,
        destroy_image,
        image_memory_requirements,
        bind_image_memory,
        allocate_memory,
        free_memory,
        enumerate_device_extensions,
        memory_win32_handle,
        map_memory,
        create_image_view,
        destroy_image_view,
        create_shader_module,
        destroy_shader_module,
        create_descriptor_set_layout,
        destroy_descriptor_set_layout,
        create_pipeline_layout,
        destroy_pipeline_layout,
        create_compute_pipelines,
        create_graphics_pipelines,
        destroy_pipeline,
        create_descriptor_pool,
        destroy_descriptor_pool,
        allocate_descriptor_sets,
        update_descriptor_sets,
        create_render_pass,
        destroy_render_pass,
        create_framebuffer,
        destroy_framebuffer,
        create_command_pool,
        destroy_command_pool,
        allocate_command_buffers,
        create_fence,
        destroy_fence,
        wait_for_fences,
        begin_command_buffer,
        end_command_buffer,
        cmd_bind_pipeline,
        cmd_bind_descriptor_sets,
        cmd_dispatch,
        cmd_begin_render_pass,
        cmd_end_render_pass,
        cmd_draw,
        cmd_copy_image_to_buffer,
        queue_submit,
        create_win32_surface,
        destroy_surface,
        surface_capabilities,
        surface_formats,
        surface_present_modes,
        create_swapchain,
        destroy_swapchain,
        swapchain_images,
        acquire_next_image,
        queue_present,
        count
    };
    static constexpr uint32_t function_count = static_cast<uint32_t>(Function::count);

    /* The function resolved for `function`, as the C type the caller names. */
    template <typename Pointer>
    Pointer resolved(Function function) const;

    GetInstanceProcAddr get_instance_proc_addr_;
    PFN_vkVoidFunction  functions_[function_count];
};

template <typename Pointer>
Pointer LoaderVulkan::resolved(Function function) const
{
    AV_SCOPE("LoaderVulkan::resolved");
    return reinterpret_cast<Pointer>(functions_[static_cast<uint32_t>(function)]);
}

}  // namespace av

#endif  // AV_LOADER_VULKAN_HPP
