#include "icd/vulkan/icd_root.hpp"

#include "guest/transport/shared_object_call.hpp"
#include "icd/vulkan/handle_bits.hpp"
#include "proto/resources/allocation_descriptor.hpp"
#include "proto/resources/allocation_kind.hpp"
#include "proto/resources/scanout_target.hpp"
#include "proto/resources/shared_object_kind.hpp"
#include "proto/resources/shared_object_record.hpp"
#include "proto/resources/surface_layout.hpp"

#include "core/support/scope_exit.hpp"
#include "core/support/text_buffer.hpp"
#include "core/trace/function_scope.hpp"
#include "core/trace/state_writer.hpp"
#include "guest/kmt/kmt.hpp"
#include "guest/transport/context_fault.hpp"
#include "icd/vulkan/host_extension_list.hpp"
#include "icd/vulkan/icd_binding.hpp"
#include "icd/vulkan/local_entries.hpp"
#include "icd/common/log_sink.hpp"
#include "icd/vulkan/memory_allocate_chain.hpp"
#include "icd/vulkan/memory_type_table.hpp"
#include "icd/vulkan/physical_device_view.hpp"
#include "icd/common/sliced_wait.hpp"
#include "icd/common/thread_lock.hpp"
#include "proto/generated/vulkan/vk_dispatch_client.h"
#include "proto/generated/vulkan/vk_serialize.h"
#include "proto/generated/vulkan/vk_icd_entries.h"
#include "proto/wire/foreign_enum.hpp"

