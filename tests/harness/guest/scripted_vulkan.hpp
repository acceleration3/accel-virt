/* The Vulkan the probe's unit tests play (feature 045, T037): a GPU small
 * enough to judge, behind VulkanEntry. Physical devices carry the LUIDs a test
 * gives them; memory a type marks host visible is this object's own bytes,
 * mapped as they are; a submitted command buffer that dispatched over a bound
 * storage buffer doubles the buffer's elements, and one that drew inside a
 * render pass and copied the image out fills the copy's buffer with the
 * fragment shader's colour. Any VkResult call can be made to answer an error
 * on its Nth use, and every object made is tracked, so a test asserts nothing
 * is left and nothing was destroyed twice. */
#ifndef AV_SCRIPTED_VULKAN_HPP
#define AV_SCRIPTED_VULKAN_HPP

#include "guest/suite/vulkan_entry.hpp"

#include <cstdint>

namespace av {

class Kmt;
class ScriptedWindowFrame;

class ScriptedVulkan final : public VulkanEntry {
public:
    /* The calls a test can fail, and count. */
    enum class Call : std::uint32_t {
        create_instance,
        enumerate_physical_devices,
        create_device,
        force_context_fault,
        device_wait_idle,
        create_buffer,
        bind_buffer_memory,
        create_image,
        bind_image_memory,
        allocate_memory,
        map_memory,
        create_image_view,
        create_shader_module,
        create_descriptor_set_layout,
        create_pipeline_layout,
        create_compute_pipeline,
        create_graphics_pipeline,
        create_descriptor_pool,
        allocate_descriptor_set,
        create_render_pass,
        create_framebuffer,
        create_command_pool,
        allocate_command_buffer,
        create_fence,
        wait_for_fence,
        begin_command_buffer,
        end_command_buffer,
        queue_submit,
        create_win32_surface,
        create_swapchain,
        acquire_next_image,
        queue_present,
        count
    };

    /* More than ProbeDevice looks at. */
    static constexpr std::uint32_t max_physical_devices = 20u;
    static constexpr std::uint32_t max_objects = 64u;
    static constexpr std::uint32_t max_allocations = 4u;
    static constexpr std::uint64_t max_mapped_bytes = 64u * 64u * 4u;
    /* solid.frag's vec4(0.25, 0.5, 0.75, 1.0) as UNORM bytes. */
    static constexpr std::uint8_t  colour[4] = {64u, 128u, 191u, 255u};
    /* Two types: 0 device local, 1 host visible; the buffers may use both. */
    static constexpr std::uint32_t default_memory_types = 2u;
    /* A swapchain's images, as the probe's present asks for. */
    static constexpr std::uint32_t images_per_swapchain = 2u;

    ScriptedVulkan();
    ScriptedVulkan(const ScriptedVulkan &) = delete;
    ScriptedVulkan &operator=(const ScriptedVulkan &) = delete;

    /* The Nth use (1-based, counted from the first) of `call` answers `result`. */
    void          fail(Call call, std::uint32_t nth, VkResult result);
    std::uint32_t calls(Call call) const;
    /* `count` physical devices, the one at `index` given `luid` and whether
     * the LUID is valid; the others' LUIDs are their index plus one. */
    void          set_physical_devices(std::uint32_t count);
    void          set_luid(std::uint32_t index, std::uint64_t luid, bool valid);
    /* `count` queue families, all with `flags`. */
    void          set_queue_families(std::uint32_t count, VkQueueFlags flags);
    /* The type at `index` has `flags`; `count` types in all. */
    void          set_memory_types(std::uint32_t count);
    void          set_memory_type(std::uint32_t index, VkMemoryPropertyFlags flags);
    /* The types buffers and images may use. */
    void          set_allowed_types(std::uint32_t buffers, std::uint32_t images);
    /* The dispatch leaves the last element as it was; the draw leaves the last
     * pixel black. */
    void          miscompute();
    /* Submissions complete nothing: their fences are never signalled. */
    void          hold_fences();
    void          misdraw();
    /* What vkAvForceContextFault answers, and what the device answers to
     * vkDeviceWaitIdle after it. */
    void          answer_fault(VkResult fault, VkResult idle_after);
    /* The window a present draws on, as the driver's blit would: every pixel
     * of it becomes the fragment shader's colour, in the window's own channel
     * order, once a frame has been drawn into the image being presented. */
    void          present_into(ScriptedWindowFrame &window);
    /* Each present waits for a vertical blank on `adapter`, as the driver's
     * FIFO pacing does; without this a present is unpaced. */
    void          pace_on(Kmt &kmt, std::uint32_t adapter, std::uint32_t device);
    void          present_unpaced();
    /* The surface's current extent is not the window's: what an application
     * meets between a resize and the message that follows it. */
    void          hold_the_surface_extent(std::uint32_t width, std::uint32_t height);
    /* A surface offering neither the format nor the mode this driver presents
     * in, and one whose queries answer that it is lost. */
    void          offer_nothing_presentable();
    void          lose_the_surface_queries();
    /* The least images the surface will make a swapchain of: a driver asking
     * for more than the caller keeps room for. */
    void          ask_for_images(std::uint32_t minimum);
    /* Every mode the surface offers is this one, listed twice: a driver that
     * presents one way only. */
    void          offer_only_the_mode(VkPresentModeKHR mode);
    /* The present modes alone are lost, after the formats were answered. */
    void          lose_the_present_modes();
    /* A swapchain whose images cannot be listed, and one that hands out one
     * image where the caller asked for two. */
    void          lose_the_swapchain_images();
    void          hand_out_one_image();
    std::uint32_t presents() const;
    /* Whether the last instance or device was made with `name` enabled: what a
     * real loader needs before the extension's entry points dispatch. */
    bool          instance_enabled(const char *name) const;
    bool          device_enabled(const char *name) const;

