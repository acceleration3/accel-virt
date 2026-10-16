/* What a Vulkan handle the guest may name is, as far as this host has to
 * tell them apart.
 *
 * The five the loader dispatches on: the loader passes a non-dispatchable
 * handle to the driver as the number it is, while a dispatchable one it
 * dereferences to find the table to call through - so a guest that names a
 * queue where a device belongs aborts the host process rather than getting an
 * error back. Found by tests/fuzz/fuzz_executor_vulkan.cpp in
 * vkAllocateMemory.
 *
 * An image made with the memory source's handle type, the one kind of image
 * its memory may back (VulkanBackend::on_vkCreateImage), and a descriptor
 * update template, whose handle is kept with the bytes one update through it
 * reads (feature 047): each ends when the handle does.
 *
 * Everything else is `other`. */
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

}  // namespace av

#endif  // AV_VULKAN_OBJECT_KIND_HPP
