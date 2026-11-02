#ifndef AV_VULKAN_OBJECT_KIND_HPP
#define AV_VULKAN_OBJECT_KIND_HPP

#include <stdint.h>

namespace av {

enum class VulkanObjectKind : uint8_t {
    other,
    instance,
    physical_device,
    device,
    queue,
    command_buffer,
    external_image,
    descriptor_template
};

}

#endif