    /* After the fault every LUID is xored with `luid_xor` and every family has
     * `flags`: what a host that changed under the probe looks like. */
    void          change_after_fault(std::uint64_t luid_xor, VkQueueFlags flags);

    /* Objects made and not destroyed, and destroys of objects never made or
     * already destroyed. */
    std::uint32_t live() const;
    std::uint32_t misuses() const;
    /* The last instance's API version, the memory type of each allocation in
     * order, the last dispatch's group count and draw's vertex count, and the
     * words of every shader module made, summed. */
    std::uint32_t instance_api_version() const;
    std::uint32_t allocated_type(std::uint32_t index) const;
    std::uint32_t dispatched_groups() const;
    std::uint32_t drawn_vertices() const;
    std::uint64_t shader_words() const;
    /* The physical device the last device was made on, as its index. */
    std::uint32_t device_parent() const;

    VkResult create_instance(const VkInstanceCreateInfo *info, VkInstance *instance) override;
    void     destroy_instance(VkInstance instance) override;
    VkResult enumerate_physical_devices(VkInstance instance, std::uint32_t *count,
                                        VkPhysicalDevice *physical_devices) override;
    void     physical_device_properties2(VkPhysicalDevice physical_device,
                                         VkPhysicalDeviceProperties2 *properties) override;
    void     queue_family_properties(VkPhysicalDevice physical_device, std::uint32_t *count,
                                     VkQueueFamilyProperties *properties) override;
    void     memory_properties(VkPhysicalDevice physical_device, VkPhysicalDeviceMemoryProperties *properties) override;
    VkResult create_device(VkPhysicalDevice physical_device, const VkDeviceCreateInfo *info, VkDevice *device) override;
    VkResult force_context_fault(VkInstance instance, VkPhysicalDevice physical_device) override;

