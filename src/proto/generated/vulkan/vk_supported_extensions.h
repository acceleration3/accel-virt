#pragma once


#include <stddef.h>

namespace av::vkproto {

// Device extensions this build generated entry points for.
//
// The ICD filters the driver's list against this one before
// handing it to the application. An extension missing here is not
// necessarily unsupported by the GPU — it is unsupported by THIS
// proxy, which is the only claim the ICD is entitled to make.
inline constexpr const char* kImplementedDeviceExtensions[] = {
    "VK_EXT_4444_formats",
    "VK_EXT_attachment_feedback_loop_layout",
    "VK_EXT_calibrated_timestamps",
    "VK_EXT_color_write_enable",
    "VK_EXT_conditional_rendering",
    "VK_EXT_custom_border_color",
    "VK_EXT_depth_clip_enable",
    "VK_EXT_descriptor_indexing",
    "VK_EXT_extended_dynamic_state",
    "VK_EXT_extended_dynamic_state2",
    "VK_EXT_extended_dynamic_state3",
    "VK_EXT_external_memory_dma_buf",
    "VK_EXT_graphics_pipeline_library",
    "VK_EXT_host_image_copy",
    "VK_EXT_line_rasterization",
    "VK_EXT_memory_budget",
    "VK_EXT_memory_priority",
    "VK_EXT_mesh_shader",
    "VK_EXT_pageable_device_local_memory",
    "VK_EXT_robustness2",
    "VK_EXT_transform_feedback",
    "VK_EXT_vertex_attribute_divisor",
    "VK_EXT_vertex_input_dynamic_state",
    "VK_KHR_acceleration_structure",
    "VK_KHR_bind_memory2",
    "VK_KHR_buffer_device_address",
    "VK_KHR_copy_commands2",
    "VK_KHR_create_renderpass2",
    "VK_KHR_dedicated_allocation",
    "VK_KHR_deferred_host_operations",
    "VK_KHR_descriptor_update_template",
    "VK_KHR_device_group",
    "VK_KHR_draw_indirect_count",
    "VK_KHR_dynamic_rendering",
    "VK_KHR_dynamic_rendering_local_read",
    "VK_KHR_external_fence",
    "VK_KHR_external_fence_fd",
    "VK_KHR_external_memory",
    "VK_KHR_external_memory_fd",
    "VK_KHR_external_semaphore",
    "VK_KHR_external_semaphore_fd",
    "VK_KHR_fragment_shading_rate",
    "VK_KHR_get_memory_requirements2",
    "VK_KHR_maintenance1",
    "VK_KHR_maintenance3",
    "VK_KHR_maintenance4",
    "VK_KHR_maintenance5",
    "VK_KHR_multiview",
    "VK_KHR_pipeline_library",
    "VK_KHR_push_descriptor",
    "VK_KHR_ray_query",
    "VK_KHR_ray_tracing_pipeline",
    "VK_KHR_sampler_ycbcr_conversion",
    "VK_KHR_shader_float_controls",
    "VK_KHR_shader_non_semantic_info",
    "VK_KHR_shader_relaxed_extended_instruction",
    "VK_KHR_spirv_1_4",
    "VK_KHR_swapchain",
    "VK_KHR_synchronization2",
    "VK_KHR_timeline_semaphore",
};

// Instance extensions, for the same reason. The ICD answers
// instance enumeration from a hand-written table rather than the
// driver, so this is a cross-check rather than a filter.
inline constexpr const char* kImplementedInstanceExtensions[] = {
    "VK_EXT_headless_surface",
    "VK_KHR_device_group_creation",
    "VK_KHR_external_fence_capabilities",
    "VK_KHR_external_memory_capabilities",
    "VK_KHR_external_semaphore_capabilities",
    "VK_KHR_get_physical_device_properties2",
    "VK_KHR_get_surface_capabilities2",
    "VK_KHR_surface",
    // The platform surface-creation extension is the one the presenter
    // is built for: Xlib presents with XPutImage, Win32 with
    // StretchDIBits. Advertising the other platform's name would offer a
    // surface nothing on this build can present through.
#if defined(_WIN32)
    "VK_KHR_win32_surface",
#else
    "VK_KHR_xlib_surface",
#endif
};

// Entry points the ICD must provide itself.
//
// These are ``[commands].exclude`` in the profile: commands that
// cannot be generated because they carry something no wire format
// holds — a window, a mapped pointer, a surface naming a window in
// this process. The lists above count them as implemented, which
// is what keeps VK_KHR_surface claimable at all.
//
// That makes this list a promise, and the ICD checks it at
// startup: a name here that its own table does not answer is an
// extension advertised over a null pointer, which is how a D3D8
// title came to exit with status 1 and no message.
inline constexpr const char* kManualEntryPoints[] = {
    "vkAcquireNextImageKHR",
    "vkCreateHeadlessSurfaceEXT",
    "vkCreateSwapchainKHR",
#if defined(_WIN32)
    "vkCreateWin32SurfaceKHR",
#else
    "vkCreateXlibSurfaceKHR",
#endif
    "vkDestroySurfaceKHR",
    "vkDestroySwapchainKHR",
    "vkEnumerateInstanceExtensionProperties",
    "vkEnumerateInstanceLayerProperties",
    "vkEnumerateInstanceVersion",
    "vkGetDeviceProcAddr",
    "vkGetInstanceProcAddr",
    "vkGetPhysicalDeviceSurfaceCapabilities2KHR",
    "vkGetPhysicalDeviceSurfaceCapabilitiesKHR",
    "vkGetPhysicalDeviceSurfaceFormats2KHR",
    "vkGetPhysicalDeviceSurfaceFormatsKHR",
    "vkGetPhysicalDeviceSurfacePresentModesKHR",
    "vkGetPhysicalDeviceSurfaceSupportKHR",
#if defined(_WIN32)
    "vkGetPhysicalDeviceWin32PresentationSupportKHR",
#else
    "vkGetPhysicalDeviceXlibPresentationSupportKHR",
#endif
    "vkGetSwapchainImagesKHR",
    "vkMapMemory",
    "vkQueuePresentKHR",
    "vkUnmapMemory",
};

}  // namespace av::vkproto
