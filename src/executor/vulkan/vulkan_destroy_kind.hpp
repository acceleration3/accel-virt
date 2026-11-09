#ifndef AV_VULKAN_DESTROY_KIND_HPP
#define AV_VULKAN_DESTROY_KIND_HPP

#include <stdint.h>

namespace av {

enum class VulkanDestroyKind : uint8_t {
    none,
    buffer,
    image,
    image_view,
    buffer_view,
    sampler,
    pipeline,
    pipeline_layout,
    render_pass,
    framebuffer,
    query_pool,
    event,
    descriptor_set_layout,
    descriptor_pool,
    descriptor_pool_reset,
    descriptor_set,
    command_pool,
    command_buffer,
    memory,
    imported_memory,
    imported_semaphore,
    acceleration_structure,
    shader
};

}

#endif