    void     destroy_device(VkDevice device) override;
    VkResult device_wait_idle(VkDevice device) override;
    void     device_queue(VkDevice device, std::uint32_t family, std::uint32_t index, VkQueue *queue) override;
    VkResult create_buffer(VkDevice device, const VkBufferCreateInfo *info, VkBuffer *buffer) override;
    void     destroy_buffer(VkDevice device, VkBuffer buffer) override;
    void     buffer_memory_requirements(VkDevice device, VkBuffer buffer, VkMemoryRequirements *requirements) override;
    VkResult bind_buffer_memory(VkDevice device, VkBuffer buffer, VkDeviceMemory memory) override;
    VkResult create_image(VkDevice device, const VkImageCreateInfo *info, VkImage *image) override;
    void     destroy_image(VkDevice device, VkImage image) override;
    void     image_memory_requirements(VkDevice device, VkImage image, VkMemoryRequirements *requirements) override;
    VkResult bind_image_memory(VkDevice device, VkImage image, VkDeviceMemory memory) override;
    /* Feature 053: whether this host reports VK_KHR_external_memory_win32 and
     * what its export answers. Off by default, as a host with no fd sharing
     * is. */
    void     report_external_objects(void *handle);
    /* Feature 053: the device-extension enumeration answers this instead of
     * the host's own list - `count` extensions none of which the probe is
     * looking for, with `first` from the counting call and `second` from the
     * one that fills the names in. What the probe must survive is every one of
     * them: a host that refuses to be asked, one that claims more extensions
     * than the probe keeps room for, and one that lists only others. */
    void     answer_extensions(std::uint32_t count, VkResult first, VkResult second);
    /* An allocate that answers success and no memory, and a handle export that
     * answers success and no handle: a host keeping neither promise, which the
     * probe must read as a failed step rather than a passed one. */
    void     allocate_nothing();
    void     hand_out_no_handle();
    /* The NTSTATUS the driver reports after the free, which a release that
     * went wrong leaves non-zero. */
    void     refuse_the_release(std::uint32_t code);
    VkResult allocate_memory(VkDevice device, const VkMemoryAllocateInfo *info, VkDeviceMemory *memory) override;
    void     free_memory(VkDevice device, VkDeviceMemory memory) override;
    VkResult enumerate_device_extensions(VkPhysicalDevice physical_device, std::uint32_t *count,
                                         VkExtensionProperties *properties) override;
    std::uint32_t last_kernel_status(VkInstance instance, VkPhysicalDevice physical_device) override;
    /* The NTSTATUS a scripted driver says its last kernel call answered. */
    void          set_kernel_status(std::uint32_t code);
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
    VkResult wait_for_fence(VkDevice device, VkFence fence, std::uint64_t timeout) override;

    VkResult begin_command_buffer(VkCommandBuffer command_buffer, const VkCommandBufferBeginInfo *info) override;
    VkResult end_command_buffer(VkCommandBuffer command_buffer) override;
    void     cmd_bind_pipeline(VkCommandBuffer command_buffer, VkPipelineBindPoint point, VkPipeline pipeline) override;
    void     cmd_bind_compute_set(VkCommandBuffer command_buffer, VkPipelineLayout layout,
                                  VkDescriptorSet set) override;
    void     cmd_dispatch(VkCommandBuffer command_buffer, std::uint32_t groups_x) override;
    void     cmd_begin_render_pass(VkCommandBuffer command_buffer, const VkRenderPassBeginInfo *info) override;
    void     cmd_end_render_pass(VkCommandBuffer command_buffer) override;
    void     cmd_draw(VkCommandBuffer command_buffer, std::uint32_t vertices) override;
    void     cmd_copy_image_to_buffer(VkCommandBuffer command_buffer, VkImage image, VkImageLayout layout,
                                      VkBuffer buffer, const VkBufferImageCopy *region) override;
    void     cmd_fill_buffer(VkCommandBuffer command_buffer, VkBuffer buffer, VkDeviceSize size,
                             uint32_t value) override;

    VkResult queue_submit(VkQueue queue, const VkSubmitInfo *submit, VkFence fence) override;

    VkResult create_win32_surface(VkInstance instance, void *module, void *window, VkSurfaceKHR *surface) override;
    void     destroy_surface(VkInstance instance, VkSurfaceKHR surface) override;
    VkResult surface_capabilities(VkPhysicalDevice physical_device, VkSurfaceKHR surface,
                                  VkSurfaceCapabilitiesKHR *capabilities) override;
    VkResult surface_formats(VkPhysicalDevice physical_device, VkSurfaceKHR surface, std::uint32_t *count,
                             VkSurfaceFormatKHR *formats) override;
    VkResult surface_present_modes(VkPhysicalDevice physical_device, VkSurfaceKHR surface, std::uint32_t *count,
                                   VkPresentModeKHR *modes) override;
    VkResult create_swapchain(VkDevice device, const VkSwapchainCreateInfoKHR *info,
                              VkSwapchainKHR *swapchain) override;
    void     destroy_swapchain(VkDevice device, VkSwapchainKHR swapchain) override;
    VkResult swapchain_images(VkDevice device, VkSwapchainKHR swapchain, std::uint32_t *count,
                              VkImage *images) override;
    VkResult acquire_next_image(VkDevice device, VkSwapchainKHR swapchain, std::uint64_t timeout,
                                std::uint32_t *index) override;
    VkResult queue_present(VkQueue queue, const VkPresentInfoKHR *info) override;

private:
    enum class Kind : std::uint32_t { none, instance, device, buffer, image, memory, view, shader, set_layout,
                                      pipeline_layout, pipeline, descriptor_pool, render_pass, framebuffer,
                                      command_pool, fence, surface, swapchain };
    struct Object {
        std::uint64_t handle;
        Kind          kind;
        /* A buffer's memory, a memory's allocation slot, a fence's signal. */
        std::uint64_t link;
    };
    struct Allocation {
        std::uint64_t memory;
        std::uint64_t size;
        std::uint32_t type;
        std::uint8_t  bytes[max_mapped_bytes];
    };
    struct Recording {
        std::uint64_t set;
        std::uint32_t groups;
        std::uint32_t vertices;
        bool          in_render_pass;
        bool          drew;
        std::uint64_t copied_to;
        /* The buffer a fill names and the word it writes over it, which the
         * run puts into the memory that buffer is bound to. */
        std::uint64_t filled;
        std::uint32_t fill_value;
    };

