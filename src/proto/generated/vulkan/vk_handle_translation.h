#pragma once


#include <stdint.h>

#include <vulkan/vulkan_core.h>

namespace av::vkproto {

// The kinds of handle that cross the wire: one per handle type the
// profile carries.
enum class HandleKind : uint8_t {
    acceleration_structure_khr,
    acceleration_structure_nv,
    buffer,
    buffer_view,
    command_buffer,
    command_pool,
    deferred_operation_khr,
    descriptor_pool,
    descriptor_set,
    descriptor_set_layout,
    descriptor_update_template,
    device,
    device_memory,
    event,
    fence,
    framebuffer,
    image,
    image_view,
    instance,
    micromap_ext,
    physical_device,
    pipeline,
    pipeline_binary_khr,
    pipeline_cache,
    pipeline_layout,
    private_data_slot,
    query_pool,
    queue,
    render_pass,
    sampler,
    sampler_ycbcr_conversion,
    semaphore,
    shader_ext,
    shader_module,
    surface_khr,
    swapchain_khr,
    validation_cache_ext,
};

// What a handle is on the other side of the wire, and the one length a
// call leaves for its own side to know. Every encoder and decoder below
// that can meet a handle is handed one: on the host it is the
// ServerDispatch the frame is for, which answers only for the handles it
// made and refuses every other number a guest names (feature 042); in the
// guest ICD it unwraps the handles the loader dispatches on into the
// host's own; HandleIdentity is a caller's whose handles are already the
// host's numbers. The same shape as OpenCL's ServerDispatch::handle_for.
class HandleTranslation {
public:
    // The number `handle`, a `kind`, crosses the wire as.
    [[nodiscard]] virtual uint64_t wire_for(HandleKind kind, uint64_t handle) = 0;
    // The `kind` the number `wire` names, into `handle`: false for a
    // number that names none, and the frame is refused. Null is a value
    // Vulkan gives most handles and never a dispatchable one, so it is
    // judged here too rather than passed.
    [[nodiscard]] virtual bool handle_for(HandleKind kind, uint64_t wire, uint64_t& handle) const = 0;
    // How many bytes one update through `made` reads. The length of a
    // descriptor blob is the template's, from the entries it was created
    // with, and nothing in the call carries it: each side answers from its
    // own table of templates, 0 for one it never made (feature 047).
    [[nodiscard]] virtual uint64_t template_bytes(VkDescriptorUpdateTemplate made) const = 0;

protected:
    HandleTranslation() = default;
    ~HandleTranslation() = default;
};

// The numbers are the caller's own, and it made no templates: the
// guest's test programs, which speak to the executor in the host's
// handles directly.
class HandleIdentity final : public HandleTranslation {
public:
    [[nodiscard]] uint64_t wire_for(HandleKind kind, uint64_t handle) override;
    [[nodiscard]] bool handle_for(HandleKind kind, uint64_t wire, uint64_t& handle) const override;
    [[nodiscard]] uint64_t template_bytes(VkDescriptorUpdateTemplate made) const override;
};

}  // namespace av::vkproto
