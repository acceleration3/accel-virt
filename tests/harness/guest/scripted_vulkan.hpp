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
    /* What vkRvkForceContextFault answers, and what the device answers to
     * vkDeviceWaitIdle after it. */
    void          answer_fault(VkResult fault, VkResult idle_after);
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
    VkResult allocate_memory(VkDevice device, const VkMemoryAllocateInfo *info, VkDeviceMemory *memory) override;
    void     free_memory(VkDevice device, VkDeviceMemory memory) override;
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

    VkResult queue_submit(VkQueue queue, const VkSubmitInfo *submit, VkFence fence) override;

private:
    enum class Kind : std::uint32_t { none, instance, device, buffer, image, memory, view, shader, set_layout,
                                      pipeline_layout, pipeline, descriptor_pool, render_pass, framebuffer,
                                      command_pool, fence };
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
    Allocation    allocations_[max_allocations];
    std::uint32_t allocated_types_[max_allocations * 4u];
    std::uint32_t allocations_made_;
    Recording     recording_;
    std::uint32_t api_version_;
    std::uint32_t dispatched_groups_;
    std::uint32_t drawn_vertices_;
    std::uint64_t shader_words_;
    std::uint32_t device_parent_;
};

}  // namespace av

#endif  // AV_SCRIPTED_VULKAN_HPP