    /* The scripted failure for this use of `call`, or VK_SUCCESS. */
    VkResult      use(Call call);
    std::uint64_t make(Kind kind, std::uint64_t link);
    void          destroy(Kind kind, std::uint64_t handle);
    Object       *find(Kind kind, std::uint64_t handle);
    Allocation   *allocation_of(std::uint64_t memory);
    void          run(const Recording &recording);

    std::uint32_t calls_[static_cast<std::uint32_t>(Call::count)];
    std::uint32_t fail_at_[static_cast<std::uint32_t>(Call::count)];
    VkResult      fail_with_[static_cast<std::uint32_t>(Call::count)];
    std::uint32_t physical_devices_;
    std::uint64_t luids_[max_physical_devices];
    bool          luid_valid_[max_physical_devices];
    std::uint32_t queue_families_;
    VkQueueFlags  queue_flags_;
    VkPhysicalDeviceMemoryProperties memory_;
    std::uint32_t buffer_types_;
    std::uint32_t image_types_;
    bool          miscompute_;
    bool          misdraw_;
    bool          hold_fences_;
    VkResult      fault_answer_;
    VkResult      idle_after_fault_;
    std::uint64_t luid_xor_;
    VkQueueFlags  flags_after_fault_;
    bool          lost_;
    Object        objects_[max_objects];
    std::uint32_t misuses_;
    std::uint64_t next_handle_;
    /* Feature 053: which external-object extensions this host reports, and the
     * handle its export answers - what a probe over it is judged against. */
    bool          external_objects_;
    void         *win32_handle_;
    bool          scripted_extensions_;
    std::uint32_t extension_count_;
    VkResult      extension_first_;
    VkResult      extension_second_;
    bool          allocate_nothing_;
    bool          no_handle_;
    std::uint32_t release_code_;
    std::uint32_t kernel_status_;
    Allocation    allocations_[max_allocations];
    std::uint32_t allocated_types_[max_allocations * 4u];
    std::uint32_t allocations_made_;
    Recording     recording_;
    std::uint32_t api_version_;
    std::uint32_t dispatched_groups_;
    std::uint32_t drawn_vertices_;
    std::uint64_t shader_words_;
    std::uint32_t device_parent_;
    ScriptedWindowFrame *window_;
    void         *window_handle_;
    Kmt          *pacing_kmt_;
    std::uint32_t pacing_adapter_;
    std::uint32_t pacing_device_;
    static constexpr std::uint32_t max_enabled = 8u;
    const char   *instance_names_[max_enabled];
    std::uint32_t instance_name_count_;
    const char   *device_names_[max_enabled];
    std::uint32_t device_name_count_;
    bool          held_extent_;
    bool          nothing_presentable_;
    bool          lost_surface_;
    bool          lost_present_modes_;
    bool          one_mode_;
    VkPresentModeKHR only_mode_;
    std::uint32_t least_images_;
    bool          lost_swapchain_images_;
    bool          one_image_;
    std::uint32_t held_width_;
    std::uint32_t held_height_;
    std::uint64_t swapchain_;
    std::uint32_t swapchain_width_;
    std::uint32_t swapchain_height_;
    std::uint64_t swapchain_images_[images_per_swapchain];
    std::uint64_t retired_;
    std::uint64_t retired_images_[images_per_swapchain];
    std::uint32_t next_image_;
    std::uint32_t presents_;
    bool          drew_since_present_;
};

}  // namespace av

#endif  // AV_SCRIPTED_VULKAN_HPP