namespace av::icd {

namespace {

/* The host extension a shared allocation is exported through, and the
 * application extension whose presence asks for it (feature 053). */
constexpr const char *dma_buf_extension = "VK_EXT_external_memory_dma_buf";
constexpr const char *win32_memory_extension = "VK_KHR_external_memory_win32";

/* Whether `count` names hold `wanted`. Every name is known to be there: the one
 * caller has already put the list through first_refused and HostExtensionList,
 * either of which answers a null before this runs, so a check for one here
 * would be a branch nothing can take. */
bool names_hold(const char *const *names, uint32_t count, const char *wanted)
{
    AV_SCOPE("names_hold");
    if (names == nullptr) return false;
    for (uint32_t index = 0u; index < count; index++)
        if (__builtin_strcmp(names[index], wanted) == 0) return true;
    return false;
}

using Entry = vkicd::generated::Entry;
using Level = vkicd::generated::DispatchLevel;

/* The first rows are what the loader may ask for with no instance
 * (loader_interface.md); the rest answer only through an instance. */
constexpr uint32_t before_instance_rows = 6u;
const Entry        local_rows[] = {
    {"vkCreateInstance", reinterpret_cast<PFN_vkVoidFunction>(&local_vkCreateInstance), Level::global},
    {"vkEnumerateInstanceExtensionProperties",
            reinterpret_cast<PFN_vkVoidFunction>(&local_vkEnumerateInstanceExtensionProperties), Level::global},
    {"vkEnumerateInstanceVersion", reinterpret_cast<PFN_vkVoidFunction>(&local_vkEnumerateInstanceVersion), Level::global},
    {"vk_icdNegotiateLoaderICDInterfaceVersion",
            reinterpret_cast<PFN_vkVoidFunction>(&local_vk_icdNegotiateLoaderICDInterfaceVersion), Level::global},
    {"vk_icdGetPhysicalDeviceProcAddr",
            reinterpret_cast<PFN_vkVoidFunction>(&local_vk_icdGetPhysicalDeviceProcAddr), Level::global},
    {"vk_icdEnumerateAdapterPhysicalDevices",
            reinterpret_cast<PFN_vkVoidFunction>(&local_vk_icdEnumerateAdapterPhysicalDevices), Level::global},
    {"vkGetDeviceProcAddr", reinterpret_cast<PFN_vkVoidFunction>(&local_vkGetDeviceProcAddr), Level::device},
    {"vkDestroyDevice", reinterpret_cast<PFN_vkVoidFunction>(&local_vkDestroyDevice), Level::device},
    {"vkEnumerateDeviceExtensionProperties",
            reinterpret_cast<PFN_vkVoidFunction>(&local_vkEnumerateDeviceExtensionProperties), Level::physical_device},
    {"vkCreateDevice", reinterpret_cast<PFN_vkVoidFunction>(&local_vkCreateDevice), Level::physical_device},
    {"vkGetPhysicalDeviceProperties", reinterpret_cast<PFN_vkVoidFunction>(&local_vkGetPhysicalDeviceProperties), Level::physical_device},
    {"vkGetPhysicalDeviceProperties2", reinterpret_cast<PFN_vkVoidFunction>(&local_vkGetPhysicalDeviceProperties2), Level::physical_device},
    {"vkGetDeviceQueue", reinterpret_cast<PFN_vkVoidFunction>(&local_vkGetDeviceQueue), Level::device},
    {"vkGetDeviceQueue2", reinterpret_cast<PFN_vkVoidFunction>(&local_vkGetDeviceQueue2), Level::device},
    {"vkAllocateCommandBuffers", reinterpret_cast<PFN_vkVoidFunction>(&local_vkAllocateCommandBuffers), Level::device},
    {"vkFreeCommandBuffers", reinterpret_cast<PFN_vkVoidFunction>(&local_vkFreeCommandBuffers), Level::device},
    {"vkBeginCommandBuffer", reinterpret_cast<PFN_vkVoidFunction>(&local_vkBeginCommandBuffer), Level::device},
    {"vkEndCommandBuffer", reinterpret_cast<PFN_vkVoidFunction>(&local_vkEndCommandBuffer), Level::device},
    {"vkResetCommandBuffer", reinterpret_cast<PFN_vkVoidFunction>(&local_vkResetCommandBuffer), Level::device},
    {"vkWaitForFences", reinterpret_cast<PFN_vkVoidFunction>(&local_vkWaitForFences), Level::device},
    {"vkWaitSemaphores", reinterpret_cast<PFN_vkVoidFunction>(&local_vkWaitSemaphores), Level::device},
    {"vkWaitSemaphoresKHR", reinterpret_cast<PFN_vkVoidFunction>(&local_vkWaitSemaphores), Level::device},
    {"vkQueueWaitIdle", reinterpret_cast<PFN_vkVoidFunction>(&local_vkQueueWaitIdle), Level::device},
    {"vkDeviceWaitIdle", reinterpret_cast<PFN_vkVoidFunction>(&local_vkDeviceWaitIdle), Level::device},
    {"vkDestroyInstance", reinterpret_cast<PFN_vkVoidFunction>(&local_vkDestroyInstance), Level::instance},
    {"vkAvForceContextFault", reinterpret_cast<PFN_vkVoidFunction>(&local_vkAvForceContextFault),
            Level::physical_device},
    {"vkAvLastKernelStatus", reinterpret_cast<PFN_vkVoidFunction>(&local_vkAvLastKernelStatus),
            Level::physical_device},
    {"vkGetPhysicalDeviceMemoryProperties",
            reinterpret_cast<PFN_vkVoidFunction>(&local_vkGetPhysicalDeviceMemoryProperties), Level::physical_device},
    {"vkGetPhysicalDeviceMemoryProperties2",
            reinterpret_cast<PFN_vkVoidFunction>(&local_vkGetPhysicalDeviceMemoryProperties2), Level::physical_device},
    {"vkAllocateMemory", reinterpret_cast<PFN_vkVoidFunction>(&local_vkAllocateMemory), Level::device},
    {"vkFreeMemory", reinterpret_cast<PFN_vkVoidFunction>(&local_vkFreeMemory), Level::device},
    {"vkCreateDescriptorUpdateTemplate",
            reinterpret_cast<PFN_vkVoidFunction>(&local_vkCreateDescriptorUpdateTemplate), Level::device},
    {"vkCreateDescriptorUpdateTemplateKHR",
            reinterpret_cast<PFN_vkVoidFunction>(&local_vkCreateDescriptorUpdateTemplateKHR), Level::device},
    {"vkDestroyDescriptorUpdateTemplate",
            reinterpret_cast<PFN_vkVoidFunction>(&local_vkDestroyDescriptorUpdateTemplate), Level::device},
    {"vkDestroyDescriptorUpdateTemplateKHR",
            reinterpret_cast<PFN_vkVoidFunction>(&local_vkDestroyDescriptorUpdateTemplateKHR), Level::device},
    {"vkGetMemoryWin32HandleKHR", reinterpret_cast<PFN_vkVoidFunction>(&local_vkGetMemoryWin32HandleKHR),
            Level::device},
    {"vkGetMemoryWin32HandlePropertiesKHR",
            reinterpret_cast<PFN_vkVoidFunction>(&local_vkGetMemoryWin32HandlePropertiesKHR), Level::device},
    {"vkGetSemaphoreWin32HandleKHR", reinterpret_cast<PFN_vkVoidFunction>(&local_vkGetSemaphoreWin32HandleKHR),
            Level::device},
    {"vkImportSemaphoreWin32HandleKHR",
            reinterpret_cast<PFN_vkVoidFunction>(&local_vkImportSemaphoreWin32HandleKHR), Level::device},
    {"vkDestroySemaphore", reinterpret_cast<PFN_vkVoidFunction>(&local_vkDestroySemaphore), Level::device},
    {"vkGetPhysicalDeviceExternalBufferProperties",
            reinterpret_cast<PFN_vkVoidFunction>(&local_vkGetPhysicalDeviceExternalBufferProperties),
            Level::physical_device},
    {"vkGetPhysicalDeviceExternalBufferPropertiesKHR",
            reinterpret_cast<PFN_vkVoidFunction>(&local_vkGetPhysicalDeviceExternalBufferProperties),
            Level::physical_device},
    {"vkGetPhysicalDeviceExternalSemaphoreProperties",
            reinterpret_cast<PFN_vkVoidFunction>(&local_vkGetPhysicalDeviceExternalSemaphoreProperties),
            Level::physical_device},
    {"vkGetPhysicalDeviceExternalSemaphorePropertiesKHR",
            reinterpret_cast<PFN_vkVoidFunction>(&local_vkGetPhysicalDeviceExternalSemaphoreProperties),
            Level::physical_device},
    {"vkMapMemory", reinterpret_cast<PFN_vkVoidFunction>(&local_vkMapMemory), Level::device},
    {"vkMapMemory2KHR", reinterpret_cast<PFN_vkVoidFunction>(&local_vkMapMemory2KHR), Level::device},
    {"vkUnmapMemory2KHR", reinterpret_cast<PFN_vkVoidFunction>(&local_vkUnmapMemory2KHR), Level::device},
    {"vkUnmapMemory", reinterpret_cast<PFN_vkVoidFunction>(&local_vkUnmapMemory), Level::device},
    {"vkFlushMappedMemoryRanges", reinterpret_cast<PFN_vkVoidFunction>(&local_vkFlushMappedMemoryRanges), Level::device},
    {"vkInvalidateMappedMemoryRanges",
            reinterpret_cast<PFN_vkVoidFunction>(&local_vkInvalidateMappedMemoryRanges), Level::device},
    {"vkCreateWin32SurfaceKHR", reinterpret_cast<PFN_vkVoidFunction>(&local_vkCreateWin32SurfaceKHR), Level::instance},
    {"vkDestroySurfaceKHR", reinterpret_cast<PFN_vkVoidFunction>(&local_vkDestroySurfaceKHR), Level::instance},
    {"vkGetPhysicalDeviceSurfaceSupportKHR",
            reinterpret_cast<PFN_vkVoidFunction>(&local_vkGetPhysicalDeviceSurfaceSupportKHR), Level::physical_device},
    {"vkGetPhysicalDeviceWin32PresentationSupportKHR",
            reinterpret_cast<PFN_vkVoidFunction>(&local_vkGetPhysicalDeviceWin32PresentationSupportKHR),
            Level::physical_device},
    {"vkGetPhysicalDeviceSurfaceCapabilitiesKHR",
            reinterpret_cast<PFN_vkVoidFunction>(&local_vkGetPhysicalDeviceSurfaceCapabilitiesKHR), Level::physical_device},
    {"vkGetPhysicalDeviceSurfaceCapabilities2KHR",
            reinterpret_cast<PFN_vkVoidFunction>(&local_vkGetPhysicalDeviceSurfaceCapabilities2KHR), Level::physical_device},
    {"vkGetPhysicalDeviceSurfaceFormatsKHR",
            reinterpret_cast<PFN_vkVoidFunction>(&local_vkGetPhysicalDeviceSurfaceFormatsKHR), Level::physical_device},
    {"vkGetPhysicalDeviceSurfaceFormats2KHR",
            reinterpret_cast<PFN_vkVoidFunction>(&local_vkGetPhysicalDeviceSurfaceFormats2KHR), Level::physical_device},
    {"vkGetPhysicalDeviceSurfacePresentModesKHR",
            reinterpret_cast<PFN_vkVoidFunction>(&local_vkGetPhysicalDeviceSurfacePresentModesKHR), Level::physical_device},
    {"vkCreateSwapchainKHR", reinterpret_cast<PFN_vkVoidFunction>(&local_vkCreateSwapchainKHR), Level::device},
    {"vkDestroySwapchainKHR", reinterpret_cast<PFN_vkVoidFunction>(&local_vkDestroySwapchainKHR), Level::device},
    {"vkGetSwapchainImagesKHR", reinterpret_cast<PFN_vkVoidFunction>(&local_vkGetSwapchainImagesKHR), Level::device},
    {"vkAcquireNextImageKHR", reinterpret_cast<PFN_vkVoidFunction>(&local_vkAcquireNextImageKHR), Level::device},
    {"vkQueuePresentKHR", reinterpret_cast<PFN_vkVoidFunction>(&local_vkQueuePresentKHR), Level::device},
};
constexpr uint32_t local_row_count = sizeof local_rows / sizeof local_rows[0];
/* VK_API_VERSION_1_3, spelled without the header macro's C casts. */
constexpr uint32_t vulkan_1_3 = (1u << 22u) | (3u << 12u);
constexpr size_t   line_bytes = 160u;
/* The device extensions this ICD reports (research.md §9), each with the test
 * that exercises it through the ICD; tools/check_extension_policy.py holds every
 * row to a test that exists and a name the generator implemented. The probe's
 * arrive with T037. */
constexpr ExtensionPolicy::Row reported_extensions[] = {
    {"VK_EXT_memory_budget", 1u, "icd_memory.the_budget_nodes_in_the_chain_are_the_adapter_s"},
    {"VK_KHR_swapchain", 70u, "icd_presentation.a_swapchain_presents_its_frame_on_the_window"},
    /* Feature 047. VK_EXT_map_memory_placed is deliberately absent: a chunk's
     * view is at the address VidMm chose, so this driver cannot place a
     * mapping and does not claim to (spec.md Clarifications, research.md 9). */
    {"VK_KHR_map_memory2", 1u, "icd_memory.map_memory2_maps_what_vk_map_memory_does_and_refuses_placement"},
    {"VK_KHR_descriptor_update_template", 1u,
            "icd_descriptor_templates.a_template_is_remembered_and_sizes_the_blob_it_carries"},
    {"VK_KHR_push_descriptor", 2u,
            "fullstack_templates.a_descriptor_set_updated_through_a_template_reaches_the_host"},
    /* The two Win32 external-object extensions, answered over the host's own
     * fd extensions: no Linux driver has a Win32 handle to give, so what the
     * host must have is the mechanism behind them (feature 047, US3,
     * research.md §10). Their version is this ICD's, since the host's is a
     * different extension's. VK_KHR_external_memory_win32's KMT and Direct3D
     * handle types are deliberately absent - answered as absent by the
     * external-property queries - because nothing here turns a Direct3D object
     * into a host one. */
    {"VK_KHR_external_memory_win32", 1u,
            "icd_external_handles.a_handle_exported_is_opened_by_the_device_on_import",
            "VK_KHR_external_memory_fd"},
    {"VK_KHR_external_semaphore_win32", 1u,
            "icd_external_handles.a_semaphore_handle_becomes_the_host_s_own_fd_import",
            "VK_KHR_external_semaphore_fd"},
    /* Minecraft's Vulkan backend declines a device without all three - "does
     * not support required extensions from FeatureSet [Minecraft base
     * required]" - and falls back to OpenGL. Their commands are core 1.3 or
     * aliases the generator already emits, and their features are structures
     * the chain carries to the host. */
    {"VK_KHR_dynamic_rendering", 1u, "reported_extensions.every_feature_structure_an_application_chains_reaches_the_host"},
    {"VK_KHR_synchronization2", 1u, "reported_extensions.every_feature_structure_an_application_chains_reaches_the_host"},
    {"VK_EXT_vertex_attribute_divisor", 3u,
            "reported_extensions.every_feature_structure_an_application_chains_reaches_the_host"},
    /* DXVK before 2.0 declines a device without it: Portal's Vulkan
     * renderer (DXVK 1.8.1) failed "Required Vulkan extension
     * VK_KHR_image_format_list not supported". Core in 1.2; its one
     * structure crosses in an image's create. */
    {"VK_KHR_image_format_list", 1u, "reported_extensions.an_image_s_format_list_reaches_the_host"},
    /* What DXVK 3.1 and vkd3d-proton 3.0 use (audit of third_party/,
     * 2026-09-30). DXVK requires robustness2, depth clip enable, load/store
     * op none and maintenance 5 and 6, and vkd3d-proton robustness2; without
     * them neither makes a device. The rest each open a path the libraries
     * take when offered. Deliberately absent: the ray-tracing family (not
     * yet proved end to end), calibrated timestamps (the host's time domains
     * are not the guest's), AMD anti-lag (its markers are the host's
     * presents, not these), and descriptor buffers (the profile says why). */
    {"VK_EXT_robustness2", 1u, "reported_extensions.every_feature_structure_an_application_chains_reaches_the_host"},
    {"VK_EXT_depth_clip_enable", 1u, "reported_extensions.every_feature_structure_an_application_chains_reaches_the_host"},
    {"VK_EXT_custom_border_color", 12u, "reported_extensions.every_feature_structure_an_application_chains_reaches_the_host"},
    {"VK_EXT_4444_formats", 1u, "reported_extensions.every_feature_structure_an_application_chains_reaches_the_host"},
    {"VK_EXT_attachment_feedback_loop_layout", 2u, "reported_extensions.every_feature_structure_an_application_chains_reaches_the_host"},
    {"VK_EXT_memory_priority", 1u, "reported_extensions.every_feature_structure_an_application_chains_reaches_the_host"},
    {"VK_EXT_pageable_device_local_memory", 1u, "reported_extensions.every_feature_structure_an_application_chains_reaches_the_host"},
    {"VK_EXT_image_view_min_lod", 1u, "reported_extensions.every_feature_structure_an_application_chains_reaches_the_host"},
    {"VK_EXT_mutable_descriptor_type", 1u, "reported_extensions.every_feature_structure_an_application_chains_reaches_the_host"},
    {"VK_EXT_graphics_pipeline_library", 1u, "reported_extensions.every_feature_structure_an_application_chains_reaches_the_host"},
    {"VK_EXT_transform_feedback", 1u, "reported_extensions.every_feature_structure_an_application_chains_reaches_the_host"},
    {"VK_KHR_maintenance5", 1u, "reported_extensions.every_feature_structure_an_application_chains_reaches_the_host"},
    {"VK_KHR_maintenance6", 1u, "reported_extensions.every_feature_structure_an_application_chains_reaches_the_host"},
    {"VK_KHR_maintenance7", 1u, "reported_extensions.every_feature_structure_an_application_chains_reaches_the_host"},
    {"VK_KHR_maintenance8", 1u, "reported_extensions.every_feature_structure_an_application_chains_reaches_the_host"},
    {"VK_EXT_extended_dynamic_state", 1u, "reported_extensions.every_feature_structure_an_application_chains_reaches_the_host"},
    {"VK_EXT_extended_dynamic_state2", 1u, "reported_extensions.every_feature_structure_an_application_chains_reaches_the_host"},
    {"VK_EXT_extended_dynamic_state3", 2u, "reported_extensions.every_feature_structure_an_application_chains_reaches_the_host"},
    {"VK_EXT_line_rasterization", 1u, "reported_extensions.every_feature_structure_an_application_chains_reaches_the_host"},
    {"VK_EXT_conditional_rendering", 2u, "reported_extensions.every_feature_structure_an_application_chains_reaches_the_host"},
    {"VK_EXT_mesh_shader", 1u, "reported_extensions.every_feature_structure_an_application_chains_reaches_the_host"},
    {"VK_KHR_fragment_shading_rate", 2u, "reported_extensions.every_feature_structure_an_application_chains_reaches_the_host"},
    {"VK_KHR_dynamic_rendering_local_read", 1u, "reported_extensions.every_feature_structure_an_application_chains_reaches_the_host"},
    {"VK_EXT_border_color_swizzle", 1u, "reported_extensions.every_feature_structure_an_application_chains_reaches_the_host"},
    {"VK_EXT_depth_bias_control", 1u, "reported_extensions.every_feature_structure_an_application_chains_reaches_the_host"},
    {"VK_EXT_dynamic_rendering_unused_attachments", 1u, "reported_extensions.every_feature_structure_an_application_chains_reaches_the_host"},
    {"VK_EXT_fragment_shader_interlock", 1u, "reported_extensions.every_feature_structure_an_application_chains_reaches_the_host"},
    {"VK_EXT_non_seamless_cube_map", 1u, "reported_extensions.every_feature_structure_an_application_chains_reaches_the_host"},
    {"VK_EXT_shader_module_identifier", 1u, "reported_extensions.every_feature_structure_an_application_chains_reaches_the_host"},
    {"VK_KHR_shader_float_controls2", 1u, "reported_extensions.every_feature_structure_an_application_chains_reaches_the_host"},
    {"VK_KHR_shader_subgroup_uniform_control_flow", 1u, "reported_extensions.every_feature_structure_an_application_chains_reaches_the_host"},
    {"VK_KHR_compute_shader_derivatives", 1u, "reported_extensions.every_feature_structure_an_application_chains_reaches_the_host"},
    {"VK_KHR_fragment_shader_barycentric", 1u, "reported_extensions.every_feature_structure_an_application_chains_reaches_the_host"},
    {"VK_EXT_shader_image_atomic_int64", 1u, "reported_extensions.every_feature_structure_an_application_chains_reaches_the_host"},
    {"VK_KHR_shader_maximal_reconvergence", 1u, "reported_extensions.every_feature_structure_an_application_chains_reaches_the_host"},
    {"VK_KHR_shader_quad_control", 1u, "reported_extensions.every_feature_structure_an_application_chains_reaches_the_host"},
    {"VK_EXT_image_sliced_view_of_3d", 1u, "reported_extensions.every_feature_structure_an_application_chains_reaches_the_host"},
    {"VK_EXT_image_compression_control", 1u, "reported_extensions.every_feature_structure_an_application_chains_reaches_the_host"},
    {"VK_AMD_device_coherent_memory", 1u, "reported_extensions.every_feature_structure_an_application_chains_reaches_the_host"},
    {"VK_MESA_image_alignment_control", 1u, "reported_extensions.every_feature_structure_an_application_chains_reaches_the_host"},
    {"VK_KHR_cooperative_matrix", 2u, "reported_extensions.every_feature_structure_an_application_chains_reaches_the_host"},
    {"VK_KHR_pipeline_library", 1u, "reported_extensions.a_pipeline_library_in_a_create_reaches_the_host"},
    {"VK_KHR_load_store_op_none", 1u, "reported_extensions.an_extension_with_no_structure_of_its_own_is_enabled_on_the_host"},
    {"VK_EXT_shader_stencil_export", 1u, "reported_extensions.an_extension_with_no_structure_of_its_own_is_enabled_on_the_host"},
    {"VK_AMD_shader_fragment_mask", 1u, "reported_extensions.an_extension_with_no_structure_of_its_own_is_enabled_on_the_host"},
    {"VK_AMD_shader_explicit_vertex_parameter", 1u, "reported_extensions.an_extension_with_no_structure_of_its_own_is_enabled_on_the_host"},
    {"VK_EXT_conservative_rasterization", 1u, "reported_extensions.an_extension_with_no_structure_of_its_own_is_enabled_on_the_host"},
    {"VK_EXT_sample_locations", 1u, "reported_extensions.an_extension_with_no_structure_of_its_own_is_enabled_on_the_host"},
    {"VK_AMD_buffer_marker", 1u, "reported_extensions.an_extension_with_no_structure_of_its_own_is_enabled_on_the_host"},
    {"VK_AMD_shader_core_properties", 2u, "reported_extensions.an_extension_with_no_structure_of_its_own_is_enabled_on_the_host"},
    {"VK_AMD_shader_core_properties2", 1u, "reported_extensions.an_extension_with_no_structure_of_its_own_is_enabled_on_the_host"},
};
constexpr uint32_t reported_extension_count = sizeof reported_extensions / sizeof reported_extensions[0];
/* The instance extensions this ICD reports. The *2 queries are its own entry
 * points, and vulkaninfo reads a physical device's LUID only when this
 * extension is listed (the live --vulkan run of 045). */
struct InstanceExtension {
    const char *name;
    uint32_t    spec_version;
};
constexpr InstanceExtension reported_instance_extensions[] = {
    {"VK_KHR_get_physical_device_properties2", 2u},
    {"VK_KHR_surface", 25u},
    {"VK_KHR_win32_surface", 6u},
    /* DXVK resolves vkGetPhysicalDeviceSurfaceCapabilities2KHR whether or not
     * it enabled the extension, and answering null there does not make it fall
     * back - it makes D3D device creation fail
     * (tools/codegen/profiles/vulkan.toml:19-25). The ICD answers both *2
     * queries itself (feature 046), so it reports the extension that names
     * them (feature 047, US1 scenario 1). */
    {"VK_KHR_get_surface_capabilities2", 1u},
};
constexpr uint32_t reported_instance_extension_count =
    sizeof reported_instance_extensions / sizeof reported_instance_extensions[0];
/* The application's structure and the nodes after it that a memory properties
 * query looks at; a longer or cyclic chain is looked at this far. */
constexpr uint32_t max_chain = 32u;
/* More than any GPU driver lists: a host claiming more is not asked for them. */
constexpr uint32_t max_host_extensions = 1024u;
/* The most command buffers one vkAllocateCommandBuffers stages the host's
 * handles for, on the stack; the generated wrappers hold the same bound. */
constexpr uint32_t max_allocated_at_once = 1024u;

}  // namespace

IcdRoot::IcdRoot(BlockSource &memory, Kmt &kmt, ThreadLock &lock, ThreadLock &send_lock, ThreadLock &memory_lock,
                 LogSink &log, IcdBinding &binding, WindowSurface &windows, WindowChains &chains)
    : memory_(memory), kmt_(kmt), lock_(lock), send_lock_(send_lock), memory_lock_(memory_lock), log_(log),
      binding_(binding),
      api_version_(vulkan_1_3), before_instance_(local_rows, before_instance_rows),
      entry_points_(local_rows, local_row_count),
      extensions_(reported_extensions, reported_extension_count), batches_(memory), memories_(memory), handles_(memory), connections_(memory), pending_(nullptr),
      entries_(*this), presentation_(memory, kmt, windows, chains, log, *this, entries_), templates_(lock),
      externals_(lock), shared_handles_(kmt, lock), kernel_refusal_(0u), idle_skipped_(0u), idle_waited_(0u),
      memory_handles_(VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT,
                      VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT),
      semaphore_handles_(VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT,
                         VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT)
{
    AV_SCOPE("IcdRoot::IcdRoot");
    binding_.bind(*this);
}

IcdRoot::~IcdRoot()
{
    AV_SCOPE("IcdRoot::~IcdRoot");
    binding_.unbind();
}

void IcdRoot::note(const char *what, uint64_t value)
{
    AV_SCOPE("IcdRoot::note");
    char       line[line_bytes] = {};
    TextBuffer text(line, sizeof line);
    text.append("avvk: ");
    text.append(what);
    text.append(" ");
    text.append_decimal(value);
    log_.write(text.text());
}

bool IcdRoot::lost(const Connection &connection, const char *entry)
{
    AV_SCOPE("IcdRoot::lost");
    if (!connection.faulted()) return false;
    char       line[line_bytes] = {};
    TextBuffer text(line, sizeof line);
    text.append("avvk: ");
    text.append(entry);
    text.append(" on a lost device");
    log_.write(text.text());
    return true;
}

VkResult IcdRoot::negotiate(uint32_t *version)
{
    AV_SCOPE("IcdRoot::negotiate");
    if (version == nullptr) {
        log_.write("avvk: loader interface negotiation without a version");
        return VK_ERROR_INCOMPATIBLE_DRIVER;
    }
    if (*version < oldest_interface) {
        note("loader interface version refused:", *version);
        return VK_ERROR_INCOMPATIBLE_DRIVER;
    }
    if (*version > latest_interface) *version = latest_interface;
    return VK_SUCCESS;
}

PFN_vkVoidFunction IcdRoot::instance_proc_addr(VkInstance instance, const char *name)
{
    AV_SCOPE("IcdRoot::instance_proc_addr");
    if (instance == VK_NULL_HANDLE) return before_instance_.find_local(name);
    return connection_of(instance, LoaderHandle::Kind::instance) != nullptr ? entry_points_.find(name) : nullptr;
}

PFN_vkVoidFunction IcdRoot::physical_device_proc_addr(const char *name) const
{
    AV_SCOPE("IcdRoot::physical_device_proc_addr");
    return entry_points_.find_at_level(vkicd::generated::DispatchLevel::physical_device, name);
}

PFN_vkVoidFunction IcdRoot::device_proc_addr(VkDevice device, const char *name)
{
    AV_SCOPE("IcdRoot::device_proc_addr");
    if (connection_of(device, LoaderHandle::Kind::device) == nullptr) return nullptr;
    return entry_points_.find_at_level(vkicd::generated::DispatchLevel::device, name);
}

VkResult IcdRoot::enumerate_instance_extensions(const char *layer, uint32_t *count, VkExtensionProperties *properties)
{
    AV_SCOPE("IcdRoot::enumerate_instance_extensions");
    if (layer != nullptr) {
        log_.write("avvk: instance extensions of a layer asked for; this driver is no layer");
        return VK_ERROR_LAYER_NOT_PRESENT;
    }
    if (properties == nullptr) {
        *count = reported_instance_extension_count;
        return VK_SUCCESS;
    }
    const uint32_t written = *count < reported_instance_extension_count ? *count : reported_instance_extension_count;
    for (uint32_t index = 0u; index < written; index++) {
        properties[index] = VkExtensionProperties{};
        __builtin_memcpy(properties[index].extensionName, reported_instance_extensions[index].name,
                         __builtin_strlen(reported_instance_extensions[index].name) + 1u);
        properties[index].specVersion = reported_instance_extensions[index].spec_version;
    }
    *count = written;
    return written < reported_instance_extension_count ? VK_INCOMPLETE : VK_SUCCESS;
}

VkResult IcdRoot::enumerate_instance_version(uint32_t *version)
{
    AV_SCOPE("IcdRoot::enumerate_instance_version");
    *version = api_version_;
    return VK_SUCCESS;
}

Status IcdRoot::open_connection(Connection *&opened)
{
    AV_SCOPE("IcdRoot::open_connection");
    lock_.acquire();
    ScopeExit release([&] { lock_.release(); });
    const Status made = connections_.create(opened, kmt_, memory_, send_lock_);
    if (made != Status::ok) return made;
    const Status status = opened->open();
    if (status == Status::ok) return Status::ok;
    static_cast<void>(connections_.destroy(opened));
    opened = nullptr;
    return status;
}

VkResult IcdRoot::create_instance(const VkInstanceCreateInfo *create, const VkAllocationCallbacks *allocator,
                                  VkInstance *instance)
{
    AV_SCOPE("IcdRoot::create_instance");
    Connection  *opened = nullptr;
    const Status status = open_connection(opened);
    if (status != Status::ok) {
        note("vkCreateInstance: no Vulkan connection to an avgpu adapter, status", static_cast<uint64_t>(status));
        return status == Status::no_memory ? VK_ERROR_OUT_OF_HOST_MEMORY : VK_ERROR_INCOMPATIBLE_DRIVER;
    }
    pending_ = opened;
    HostExtensionList asked;
    if (asked.take(create->ppEnabledExtensionNames, create->enabledExtensionCount) != Status::ok) {
        note("vkCreateInstance: the extensions it names are not a list this driver can pass on, count",
             create->enabledExtensionCount);
        release_connection(opened);
        pending_ = nullptr;
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
    /* The window-system extensions are answered here, not by the host: it runs
     * on the host's own platform and has none of them (feature 046). */
    VkInstanceCreateInfo forwarded = *create;
    forwarded.enabledExtensionCount = asked.count();
    forwarded.ppEnabledExtensionNames = asked.names();
    const VkResult result = vkicd::generated::Wrap_vkCreateInstance(&forwarded, allocator, instance);
    if (result != VK_SUCCESS) {
        note("vkCreateInstance: the host refused, result", static_cast<uint32_t>(result));
        return result;
    }
    if (*instance != VK_NULL_HANDLE) return VK_SUCCESS;
    /* The host made an instance no handle could be made for: closing its
     * context is what destroys it. */
    release_connection(opened);
    return VK_ERROR_OUT_OF_HOST_MEMORY;
}

VkResult IcdRoot::enumerate_adapter_physical_devices(VkInstance instance, uint64_t luid, uint32_t *count,
                                                     VkPhysicalDevice *physical_devices)
{
    AV_SCOPE("IcdRoot::enumerate_adapter_physical_devices");
    const Connection *const connection = connection_of(instance, LoaderHandle::Kind::instance);
    if (connection == nullptr) {
        log_.write("avvk: vk_icdEnumerateAdapterPhysicalDevices on an instance this driver did not make");
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    if (lost(*connection, "vk_icdEnumerateAdapterPhysicalDevices")) return VK_ERROR_DEVICE_LOST;
    if (luid != connection->luid()) {
        *count = 0u;
        return VK_SUCCESS;
    }
    const VkResult result = vkicd::generated::Wrap_vkEnumeratePhysicalDevices(instance, count, physical_devices);
    if (physical_devices == nullptr || (result != VK_SUCCESS && result != VK_INCOMPLETE)) return result;
    for (uint32_t index = 0u; index < *count; index++) {
        if (physical_devices[index] == VK_NULL_HANDLE) return VK_ERROR_OUT_OF_HOST_MEMORY;
    }
    return result;
}

VkResult IcdRoot::enumerate_device_extensions(VkPhysicalDevice physical_device, const char *layer, uint32_t *count,
                                              VkExtensionProperties *properties)
{
    AV_SCOPE("IcdRoot::enumerate_device_extensions");
    LoaderHandle *const made = handle_of(physical_device, LoaderHandle::Kind::physical_device);
    if (made == nullptr) {
        log_.write("avvk: vkEnumerateDeviceExtensionProperties on a physical device this driver did not make");
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    if (lost(*made->connection(), "vkEnumerateDeviceExtensionProperties")) return VK_ERROR_DEVICE_LOST;
    if (layer != nullptr) {
        log_.write("avvk: device extensions of a layer asked for; this driver is no layer");
        return VK_ERROR_LAYER_NOT_PRESENT;
    }
    ClientTransport &transport = made->connection()->transport();
    uint32_t         listed = 0u;
    VkResult result = vkproto::av_vkEnumerateDeviceExtensionProperties(transport, physical_device, nullptr, &listed,
                                                                        nullptr);
    if (result != VK_SUCCESS) return result;
    if (listed == 0u) return extensions_.report(nullptr, 0u, count, properties);
    if (listed > max_host_extensions) {
        note("vkEnumerateDeviceExtensionProperties: the host lists more device extensions than 1024:", listed);
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }
    auto *const host =
        static_cast<VkExtensionProperties *>(memory_.allocate(listed * sizeof(VkExtensionProperties), Pool::paged));
    if (host == nullptr) return VK_ERROR_OUT_OF_HOST_MEMORY;
    ScopeExit released([&] { memory_.release(host); });
    result = vkproto::av_vkEnumerateDeviceExtensionProperties(transport, physical_device, nullptr, &listed, host);
    if (result != VK_SUCCESS && result != VK_INCOMPLETE) return result;
    /* The one time the host's own list is in hand: whether it shares by
     * dma-buf is decided here and read everywhere else (feature 053, T031). */
    for (uint32_t index = 0u; index < listed; index++)
        if (__builtin_strcmp(host[index].extensionName, dma_buf_extension) == 0)
            made->connection()->note_host_dma_buf();
    return extensions_.report(host, listed, count, properties);
}

/* Whether this host has the dma-buf extension, asked once per connection. A
 * refusal or a host that lists none leaves the answer as it was, which is the
 * opaque fd - what every host shared by before feature 053. */
void IcdRoot::learn_host_sharing(LoaderHandle &made, VkPhysicalDevice physical_device)
{
    AV_SCOPE("IcdRoot::learn_host_sharing");
    if (made.connection()->host_has_dma_buf()) return;
    ClientTransport &transport = made.connection()->transport();
    uint32_t         listed = 0u;
    if (vkproto::av_vkEnumerateDeviceExtensionProperties(transport, physical_device, nullptr, &listed,
                                                          nullptr) != VK_SUCCESS)
        return;
    if (listed == 0u || listed > max_host_extensions) return;
    auto *const host =
        static_cast<VkExtensionProperties *>(memory_.allocate(listed * sizeof(VkExtensionProperties), Pool::paged));
    if (host == nullptr) return;
    ScopeExit      released([&] { memory_.release(host); });
    const VkResult result =
        vkproto::av_vkEnumerateDeviceExtensionProperties(transport, physical_device, nullptr, &listed, host);
    if (result != VK_SUCCESS && result != VK_INCOMPLETE) return;
    for (uint32_t index = 0u; index < listed; index++)
        if (__builtin_strcmp(host[index].extensionName, dma_buf_extension) == 0)
            made.connection()->note_host_dma_buf();
}

VkResult IcdRoot::create_device(VkPhysicalDevice physical_device, const VkDeviceCreateInfo *create,
                                const VkAllocationCallbacks *allocator, VkDevice *device)
{
    AV_SCOPE("IcdRoot::create_device");
    LoaderHandle *const made = handle_of(physical_device, LoaderHandle::Kind::physical_device);
    if (made == nullptr) {
        log_.write("avvk: vkCreateDevice on a physical device this driver did not make");
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    if (lost(*made->connection(), "vkCreateDevice")) return VK_ERROR_DEVICE_LOST;
    const uint32_t refused = extensions_.first_refused(create->ppEnabledExtensionNames, create->enabledExtensionCount);
    if (refused != create->enabledExtensionCount) {
        const char *const name = create->ppEnabledExtensionNames[refused];
        char              line[line_bytes] = {};
        TextBuffer        text(line, sizeof line);
        text.append("avvk: vkCreateDevice names an extension this driver does not report: ");
        text.append(name != nullptr ? name : "(null)");
        log_.write(text.text());
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
    HostExtensionList asked;
    if (asked.take(create->ppEnabledExtensionNames, create->enabledExtensionCount) != Status::ok) {
        note("vkCreateDevice: the extensions it names are not a list this driver can pass on, count",
             create->enabledExtensionCount);
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
    /* Where the host has it, the dma-buf extension is asked for beside the fd
     * one: a shared allocation is exported as a dma-buf so another process's
     * open can import it, and VkExportMemoryAllocateInfo naming that type on a
     * device made without the extension is refused (feature 053). */
    if (names_hold(create->ppEnabledExtensionNames, create->enabledExtensionCount, win32_memory_extension)) {
        /* Asked here rather than waiting for the application to enumerate:
         * what a shared allocation is exported as has to be settled before the
         * device is made, and an application may never ask for the list. */
        learn_host_sharing(*made, physical_device);
        /* Not branched on, because it cannot fail: `take` refused a list longer
         * than the table, and this list names the win32 extension - which take
         * keeps for this ICD rather than passing on - so at most max_names - 1
         * of its names reached the table and there is always room for one
         * more. A branch nothing can take is one no test can judge. */
        if (made->connection()->host_has_dma_buf()) static_cast<void>(asked.add(dma_buf_extension));
    }
    /* A swapchain is this ICD's own, so the host's device is not asked for it. */
    VkDeviceCreateInfo forwarded = *create;
    forwarded.enabledExtensionCount = asked.count();
    forwarded.ppEnabledExtensionNames = asked.names();
    uint64_t       host = 0u;
    const VkResult result =
        vkproto::av_vkCreateDevice(made->connection()->transport(), physical_device, &forwarded, allocator, &host);
    if (result != VK_SUCCESS) return result;
    *device = static_cast<VkDevice>(wrap_child(LoaderHandle::Kind::device, host, *made));
    /* Without a handle the host's device cannot be named to destroy it; it
     * goes when the instance's context closes. */
    return *device != VK_NULL_HANDLE ? VK_SUCCESS : VK_ERROR_OUT_OF_HOST_MEMORY;
}

void IcdRoot::release_device_memory(LoaderHandle &device)
{
    AV_SCOPE("IcdRoot::release_device_memory");
    memory_lock_.acquire();
    ScopeExit           release([&] { memory_lock_.release(); });
    DeviceMemory *const memory = device.device_memory();
    if (memory == nullptr) return;
    const Status released = memory->release_all();
    if (released != Status::ok) note("device memory not released whole, status", static_cast<uint64_t>(released));
}

DescriptorTemplateTable &IcdRoot::templates()
{
    AV_SCOPE("IcdRoot::templates");
    return templates_;
}

/* The generated wrapper makes the template on the host; what it made is
 * remembered here, because the length of every blob a later update carries is
 * this template's and nothing in that call says so (feature 047). */
VkResult IcdRoot::create_descriptor_update_template(VkDevice device,
                                                    const VkDescriptorUpdateTemplateCreateInfo *create,
                                                    const VkAllocationCallbacks *allocator,
                                                    VkDescriptorUpdateTemplate *made)
{
    AV_SCOPE("IcdRoot::create_descriptor_update_template");
    if (create == nullptr || made == nullptr) return VK_ERROR_INITIALIZATION_FAILED;
    /* What the entries say is read before the host is asked: a template whose
     * blob cannot be sized is one no update through it could carry. */
    DescriptorTemplateLayout layout;
    if (layout.take(create->pDescriptorUpdateEntries, create->descriptorUpdateEntryCount) != Status::ok) {
        log_.write("avvk: a descriptor update template names entries this driver cannot size");
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    const VkResult result = vkicd::generated::Wrap_vkCreateDescriptorUpdateTemplate(device, create, allocator, made);
    if (result != VK_SUCCESS) return result;
    const VkResult recorded = templates_.record(device, *made, create);
    if (recorded == VK_SUCCESS) return VK_SUCCESS;
    /* No room to remember it is no template at all: an update through one this
     * table does not know would send a blob of no bytes. */
    vkicd::generated::Wrap_vkDestroyDescriptorUpdateTemplate(device, *made, allocator);
    *made = VK_NULL_HANDLE;
    return recorded;
}

void IcdRoot::destroy_descriptor_update_template(VkDevice device, VkDescriptorUpdateTemplate made,
                                                 const VkAllocationCallbacks *allocator)
{
    AV_SCOPE("IcdRoot::destroy_descriptor_update_template");
    templates_.forget(made);
    vkicd::generated::Wrap_vkDestroyDescriptorUpdateTemplate(device, made, allocator);
}

/* What a shared allocation is exported and imported as. A dma-buf where the
 * host has it, because that is the only kind the *device* can import for
 * another process: vkGetMemoryFdPropertiesKHR refuses an opaque fd, whose
 * memory type is implicit and known only to whoever allocated it, so an opaque
 * fd can be imported only where the importing application names the type
 * itself - which is this process and no other. An opaque fd is still what a
 * host without the extension shares by, and within one process that works;
 * across one, such a host shares nothing (feature 053, T031). */
VkFlags IcdRoot::memory_share_type(const Connection &connection) const
{
    AV_SCOPE("IcdRoot::memory_share_type");
    if (connection.host_has_dma_buf()) return VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT;
    return memory_handles_.host_type();
}

uint64_t IcdRoot::shared_bytes(uint64_t size) const
{
    AV_SCOPE("IcdRoot::shared_bytes");
    constexpr uint64_t page = SharedObjectRecord::page_size;
    if (size == 0u || size > ~uint64_t{0} - (page - 1u)) return 0u;
    return (size + page - 1u) & ~(page - 1u);
}

/* 047's path, unchanged in what it does: a fresh fd of the memory this process
 * holds, imported on the asking device as the host's own structure. The fd is
 * asked for each time, because a driver takes ownership of one it imports. */
VkResult IcdRoot::own_process_allocation(LoaderHandle &made, VkDevice device,
                                         const ExternalObjectSlot &holder, const VkMemoryAllocateInfo &info,
                                         const VkAllocationCallbacks *allocator, VkDeviceMemory &memory)
{
    AV_SCOPE("IcdRoot::own_process_allocation");
    int            fd = -1;
    const VkResult shared = host_memory_fd(made, holder.device_, holder.memory_, fd);
    if (shared != VK_SUCCESS) return shared;
    /* The application's own request, with the import node in the Win32 one's
     * place: its size and above all its memory type index are what the driver
     * is asked for, and an import into another type is not this memory. */
    MemoryAllocateChain chain;
    const Status        built =
        chain.take(info, memory_handles_, memory_share_type(*made.connection()), fd);
    if (built != Status::ok) {
        note("vkAllocateMemory: the chain of an imported allocation is not one this driver can send, status",
             static_cast<uint64_t>(built));
        return VK_ERROR_INVALID_EXTERNAL_HANDLE;
    }
    return vkproto::av_vkAllocateMemory(made.connection()->transport(), device, chain.forwarded(), allocator,
                                         &memory);
}

VkResult IcdRoot::publish(LoaderHandle &made, VkDevice device, uint32_t kind, uint64_t size,
                          VkDeviceMemory memory, VkSemaphore semaphore)
{
    AV_SCOPE("IcdRoot::publish");
    const uint32_t guest_device = made.connection()->device();
    /* One page, and its pages are never read. What is shared is the host
     * object; this exists only because dxgkrnl mints an identity over an
     * allocation and there is no guest allocation of host memory at all. */
    uint8_t carrier_data[AllocationDescriptor::encoded_size + SharedObjectRecord::encoded_size] = {};
    static_cast<void>(AllocationDescriptor(AllocationKind::client, true, SurfaceLayout(),
                                           SharedObjectRecord::page_size, ScanoutTarget())
                          .encode(carrier_data, AllocationDescriptor::encoded_size));
    static_cast<void>(SharedObjectRecord(0u, SharedObjectKind(kind), 0u, size)
                          .encode(carrier_data + AllocationDescriptor::encoded_size,
                                  SharedObjectRecord::encoded_size));
    uint32_t     carrier = 0u;
    const Status created =
        kmt_.create_shared_allocation(guest_device, carrier_data, sizeof carrier_data, carrier);
    if (created != Status::ok) {
        refused("vkAllocateMemory: no allocation to mint a shared object's identity into, NTSTATUS");
        /* What the refusal was about, because the status alone cannot say:
         * a device of 0 is a connection that never opened, and the length is
         * the one thing this request has that an ordinary create does not. */
        note("vkAllocateMemory: the carrier was asked for on guest device", guest_device);
        note("vkAllocateMemory: the carrier's private driver data was bytes", sizeof carrier_data);
        return VK_ERROR_TOO_MANY_OBJECTS;
    }
    ScopeExit unwind([&] { static_cast<void>(kmt_.destroy_shared_allocation(guest_device, carrier)); });
    SharedObjectRecord    minted;
    if (minted.decode(carrier_data + AllocationDescriptor::encoded_size, SharedObjectRecord::encoded_size) !=
            Status::ok ||
        !minted.assigned()) {
        log_.write("avvk: the miniport minted no identity into a shared object's carrier");
        return VK_ERROR_INVALID_EXTERNAL_HANDLE;
    }
    /* Published before the handle is minted: a handle naming an object the
     * device does not hold is one an opening process would follow to nothing. */
    SharedObjectCall     call(made.connection()->transport());
    const uint64_t       wire_handle = memory != VK_NULL_HANDLE ? vkproto::handle_to_wire(memory)
                                                                : vkproto::handle_to_wire(semaphore);
    const Status         published = call.share(minted.object_id(), kind, size, wire_handle);
    if (published != Status::ok) {
        note("vkAllocateMemory: the device would not publish a shared object, status", call.device_status());
        return VK_ERROR_TOO_MANY_OBJECTS;
    }
    uint64_t     handle = 0u;
    const Status shared = shared_handles_.mint(guest_device, carrier, minted.object_id(), handle);
    if (shared != Status::ok) {
        refused("vkAllocateMemory: no NT handle for a shared object, NTSTATUS");
        static_cast<void>(call.release(minted.object_id(), kind, size));
        return VK_ERROR_TOO_MANY_OBJECTS;
    }
    /* Not branched on, because it cannot fail: every row here is one an NT
     * handle was minted for a moment ago, so the handle table fills first and
     * answers no_space before this can. The assertion is what keeps that true
     * - the day the sizes differ it is a compile error rather than a rollback
     * no test could ever reach. */
    static_assert(ExternalObjects::max_objects <= SharedHandles::max_handles,
                  "an externals row always follows a handle row, so the handle table must fill first");
    static_cast<void>(externals_.remember(device, memory, semaphore, minted.object_id(), size, handle,
                                          carrier));
    unwind.dismiss();
    return VK_SUCCESS;
}

uint64_t IcdRoot::object_of(LoaderHandle &made, void *handle)
{
    AV_SCOPE("IcdRoot::object_of");
    const uint64_t named = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(handle));
    uint64_t       object_id = 0u;
    if (shared_handles_.resolve(named, object_id)) return object_id;
    /* Not one of this process's: the kernel is asked whether it may open it,
     * which is the only thing that can tell a handle another process sent from
     * a number an application invented. */
    const Status adopted = shared_handles_.adopt(made.connection()->device(), named, object_id);
    if (adopted == Status::ok) return object_id;
    refused("a Win32 handle this process may not open, NTSTATUS");
    return 0u;
}

void IcdRoot::release_shared(LoaderHandle &made, const ExternalObjectSlot &row)
{
    AV_SCOPE("IcdRoot::release_shared");
    const uint32_t kind = row.memory_ != VK_NULL_HANDLE ? SharedObjectKind::memory
                                                        : SharedObjectKind::monitored_fence;
    const uint64_t size = row.memory_ != VK_NULL_HANDLE ? row.size_ : 0u;
    SharedObjectCall call(made.connection()->transport());
    static_cast<void>(call.release(row.object_id_, kind, size));
    shared_handles_.forget(row.handle_);
    /* Only a carrier this process made: an object it opened was minted into
     * somebody else's allocation, and SharedHandles destroyed the one its own
     * open created. */
    if (row.carrier_ != 0u)
        static_cast<void>(kmt_.destroy_shared_allocation(made.connection()->device(), row.carrier_));
}

/* Nothing is asked of the host or the kernel here. The identity and the NT
 * handle were made when the object was created, because Vulkan promises that a
 * handle to memory an application declared exportable can be got afterwards
 * without failing for want of resources - so the refusal belongs at the
 * allocate, where the application can still act on it (feature 053). */
VkResult IcdRoot::export_memory_handle(VkDevice device, const MemoryGetWin32HandleRecord *request, void **handle)
{
    AV_SCOPE("IcdRoot::export_memory_handle");
    if (allocating_device(device) == nullptr || request == nullptr || handle == nullptr)
        return VK_ERROR_INVALID_EXTERNAL_HANDLE;
    if (foreign_enum(request->type_) != static_cast<int32_t>(memory_get_win32_handle_info) ||
        request->handle_type_ != opaque_win32_memory)
        return VK_ERROR_INVALID_EXTERNAL_HANDLE;
    const uint64_t named = externals_.handle_of_memory(request->memory_);
    if (named == 0u) {
        log_.write("avvk: vkGetMemoryWin32HandleKHR names memory this process never declared exportable");
        return VK_ERROR_INVALID_EXTERNAL_HANDLE;
    }
    *handle = reinterpret_cast<void *>(static_cast<uintptr_t>(named));
    return VK_SUCCESS;
}

VkResult IcdRoot::export_semaphore_handle(VkDevice device, const SemaphoreGetWin32HandleRecord *request,
                                          void **handle)
{
    AV_SCOPE("IcdRoot::export_semaphore_handle");
    if (allocating_device(device) == nullptr || request == nullptr || handle == nullptr)
        return VK_ERROR_INVALID_EXTERNAL_HANDLE;
    if (foreign_enum(request->type_) != static_cast<int32_t>(semaphore_get_win32_handle_info) ||
        request->handle_type_ != opaque_win32_semaphore)
        return VK_ERROR_INVALID_EXTERNAL_HANDLE;
    LoaderHandle *const made = allocating_device(device);
    /* A semaphore is published the first time a handle to it is asked for:
     * unlike memory, nothing in its creation says it will be shared. */
    uint64_t named = externals_.handle_of_semaphore(request->semaphore_);
    if (named == 0u) {
        if (request->semaphore_ == VK_NULL_HANDLE) return VK_ERROR_INVALID_EXTERNAL_HANDLE;
        if (lost(*made->connection(), "vkGetSemaphoreWin32HandleKHR")) return VK_ERROR_DEVICE_LOST;
        const VkResult published =
            publish(*made, device, SharedObjectKind::monitored_fence, 0u, VK_NULL_HANDLE,
                    request->semaphore_);
        if (published != VK_SUCCESS) return published;
        named = externals_.handle_of_semaphore(request->semaphore_);
    }
    *handle = reinterpret_cast<void *>(static_cast<uintptr_t>(named));
    return VK_SUCCESS;
}

void IcdRoot::refused(const char *what)
{
    AV_SCOPE("IcdRoot::refused");
    kernel_refusal_ = kmt_.last_code();
    note(what, kernel_refusal_);
}

VkResult IcdRoot::last_kernel_status(VkPhysicalDevice physical_device, uint32_t *status)
{
    AV_SCOPE("IcdRoot::last_kernel_status");
    if (status == nullptr) return VK_ERROR_INITIALIZATION_FAILED;
    *status = 0u;
    if (connection_of(physical_device, LoaderHandle::Kind::physical_device) == nullptr)
        return VK_ERROR_INITIALIZATION_FAILED;
    *status = kernel_refusal_;
    return VK_SUCCESS;
}

VkResult IcdRoot::memory_handle_properties(VkDevice device, void *)
{
    AV_SCOPE("IcdRoot::memory_handle_properties");
    if (allocating_device(device) == nullptr) return VK_ERROR_INVALID_EXTERNAL_HANDLE;
    /* The query answers for a handle from outside, and a handle this ICD did
     * not export names nothing here (contracts/external_objects.md). */
    log_.write("avvk: vkGetMemoryWin32HandlePropertiesKHR: a handle this driver did not export names nothing");
    return VK_ERROR_INVALID_EXTERNAL_HANDLE;
}

VkResult IcdRoot::host_memory_fd(LoaderHandle &made, VkDevice owner, VkDeviceMemory memory, int &fd)
{
    AV_SCOPE("IcdRoot::host_memory_fd");
    VkMemoryGetFdInfoKHR get = {};
    get.sType = VK_STRUCTURE_TYPE_MEMORY_GET_FD_INFO_KHR;
    get.memory = memory;
    get.handleType =
        static_cast<VkExternalMemoryHandleTypeFlagBits>(memory_share_type(*made.connection()));
    /* A faulted connection needs no check of its own: the call through it
     * answers VK_ERROR_DEVICE_LOST. */
    return vkproto::av_vkGetMemoryFdKHR(made.connection()->transport(), owner, &get, &fd);
}

VkResult IcdRoot::host_semaphore_fd(LoaderHandle &made, VkDevice owner, VkSemaphore semaphore, int &fd)
{
    AV_SCOPE("IcdRoot::host_semaphore_fd");
    VkSemaphoreGetFdInfoKHR get = {};
    get.sType = VK_STRUCTURE_TYPE_SEMAPHORE_GET_FD_INFO_KHR;
    get.semaphore = semaphore;
    get.handleType = static_cast<VkExternalSemaphoreHandleTypeFlagBits>(semaphore_handles_.host_type());
    return vkproto::av_vkGetSemaphoreFdKHR(made.connection()->transport(), owner, &get, &fd);
}

VkResult IcdRoot::import_semaphore_handle(VkDevice device, const ImportSemaphoreWin32HandleRecord *request)
{
    AV_SCOPE("IcdRoot::import_semaphore_handle");
    LoaderHandle *const made = allocating_device(device);
    if (made == nullptr) return VK_ERROR_INVALID_EXTERNAL_HANDLE;
    if (lost(*made->connection(), "vkImportSemaphoreWin32HandleKHR")) return VK_ERROR_DEVICE_LOST;
    if (request == nullptr || request->name_ != nullptr) return VK_ERROR_INVALID_EXTERNAL_HANDLE;
    if (foreign_enum(request->type_) != static_cast<int32_t>(import_semaphore_win32_handle_info) ||
        request->handle_type_ != opaque_win32_semaphore)
        return VK_ERROR_INVALID_EXTERNAL_HANDLE;
    const uint64_t object_id = object_of(*made, request->handle_);
    if (object_id == 0u) {
        log_.write("avvk: vkImportSemaphoreWin32HandleKHR names a handle this process may not open");
        return VK_ERROR_INVALID_EXTERNAL_HANDLE;
    }
    /* Vulkan imports a payload into a semaphore the application already made,
     * and OPEN_SHARED_OBJECT answers a semaphore the *device* made - there is
     * no field on that command for "into this one". So an object this process
     * published is imported the way 047 did it, from a fresh fd of the
     * semaphore this process holds; one published by another process has no
     * path yet and is refused rather than half-served (feature 053, T031). */
    ExternalObjectSlot holder = {};
    VkDevice           owner = VK_NULL_HANDLE;
    VkSemaphore        exported = VK_NULL_HANDLE;
    if (externals_.holder_of(object_id, holder) && holder.semaphore_ != VK_NULL_HANDLE) {
        owner = holder.device_;
        exported = holder.semaphore_;
    }
    if (exported == VK_NULL_HANDLE) {
        log_.write("avvk: vkImportSemaphoreWin32HandleKHR of a semaphore another process published needs "
                   "a device-side import, which the executor ABI has no call for");
        return VK_ERROR_INVALID_EXTERNAL_HANDLE;
    }
    int            fd = -1;
    const VkResult shared = host_semaphore_fd(*made, owner, exported, fd);
    if (shared != VK_SUCCESS) return shared;
    VkImportSemaphoreFdInfoKHR importing = {};
    importing.sType = VK_STRUCTURE_TYPE_IMPORT_SEMAPHORE_FD_INFO_KHR;
    importing.semaphore = request->semaphore_;
    importing.flags = request->flags_;
    importing.handleType = static_cast<VkExternalSemaphoreHandleTypeFlagBits>(semaphore_handles_.host_type());
    importing.fd = fd;
    return vkproto::av_vkImportSemaphoreFdKHR(made->connection()->transport(), device, &importing);
}

void IcdRoot::destroy_semaphore(VkDevice device, VkSemaphore semaphore, const VkAllocationCallbacks *allocator)
{
    AV_SCOPE("IcdRoot::destroy_semaphore");
    LoaderHandle *const made = allocating_device(device);
    /* A device this ICD did not make has no connection to send over, and the
     * generated wrapper follows one without asking. Every other entry point
     * here answers such a device INVALID_EXTERNAL_HANDLE; this one returns
     * nothing, because vkDestroySemaphore has nothing to answer with. */
    if (made == nullptr) return;
    ExternalObjectSlot row = {};
    if (externals_.take_semaphore(semaphore, row)) release_shared(*made, row);
    vkicd::generated::Wrap_vkDestroySemaphore(device, semaphore, allocator);
}

void IcdRoot::external_buffer_properties(VkPhysicalDevice physical_device,
                                         const VkPhysicalDeviceExternalBufferInfo *info,
                                         VkExternalBufferProperties               *properties)
{
    AV_SCOPE("IcdRoot::external_buffer_properties");
    if (info == nullptr || properties == nullptr) return;
    Connection *const connection = connection_of(physical_device, LoaderHandle::Kind::physical_device);
    if (connection == nullptr) return;
    if (!memory_handles_.answers(info->handleType)) {
        /* Not a type this ICD shares by: answered as absent rather than asked
         * of a host that means something else by it. */
        properties->externalMemoryProperties = VkExternalMemoryProperties{};
        return;
    }
    VkPhysicalDeviceExternalBufferInfo asked = *info;
    asked.pNext = nullptr;
    asked.handleType = static_cast<VkExternalMemoryHandleTypeFlagBits>(memory_share_type(*connection));
    VkExternalBufferProperties answered = {};
    answered.sType = VK_STRUCTURE_TYPE_EXTERNAL_BUFFER_PROPERTIES;
    vkproto::av_vkGetPhysicalDeviceExternalBufferProperties(connection->transport(), physical_device, &asked,
                                                             &answered);
    properties->externalMemoryProperties.externalMemoryFeatures =
        answered.externalMemoryProperties.externalMemoryFeatures;
    properties->externalMemoryProperties.exportFromImportedHandleTypes =
        memory_handles_.reported(answered.externalMemoryProperties.exportFromImportedHandleTypes);
    properties->externalMemoryProperties.compatibleHandleTypes =
        memory_handles_.reported(answered.externalMemoryProperties.compatibleHandleTypes);
}

void IcdRoot::external_semaphore_properties(VkPhysicalDevice physical_device,
                                            const VkPhysicalDeviceExternalSemaphoreInfo *info,
                                            VkExternalSemaphoreProperties               *properties)
{
    AV_SCOPE("IcdRoot::external_semaphore_properties");
    if (info == nullptr || properties == nullptr) return;
    Connection *const connection = connection_of(physical_device, LoaderHandle::Kind::physical_device);
    if (connection == nullptr) return;
    if (!semaphore_handles_.answers(info->handleType)) {
        properties->exportFromImportedHandleTypes = 0u;
        properties->compatibleHandleTypes = 0u;
        properties->externalSemaphoreFeatures = 0u;
        return;
    }
    VkPhysicalDeviceExternalSemaphoreInfo asked = *info;
    asked.pNext = nullptr;
    asked.handleType = static_cast<VkExternalSemaphoreHandleTypeFlagBits>(semaphore_handles_.host_type());
    VkExternalSemaphoreProperties answered = {};
    answered.sType = VK_STRUCTURE_TYPE_EXTERNAL_SEMAPHORE_PROPERTIES;
    vkproto::av_vkGetPhysicalDeviceExternalSemaphoreProperties(connection->transport(), physical_device, &asked,
                                                                &answered);
    properties->externalSemaphoreFeatures = answered.externalSemaphoreFeatures;
    properties->exportFromImportedHandleTypes =
        semaphore_handles_.reported(answered.exportFromImportedHandleTypes);
    properties->compatibleHandleTypes = semaphore_handles_.reported(answered.compatibleHandleTypes);
}

bool IcdRoot::shares_memory(const void *chain) const
{
    AV_SCOPE("IcdRoot::shares_memory");
    uint32_t read = 0u;
    for (const auto *node = static_cast<const VkBaseInStructure *>(chain); node != nullptr && read < max_chain;
         node = node->pNext, read++) {
        const VkStructureType type = static_cast<VkStructureType>(foreign_enum(node->sType));
        if (type == import_memory_win32_handle_info || type == export_memory_win32_handle_info) return true;
        if (type != VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO) continue;
        const auto *const asked = reinterpret_cast<const VkExportMemoryAllocateInfo *>(node);
        if (memory_handles_.answers(asked->handleTypes)) return true;
    }
    return false;
}

const ImportMemoryWin32HandleRecord *IcdRoot::imported_memory_record(const void *chain) const
{
    AV_SCOPE("IcdRoot::imported_memory_record");
    uint32_t read = 0u;
    for (const auto *node = static_cast<const VkBaseInStructure *>(chain); node != nullptr && read < max_chain;
         node = node->pNext, read++)
        if (static_cast<VkStructureType>(foreign_enum(node->sType)) == import_memory_win32_handle_info)
            return reinterpret_cast<const ImportMemoryWin32HandleRecord *>(node);
    return nullptr;
}

VkResult IcdRoot::shared_allocation(LoaderHandle &made, VkDevice device, const VkMemoryAllocateInfo &info,
                                    const VkAllocationCallbacks *allocator, VkDeviceMemory &memory)
{
    AV_SCOPE("IcdRoot::shared_allocation");
    const uint64_t size = shared_bytes(info.allocationSize);
    if (size == 0u) return VK_ERROR_INVALID_EXTERNAL_HANDLE;
    const ImportMemoryWin32HandleRecord *const importing = imported_memory_record(info.pNext);
    if (importing != nullptr)
        return imported_allocation(made, device, *importing, info, allocator, size, memory);
    MemoryAllocateChain chain;
    const Status        built =
        chain.take(info, memory_handles_, memory_share_type(*made.connection()), -1);
    if (built != Status::ok) {
        note("vkAllocateMemory: the chain of a shared allocation is not one this driver can send, status",
             static_cast<uint64_t>(built));
        return VK_ERROR_INVALID_EXTERNAL_HANDLE;
    }
    const VkResult allocated = vkproto::av_vkAllocateMemory(made.connection()->transport(), device,
                                                             chain.forwarded(), allocator, &memory);
    if (allocated != VK_SUCCESS) return allocated;
    /* Declared exportable, so the identity is minted now: Vulkan promises the
     * handle can be got later without failing for want of resources, and a
     * memory nobody shares is never given one (feature 053, T031). */
    const VkResult published = publish(made, device, SharedObjectKind::memory, size, memory, VK_NULL_HANDLE);
    if (published == VK_SUCCESS) return VK_SUCCESS;
    vkproto::av_vkFreeMemory(made.connection()->transport(), device, memory, allocator);
    memory = VK_NULL_HANDLE;
    return published;
}

/* Memory a Win32 handle names, imported into another device of this process or
 * opened from another process entirely (feature 053, T031).
 *
 * The two are different paths on purpose. Every device of one process shares
 * one guest context, and a context that publishes an object may not open it
 * again - a second handle for one object in one context is one the guest could
 * free twice. So an object this process published is imported the way 047 did
 * it, from a fresh fd of the memory this process holds, which gives the second
 * VkDevice a VkDeviceMemory of its own. An object another process published
 * has no such memory here, and is opened on the device. */
VkResult IcdRoot::imported_allocation(LoaderHandle &made, VkDevice device,
                                      const ImportMemoryWin32HandleRecord &importing,
                                      const VkMemoryAllocateInfo &info,
                                      const VkAllocationCallbacks *allocator, uint64_t size,
                                      VkDeviceMemory &memory)
{
    AV_SCOPE("IcdRoot::imported_allocation");
    if (importing.name_ != nullptr) return VK_ERROR_INVALID_EXTERNAL_HANDLE;
    if (importing.handle_type_ != opaque_win32_memory) return VK_ERROR_INVALID_EXTERNAL_HANDLE;
    const uint64_t object_id = object_of(made, importing.handle_);
    if (object_id == 0u) {
        log_.write("avvk: vkAllocateMemory names a Win32 handle this process may not open");
        return VK_ERROR_INVALID_EXTERNAL_HANDLE;
    }
    ExternalObjectSlot holder = {};
    if (externals_.holder_of(object_id, holder) && holder.memory_ != VK_NULL_HANDLE)
        return own_process_allocation(made, device, holder, info, allocator, memory);
    SharedObjectCall call(made.connection()->transport());
    uint64_t         opened = 0u;
    const Status     answered = call.open(object_id, SharedObjectKind::memory, size, opened);
    if (answered != Status::ok) {
        note("vkAllocateMemory: the device would not open a shared object, status", call.device_status());
        return VK_ERROR_INVALID_EXTERNAL_HANDLE;
    }
    /* Not branched on, because it cannot fail: SharedObjectCall::judge has
     * already refused an open that answered no handle, and the conversion of a
     * non-zero one is a copy of the same width. A branch nothing can take is
     * one no test can judge. */
    static_cast<void>(vkproto::wire_to_handle(opened, memory));
    /* As in publish, and not branched on for the same reason: the handle was
     * adopted before this, so the handle table is the one that runs out. */
    static_cast<void>(externals_.remember(device, memory, VK_NULL_HANDLE, object_id, size,
                                          static_cast<uint64_t>(reinterpret_cast<uintptr_t>(
                                              importing.handle_)),
                                          0u));
    return VK_SUCCESS;
}

void IcdRoot::destroy_device(VkDevice device, const VkAllocationCallbacks *allocator)
{
    AV_SCOPE("IcdRoot::destroy_device");
    templates_.forget_device(device);
    LoaderHandle *const made = handle_of(device, LoaderHandle::Kind::device);
    if (made == nullptr) return;
    /* A process that exits holding everything leaves nothing: each object's
     * reference on the device, its NT handle and its carrier are given back
     * before the device that owns them goes (feature 053, T033). */
    ExternalObjectSlot row = {};
    while (externals_.take_any_of_device(device, row)) release_shared(*made, row);
    /* A swapchain frees its memory through the device: before the device's own. */
    presentation_.forget_device(device);
    release_device_memory(*made);
    release_idle_fences(*made);
    note("queue idle waits answered without the host", idle_skipped_);
    note("queue idle waits sent to the host", idle_waited_);
    vkicd::generated::Wrap_vkDestroyDevice(device, allocator);
    destroy_wrapper(device, LoaderHandle::Kind::device);
}

/* Each queue's vkQueueWaitIdle fence, before the device that owns them. */
void IcdRoot::release_idle_fences(LoaderHandle &device)
{
    AV_SCOPE("IcdRoot::release_idle_fences");
    for (uint32_t index = 0u;; index++) {
        LoaderHandle *const queue = child_of(device, LoaderHandle::Kind::queue, index);
        if (queue == nullptr) return;
        if (queue->idle_fence() == 0u) continue;
        vkproto::av_vkDestroyFence(device.connection()->transport(), static_cast<VkDevice>(static_cast<void *>(&device)),
                                    HandleBits(queue->idle_fence()).as<VkFence>(), nullptr);
        queue->keep_idle_fence(0u);
    }
}

void IcdRoot::destroy_instance(VkInstance instance, const VkAllocationCallbacks *allocator)
{
    AV_SCOPE("IcdRoot::destroy_instance");
    LoaderHandle *const made = handle_of(instance, LoaderHandle::Kind::instance);
    if (made == nullptr) return;
    for (uint32_t physical = 0u;; physical++) {
        LoaderHandle *const physical_device = child_of(*made, LoaderHandle::Kind::physical_device, physical);
        if (physical_device == nullptr) break;
        for (uint32_t index = 0u;; index++) {
            LoaderHandle *const device = child_of(*physical_device, LoaderHandle::Kind::device, index);
            if (device == nullptr) break;
            presentation_.forget_device(reinterpret_cast<VkDevice>(device));
            release_device_memory(*device);
            release_idle_fences(*device);
        }
    }
    presentation_.forget_instance(instance);
    vkicd::generated::Wrap_vkDestroyInstance(instance, allocator);
}

void IcdRoot::learn_types(Connection &connection, VkPhysicalDevice physical_device, MemoryTypeTable &types,
                          const char *entry)
{
    AV_SCOPE("IcdRoot::learn_types");
    if (lost(connection, entry)) return;
    VkPhysicalDeviceMemoryProperties host = {};
    vkproto::av_vkGetPhysicalDeviceMemoryProperties(connection.transport(), physical_device, &host);
    if (lost(connection, entry)) return;
    const Status built = types.build(host);
    if (built != Status::ok) note("the host's memory types were not taken, status", static_cast<uint64_t>(built));
}

void IcdRoot::physical_device_memory_properties(VkPhysicalDevice physical_device,
                                                VkPhysicalDeviceMemoryProperties *properties)
{
    AV_SCOPE("IcdRoot::physical_device_memory_properties");
    Connection *const connection = connection_of(physical_device, LoaderHandle::Kind::physical_device);
    if (connection == nullptr) return;
    MemoryTypeTable types(kmt_, connection->adapter());
    learn_types(*connection, physical_device, types, "vkGetPhysicalDeviceMemoryProperties");
    types.properties(*properties);
}

void IcdRoot::physical_device_memory_properties2(VkPhysicalDevice physical_device,
                                                 VkPhysicalDeviceMemoryProperties2 *properties)
{
    AV_SCOPE("IcdRoot::physical_device_memory_properties2");
    Connection *const connection = connection_of(physical_device, LoaderHandle::Kind::physical_device);
    if (connection == nullptr) return;
    MemoryTypeTable types(kmt_, connection->adapter());
    learn_types(*connection, physical_device, types, "vkGetPhysicalDeviceMemoryProperties2");
    types.properties(properties->memoryProperties);
    auto *node = static_cast<VkBaseOutStructure *>(properties->pNext);
    for (uint32_t seen = 1u; node != nullptr && seen < max_chain; seen++, node = node->pNext)
        if (node->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT)
            types.budget(*reinterpret_cast<VkPhysicalDeviceMemoryBudgetPropertiesEXT *>(node));
}

LoaderHandle *IcdRoot::allocating_device(VkDevice device)
{
    AV_SCOPE("IcdRoot::allocating_device");
    LoaderHandle *const made = handle_of(device, LoaderHandle::Kind::device);
    if (made != nullptr && made->parent()->kind() == LoaderHandle::Kind::physical_device) return made;
    log_.write("avvk: memory of a device this driver did not make through a physical device");
    return nullptr;
}

DeviceMemory *IcdRoot::memory_of(LoaderHandle &device)
{
    AV_SCOPE("IcdRoot::memory_of");
    lock_.acquire();
    ScopeExit release([&] { lock_.release(); });
    if (device.device_memory() != nullptr) return device.device_memory();
    DeviceMemory *made = nullptr;
    if (memories_.create(made, memory_, kmt_, *device.connection(), log_) != Status::ok) {
        log_.write("avvk: no memory to keep a device's allocations in");
        return nullptr;
    }
    device.allocate_from(memories_, *made);
    return made;
}

VkResult IcdRoot::allocate_memory(VkDevice device, const VkMemoryAllocateInfo *info,
                                  const VkAllocationCallbacks *allocator, VkDeviceMemory *memory)
{
    AV_SCOPE("IcdRoot::allocate_memory");
    LoaderHandle *const made = allocating_device(device);
    if (made == nullptr) return VK_ERROR_DEVICE_LOST;
    if (lost(*made->connection(), "vkAllocateMemory")) return VK_ERROR_DEVICE_LOST;
    memory_lock_.acquire();
    ScopeExit           release([&] { memory_lock_.release(); });
    /* An allocation that shares memory is the host's whole and never a chunk
     * of this process's RAM: its chain is rebuilt over the host's own fd
     * mechanism (feature 047, US3). */
    if (shares_memory(info->pNext)) return shared_allocation(*made, device, *info, allocator, *memory);
    DeviceMemory *const owned = memory_of(*made);
    if (owned == nullptr) return VK_ERROR_OUT_OF_HOST_MEMORY;
    const auto physical = static_cast<VkPhysicalDevice>(static_cast<void *>(made->parent()));
    return owned->allocate(physical, device, *info, allocator, *memory);
}

void IcdRoot::free_memory(VkDevice device, VkDeviceMemory memory, const VkAllocationCallbacks *allocator)
{
    AV_SCOPE("IcdRoot::free_memory");
    LoaderHandle *const made = allocating_device(device);
    if (made == nullptr) return;
    memory_lock_.acquire();
    ScopeExit          release([&] { memory_lock_.release(); });
    ExternalObjectSlot row = {};
    if (externals_.take_memory(memory, row)) {
        release_shared(*made, row);
        /* Memory this process opened was made by the device inside the open
         * and destroyed by the release; only memory it allocated itself is
         * still the host's to free. */
        if (row.carrier_ == 0u) return;
    }
    DeviceMemory *const owned = memory_of(*made);
    if (owned != nullptr) owned->free(device, memory, allocator);
}

VkResult IcdRoot::map_memory(VkDevice device, VkDeviceMemory memory, VkDeviceSize offset, VkDeviceSize size,
                             VkMemoryMapFlags flags, void **data)
{
    AV_SCOPE("IcdRoot::map_memory");
    LoaderHandle *const made = allocating_device(device);
    if (made == nullptr) return VK_ERROR_MEMORY_MAP_FAILED;
    if (lost(*made->connection(), "vkMapMemory")) return VK_ERROR_DEVICE_LOST;
    memory_lock_.acquire();
    ScopeExit                 release([&] { memory_lock_.release(); });
    const DeviceMemory *const owned = made->device_memory();
    if (owned == nullptr) return VK_ERROR_MEMORY_MAP_FAILED;
    return owned->map(memory, offset, size, flags, *data);
}

/* The VkMemoryMapInfoKHR shape of the same mapping. Placement is refused, not
 * ignored: the chunk's view is at the address VidMm chose, so a caller that
 * asked for one of its own must be told no (contracts/placed_mappings.md). */
VkResult IcdRoot::map_memory2(VkDevice device, const VkMemoryMapInfoKHR *info, void **data)
{
    AV_SCOPE("IcdRoot::map_memory2");
    if (info == nullptr || data == nullptr) return VK_ERROR_MEMORY_MAP_FAILED;
    if (info->pNext != nullptr) {
        log_.write("avvk: vkMapMemory2 names something this driver cannot place");
        return VK_ERROR_MEMORY_MAP_FAILED;
    }
    return map_memory(device, info->memory, info->offset, info->size, info->flags, data);
}

VkResult IcdRoot::mapped_ranges(VkDevice device, const char *entry)
{
    AV_SCOPE("IcdRoot::mapped_ranges");
    const Connection *const connection = connection_of(device, LoaderHandle::Kind::device);
    if (connection == nullptr) {
        log_.write("avvk: mapped ranges of a device this driver did not make");
        return VK_ERROR_DEVICE_LOST;
    }
    return lost(*connection, entry) ? VK_ERROR_DEVICE_LOST : VK_SUCCESS;
}

void IcdRoot::physical_device_properties(VkPhysicalDevice physical_device, VkPhysicalDeviceProperties *properties)
{
    AV_SCOPE("IcdRoot::physical_device_properties");
    Connection *const connection = connection_of(physical_device, LoaderHandle::Kind::physical_device);
    if (connection == nullptr) return;
    PhysicalDeviceView view(connection->transport(), connection->luid(), api_version_, log_);
    view.properties(physical_device, properties);
}

void IcdRoot::physical_device_properties2(VkPhysicalDevice physical_device, VkPhysicalDeviceProperties2 *properties)
{
    AV_SCOPE("IcdRoot::physical_device_properties2");
    Connection *const connection = connection_of(physical_device, LoaderHandle::Kind::physical_device);
    if (connection == nullptr) return;
    PhysicalDeviceView view(connection->transport(), connection->luid(), api_version_, log_);
    view.properties2(physical_device, properties);
}

Connection *IcdRoot::acquire_connection()
{
    AV_SCOPE("IcdRoot::acquire_connection");
    Connection *const adopted = pending_;
    pending_ = nullptr;
    return adopted;
}

void IcdRoot::release_connection(Connection *connection)
{
    AV_SCOPE("IcdRoot::release_connection");
    lock_.acquire();
    ScopeExit release([&] { lock_.release(); });
    if (connections_.destroy(connection) != Status::ok) log_.write("avvk: release of a connection never opened");
}

void *IcdRoot::wrap(LoaderHandle::Kind kind, uint64_t host, Connection *connection)
{
    AV_SCOPE("IcdRoot::wrap");
    lock_.acquire();
    ScopeExit           release([&] { lock_.release(); });
    LoaderHandle *const parent = kind == LoaderHandle::Kind::instance ? nullptr : handles_.instance_of(connection);
    return wrap_under(kind, host, connection, parent);
}

/* Under the root's lock. A command buffer is not a handle without its batch:
 * every vkCmd* on it records there. */
LoaderHandle *IcdRoot::wrap_under(LoaderHandle::Kind kind, uint64_t host, Connection *connection,
                                  LoaderHandle *parent)
{
    AV_SCOPE("IcdRoot::wrap_under");
    LoaderHandle *handle = nullptr;
    const Status  status = handles_.wrap(kind, host, connection, parent, handle);
    if (status != Status::ok) {
        note("no handle for host object, status", static_cast<uint64_t>(status));
        return nullptr;
    }
    if (kind != LoaderHandle::Kind::command_buffer || handle->batch() != nullptr) return handle;
    CommandBatch *batch = nullptr;
    if (batches_.create(batch, memory_, *connection) != Status::ok) {
        note("no batch for host command buffer", host);
        /* The handle was made just now, so the registry cannot refuse it. */
        static_cast<void>(handles_.destroy(handle));
        return nullptr;
    }
    handle->record_into(batches_, *batch);
    return handle;
}

/* The registry refuses a parent destroyed since it was looked up. */
void *IcdRoot::wrap_child(LoaderHandle::Kind kind, uint64_t host, LoaderHandle &parent)
{
    AV_SCOPE("IcdRoot::wrap_child");
    lock_.acquire();
    ScopeExit release([&] { lock_.release(); });
    return wrap_under(kind, host, parent.connection(), &parent);
}

LoaderHandle *IcdRoot::handle_of(void *handle, LoaderHandle::Kind kind)
{
    AV_SCOPE("IcdRoot::handle_of");
    lock_.acquire();
    ScopeExit release([&] { lock_.release(); });
    return handles_.made(handle, kind);
}

LoaderHandle *IcdRoot::child_of(const LoaderHandle &parent, LoaderHandle::Kind kind, uint32_t index)
{
    AV_SCOPE("IcdRoot::child_of");
    lock_.acquire();
    ScopeExit release([&] { lock_.release(); });
    return handles_.child(parent, kind, index);
}

Connection *IcdRoot::connection_of(void *handle, LoaderHandle::Kind kind)
{
    AV_SCOPE("IcdRoot::connection_of");
    lock_.acquire();
    ScopeExit                 release([&] { lock_.release(); });
    const LoaderHandle *const made = handles_.made(handle, kind);
    return made != nullptr ? made->connection() : nullptr;
}

uint64_t IcdRoot::wire(void *handle, LoaderHandle::Kind kind)
{
    AV_SCOPE("IcdRoot::wire");
    lock_.acquire();
    ScopeExit                 release([&] { lock_.release(); });
    const LoaderHandle *const made = handles_.made(handle, kind);
    return made != nullptr ? made->host() : 0u;
}

void IcdRoot::destroy_wrapper(void *handle, LoaderHandle::Kind kind)
{
    AV_SCOPE("IcdRoot::destroy_wrapper");
    lock_.acquire();
    ScopeExit release([&] { lock_.release(); });
    if (handles_.destroy(handles_.made(handle, kind)) != Status::ok)
        log_.write("avvk: destroy of a handle this driver did not make");
}

CommandBatch &IcdRoot::record_target(VkCommandBuffer command_buffer)
{
    AV_SCOPE("IcdRoot::record_target");
    lock_.acquire();
    ScopeExit release([&] { lock_.release(); });
    return *handles_.made(command_buffer, LoaderHandle::Kind::command_buffer)->batch();
}

void IcdRoot::device_queue(VkDevice device, uint32_t family, uint32_t index, VkQueue *queue)
{
    AV_SCOPE("IcdRoot::device_queue");
    LoaderHandle *const made = handle_of(device, LoaderHandle::Kind::device);
    if (made == nullptr) return;
    uint64_t host = 0u;
    vkproto::av_vkGetDeviceQueue(made->connection()->transport(), device, family, index, &host);
    *queue = static_cast<VkQueue>(wrap_child(LoaderHandle::Kind::queue, host, *made));
}

void IcdRoot::device_queue2(VkDevice device, const VkDeviceQueueInfo2 *info, VkQueue *queue)
{
    AV_SCOPE("IcdRoot::device_queue2");
    LoaderHandle *const made = handle_of(device, LoaderHandle::Kind::device);
    if (made == nullptr) return;
    uint64_t host = 0u;
    vkproto::av_vkGetDeviceQueue2(made->connection()->transport(), device, info, &host);
    *queue = static_cast<VkQueue>(wrap_child(LoaderHandle::Kind::queue, host, *made));
}

VkResult IcdRoot::allocate_command_buffers(VkDevice device, const VkCommandBufferAllocateInfo *info,
                                           VkCommandBuffer *command_buffers)
{
    AV_SCOPE("IcdRoot::allocate_command_buffers");
    LoaderHandle *const made = handle_of(device, LoaderHandle::Kind::device);
    if (made == nullptr) {
        log_.write("avvk: vkAllocateCommandBuffers on a device this driver did not make");
        return VK_ERROR_DEVICE_LOST;
    }
    if (lost(*made->connection(), "vkAllocateCommandBuffers")) return VK_ERROR_DEVICE_LOST;
    const uint32_t count = info->commandBufferCount;
    if (count > max_allocated_at_once) {
        note("vkAllocateCommandBuffers: more command buffers at once than", max_allocated_at_once);
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }
    uint64_t       hosts[max_allocated_at_once] = {};
    const VkResult result = vkproto::av_vkAllocateCommandBuffers(made->connection()->transport(), device, info, hosts);
    if (result != VK_SUCCESS) return result;
    for (uint32_t wrapped = 0u; wrapped < count; wrapped++) {
        command_buffers[wrapped] =
            static_cast<VkCommandBuffer>(wrap_child(LoaderHandle::Kind::command_buffer, hosts[wrapped], *made));
        if (command_buffers[wrapped] != VK_NULL_HANDLE) continue;
        /* The ones already handed out go back; the host's others stay in their
         * pool until it is reset or destroyed, with no handle to name them by. */
        free_command_buffers(device, info->commandPool, wrapped, command_buffers);
        note("vkAllocateCommandBuffers: host command buffers left in their pool:", count - wrapped);
        for (uint32_t cleared = 0u; cleared < count; cleared++) command_buffers[cleared] = VK_NULL_HANDLE;
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }
    return VK_SUCCESS;
}

void IcdRoot::free_command_buffers(VkDevice device, VkCommandPool pool, uint32_t count,
                                   const VkCommandBuffer *command_buffers)
{
    AV_SCOPE("IcdRoot::free_command_buffers");
    Connection *const connection = connection_of(device, LoaderHandle::Kind::device);
    if (connection == nullptr) return;
    vkproto::av_vkFreeCommandBuffers(connection->transport(), device, pool, count, command_buffers);
    for (uint32_t index = 0u; index < count; index++)
        if (command_buffers[index] != VK_NULL_HANDLE)
            destroy_wrapper(command_buffers[index], LoaderHandle::Kind::command_buffer);
}

VkResult IcdRoot::begin_command_buffer(VkCommandBuffer command_buffer, const VkCommandBufferBeginInfo *info)
{
    AV_SCOPE("IcdRoot::begin_command_buffer");
    if (lost(*connection_of(command_buffer, LoaderHandle::Kind::command_buffer), "vkBeginCommandBuffer"))
        return VK_ERROR_DEVICE_LOST;
    CommandBatch &batch = record_target(command_buffer);
    batch.discard();
    if (vkproto::av_vkBeginCommandBuffer(batch, command_buffer, info) == VK_SUCCESS) return VK_SUCCESS;
    log_.write("avvk: vkBeginCommandBuffer: no memory to record into");
    return VK_ERROR_OUT_OF_HOST_MEMORY;
}

/* A command that could not be recorded, End's own included, is latched in the
 * batch, so what end() answers is the whole recording's answer. */
VkResult IcdRoot::end_command_buffer(VkCommandBuffer command_buffer)
{
    AV_SCOPE("IcdRoot::end_command_buffer");
    CommandBatch &batch = record_target(command_buffer);
    if (lost(*connection_of(command_buffer, LoaderHandle::Kind::command_buffer), "vkEndCommandBuffer")) {
        batch.discard();
        return VK_ERROR_DEVICE_LOST;
    }
    const VkResult recorded = vkproto::av_vkEndCommandBuffer(batch, command_buffer);
    const Status   shipped = batch.end();
    if (shipped == Status::ok) return recorded;
    note("vkEndCommandBuffer: batch not shipped, status", static_cast<uint64_t>(shipped));
    if (batch.refused() != 0u) note("vkEndCommandBuffer: a command larger than its room, id", batch.refused());
    return shipped == Status::no_memory ? VK_ERROR_OUT_OF_HOST_MEMORY : VK_ERROR_DEVICE_LOST;
}

VkResult IcdRoot::reset_command_buffer(VkCommandBuffer command_buffer, VkCommandBufferResetFlags flags)
{
    AV_SCOPE("IcdRoot::reset_command_buffer");
    record_target(command_buffer).discard();
    return vkicd::generated::Wrap_vkResetCommandBuffer(command_buffer, flags);
}

VkResult IcdRoot::wait_for_fences(VkDevice device, uint32_t count, const VkFence *fences, VkBool32 wait_all,
                                  uint64_t timeout)
{
    AV_SCOPE("IcdRoot::wait_for_fences");
    LoaderHandle *const made = handle_of(device, LoaderHandle::Kind::device);
    if (made == nullptr) {
        log_.write("avvk: vkWaitForFences on a device this driver did not make");
        return VK_ERROR_DEVICE_LOST;
    }
    if (lost(*made->connection(), "vkWaitForFences")) return VK_ERROR_DEVICE_LOST;
    uint64_t       frequency = 0u;
    const uint64_t start = kmt_.performance_counter(frequency);
    SlicedWait     wait(timeout, start, frequency);
    uint64_t       slice = 0u;
    while (wait.next(kmt_.performance_counter(frequency), slice)) {
        const VkResult result = vkproto::av_vkWaitForFences(made->connection()->transport(), device, count, fences,
                                                             wait_all, slice);
        if (result != VK_TIMEOUT) return result;
    }
    return VK_TIMEOUT;
}

VkResult IcdRoot::wait_semaphores(VkDevice device, const VkSemaphoreWaitInfo *info, uint64_t timeout)
{
    AV_SCOPE("IcdRoot::wait_semaphores");
    LoaderHandle *const made = handle_of(device, LoaderHandle::Kind::device);
    if (made == nullptr) {
        log_.write("avvk: vkWaitSemaphores on a device this driver did not make");
        return VK_ERROR_DEVICE_LOST;
    }
    if (lost(*made->connection(), "vkWaitSemaphores")) return VK_ERROR_DEVICE_LOST;
    uint64_t       frequency = 0u;
    const uint64_t start = kmt_.performance_counter(frequency);
    SlicedWait     wait(timeout, start, frequency);
    uint64_t       slice = 0u;
    while (wait.next(kmt_.performance_counter(frequency), slice)) {
        const VkResult result = vkproto::av_vkWaitSemaphores(made->connection()->transport(), device, info, slice);
        if (result != VK_TIMEOUT) return result;
    }
    return VK_TIMEOUT;
}

/* The host's own vkQueueWaitIdle has no timeout, so it could hold the context's
 * worker past TdrDelay; a fence on an empty submission can be waited for in
 * slices instead. The queue keeps that fence: making one per wait was a round
 * trip of its own, several times a frame in an application that waits for its
 * queue between passes. */
VkResult IcdRoot::queue_wait_idle(VkQueue queue)
{
    AV_SCOPE("IcdRoot::queue_wait_idle");
    LoaderHandle *const made = handle_of(queue, LoaderHandle::Kind::queue);
    if (made == nullptr || made->parent()->kind() != LoaderHandle::Kind::device) {
        log_.write("avvk: vkQueueWaitIdle on a queue this driver did not make through a device");
        return VK_ERROR_DEVICE_LOST;
    }
    if (lost(*made->connection(), "vkQueueWaitIdle")) return VK_ERROR_DEVICE_LOST;
    /* Nothing sent on the connection since the queue was last found idle -
     * no submit, no present, nothing - leaves it idle: a device-wide wait
     * right after a queue's own was a whole submission and wait for nothing. */
    Connection &connection = *made->connection();
    if (made->idle_at() == connection.submissions()) {
        idle_skipped_++;
        return VK_SUCCESS;
    }
    idle_waited_++;
    const auto        device = static_cast<VkDevice>(static_cast<void *>(made->parent()));
    ClientTransport  &transport = connection.transport();
    VkFence fence = HandleBits(made->idle_fence()).as<VkFence>();
    if (fence == VK_NULL_HANDLE) {
        VkFenceCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        const VkResult created = vkproto::av_vkCreateFence(transport, device, &info, nullptr, &fence);
        if (created != VK_SUCCESS) return created;
        made->keep_idle_fence(HandleBits(fence).value());
    } else {
        const VkResult reset = vkproto::av_vkResetFences(transport, device, 1u, &fence);
        if (reset != VK_SUCCESS) return reset;
    }
    const VkResult submitted = vkproto::av_vkQueueSubmit(transport, queue, 0u, nullptr, fence);
    if (submitted != VK_SUCCESS) return submitted;
    const VkResult waited = wait_for_fences(device, 1u, &fence, VK_TRUE, SlicedWait::forever);
    if (waited == VK_SUCCESS) made->mark_idle(connection.submissions());
    return waited;
}

VkResult IcdRoot::signal_acquire(VkDevice device, VkSemaphore semaphore, VkFence fence)
{
    AV_SCOPE("IcdRoot::signal_acquire");
    if (semaphore == VK_NULL_HANDLE && fence == VK_NULL_HANDLE) return VK_SUCCESS;
    LoaderHandle *const made = handle_of(device, LoaderHandle::Kind::device);
    if (made == nullptr) return VK_ERROR_DEVICE_LOST;
    if (lost(*made->connection(), "vkAcquireNextImageKHR")) return VK_ERROR_DEVICE_LOST;
    LoaderHandle *const queue = child_of(*made, LoaderHandle::Kind::queue, 0u);
    if (queue == nullptr) {
        log_.write("avvk: vkAcquireNextImageKHR has nothing to signal on: the device handed out no queue");
        return VK_ERROR_DEVICE_LOST;
    }
    VkSubmitInfo submit = {};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.signalSemaphoreCount = semaphore != VK_NULL_HANDLE ? 1u : 0u;
    submit.pSignalSemaphores = &semaphore;
    return vkproto::av_vkQueueSubmit(made->connection()->transport(), static_cast<VkQueue>(static_cast<void *>(queue)),
                                      1u, &submit, fence);
}

/* An application may submit only through a queue it was handed, so the queues
 * this ICD handed out are every queue with work on it. */
VkResult IcdRoot::device_wait_idle(VkDevice device)
{
    AV_SCOPE("IcdRoot::device_wait_idle");
    LoaderHandle *const made = handle_of(device, LoaderHandle::Kind::device);
    if (made == nullptr) {
        log_.write("avvk: vkDeviceWaitIdle on a device this driver did not make");
        return VK_ERROR_DEVICE_LOST;
    }
    if (lost(*made->connection(), "vkDeviceWaitIdle")) return VK_ERROR_DEVICE_LOST;
    for (uint32_t index = 0u;; index++) {
        LoaderHandle *const queue = child_of(*made, LoaderHandle::Kind::queue, index);
        if (queue == nullptr) return VK_SUCCESS;
        const VkResult idle = queue_wait_idle(static_cast<VkQueue>(static_cast<void *>(queue)));
        if (idle != VK_SUCCESS) return idle;
    }
}

VkResult IcdRoot::force_context_fault(VkPhysicalDevice physical_device)
{
    AV_SCOPE("IcdRoot::force_context_fault");
    LoaderHandle *const made = handle_of(physical_device, LoaderHandle::Kind::physical_device);
    if (made == nullptr) {
        log_.write("avvk: vkAvForceContextFault on a physical device this driver did not make");
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    Connection &connection = *made->connection();
    if (lost(connection, "vkAvForceContextFault")) return VK_ERROR_DEVICE_LOST;
    if (ContextFault(connection.transport()).send() != Status::ok) {
        log_.write("avvk: the host answered a truncated vkCreateInstance");
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    return VK_ERROR_DEVICE_LOST;
}

uint32_t IcdRoot::handles_on_connections() const
{
    AV_SCOPE("IcdRoot::handles_on_connections");
    uint32_t counted = 0u;
    static_cast<void>(connections_.all_of([&](const Connection &connection) {
        counted += handles_.live_on(&connection);
        return true;
    }));
    return counted;
}

void IcdRoot::report(StateWriter &writer)
{
    AV_SCOPE("IcdRoot::report");
    writer.open("root", 0u);
    if (!lock_.try_acquire()) {
        writer.flag("busy", true);
        writer.close();
        return;
    }
    ScopeExit release([&] { lock_.release(); });
    writer.flag("busy", false);
    writer.number("connections", connections_.live());
    writer.number("handles", handles_.live());
    writer.number("batches", handles_.batches());
    writer.number("memories", handles_.memories());
    writer.close();
    uint32_t number = 0u;
    static_cast<void>(connections_.all_of([&](const Connection &connection) {
        writer.open("connection", number++);
        connection.report(writer);
        handles_.report(writer, &connection);
        writer.close();
        return true;
    }));
}

bool IcdRoot::invariants_hold() const
{
    AV_SCOPE("IcdRoot::invariants_hold");
    /* The entry point tables are constant: a row named twice is a build
     * mistake the root's tests see as a wrong answer, not a state it reaches. */
    if (pending_ != nullptr || !handles_.invariants_hold() || handles_on_connections() != handles_.live()) return false;
    if (!batches_.invariants_hold() || batches_.live() != handles_.batches()) return false;
    if (!batches_.all_of([](const CommandBatch &batch) { return batch.invariants_hold(); })) return false;
    if (!memories_.invariants_hold() || memories_.live() != handles_.memories()) return false;
    if (!memories_.all_of([](const DeviceMemory &memory) { return memory.invariants_hold(); })) return false;
    return connections_.all_of([](const Connection &connection) {
        return connection.opened() && connection.invariants_hold();
    });
}

PFN_vkVoidFunction IcdRoot::entry(const char *name) const
{
    AV_SCOPE("IcdRoot::entry");
    return entry_points_.find(name);
}

Presentation &IcdRoot::presentation()
{
    AV_SCOPE("IcdRoot::presentation");
    return presentation_;
}

bool IcdRoot::present_context(VkDevice device, VkPhysicalDevice &physical, Connection *&connection)
{
    AV_SCOPE("IcdRoot::present_context");
    LoaderHandle *const made = handle_of(device, LoaderHandle::Kind::device);
    if (made == nullptr) return false;
    LoaderHandle *const parent = made->parent();
    physical = static_cast<VkPhysicalDevice>(static_cast<void *>(parent));
    connection = made->connection();
    return true;
}

}  // namespace av::icd
