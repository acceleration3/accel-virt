#include "icd/vulkan/icd_root.hpp"

#include "core/platform/thread_lock_hold.hpp"
#include "core/trace/report_gate.hpp"
#include "guest/transport/shared_object_call.hpp"
#include "icd/vulkan/handle_bits.hpp"
#include "proto/resources/allocation_descriptor.hpp"
#include "proto/resources/allocation_kind.hpp"
#include "proto/resources/scanout_target.hpp"
#include "proto/resources/shared_allocation_data.hpp"
#include "proto/resources/shared_object_kind.hpp"
#include "icd/common/shared_bytes.hpp"
#include "proto/resources/shared_object_record.hpp"
#include "proto/resources/surface_layout.hpp"

#include "core/platform/thread_lock.hpp"
#include "core/support/scope_exit.hpp"
#include "core/trace/function_scope.hpp"
#include "core/trace/state_writer.hpp"
#include "guest/kmt/avgpu_adapters.hpp"
#include "guest/kmt/kmt.hpp"
#include "guest/transport/context_fault.hpp"
#include "icd/vulkan/host_extension_list.hpp"
#include "icd/vulkan/icd_binding.hpp"
#include "icd/vulkan/local_entries.hpp"
#include "core/log/keyed_log.hpp"
#include "icd/vulkan/memory_allocate_chain.hpp"
#include "icd/vulkan/memory_type_table.hpp"
#include "icd/vulkan/physical_device_view.hpp"
#include "icd/common/host_completion_wait.hpp"
#include "icd/vulkan/fence_wait_probe.hpp"
#include "icd/vulkan/query_wait_probe.hpp"
#include "icd/vulkan/semaphore_wait_probe.hpp"
#include "icd/common/sliced_wait.hpp"
#include "proto/generated/vulkan/vk_dispatch_client.h"
#include "proto/generated/vulkan/vk_serialize.h"
#include "proto/generated/vulkan/vk_icd_entries.h"
#include "proto/wire/foreign_enum.hpp"

namespace av::icd {

namespace {
constexpr const char *dma_buf_extension = "VK_EXT_external_memory_dma_buf";
constexpr const char *win32_memory_extension = "VK_KHR_external_memory_win32";

using Entry = vkicd::generated::Entry;
using Level = vkicd::generated::DispatchLevel;

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
    {"vkGetQueryPoolResults", reinterpret_cast<PFN_vkVoidFunction>(&local_vkGetQueryPoolResults), Level::device},
    {"vkDeviceWaitIdle", reinterpret_cast<PFN_vkVoidFunction>(&local_vkDeviceWaitIdle), Level::device},
    {"vkDestroyInstance", reinterpret_cast<PFN_vkVoidFunction>(&local_vkDestroyInstance), Level::instance},
    {"vkEnumeratePhysicalDevices", reinterpret_cast<PFN_vkVoidFunction>(&local_vkEnumeratePhysicalDevices),
            Level::instance},
    {"vkEnumeratePhysicalDeviceGroups",
            reinterpret_cast<PFN_vkVoidFunction>(&local_vkEnumeratePhysicalDeviceGroups), Level::instance},
    {"vkEnumeratePhysicalDeviceGroupsKHR",
            reinterpret_cast<PFN_vkVoidFunction>(&local_vkEnumeratePhysicalDeviceGroupsKHR), Level::instance},
    {"vkAvForceContextFault", reinterpret_cast<PFN_vkVoidFunction>(&local_vkAvForceContextFault), Level::device},
    {"vkAvLastKernelStatus", reinterpret_cast<PFN_vkVoidFunction>(&local_vkAvLastKernelStatus), Level::device},
    {"vkGetPhysicalDeviceMemoryProperties",
            reinterpret_cast<PFN_vkVoidFunction>(&local_vkGetPhysicalDeviceMemoryProperties), Level::physical_device},
    {"vkGetPhysicalDeviceMemoryProperties2",
            reinterpret_cast<PFN_vkVoidFunction>(&local_vkGetPhysicalDeviceMemoryProperties2), Level::physical_device},
    {"vkAllocateMemory", reinterpret_cast<PFN_vkVoidFunction>(&local_vkAllocateMemory), Level::device},
    {"vkFreeMemory", reinterpret_cast<PFN_vkVoidFunction>(&local_vkFreeMemory), Level::device},
    {"vkGetMemoryHostPointerPropertiesEXT",
            reinterpret_cast<PFN_vkVoidFunction>(&local_vkGetMemoryHostPointerPropertiesEXT), Level::device},
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
constexpr uint32_t vulkan_1_3 = (1u << 22u) | (3u << 12u);
constexpr ExtensionPolicy::Row reported_extensions[] = {
    {"VK_EXT_memory_budget", 1u, "icd_memory.the_budget_nodes_in_the_chain_are_the_adapter_s"},
    {"VK_KHR_swapchain", 70u, "icd_presentation.a_swapchain_presents_its_frame_on_the_window"},
    {"VK_KHR_map_memory2", 1u, "icd_memory.map_memory2_maps_what_vk_map_memory_does_and_refuses_placement"},
    {"VK_KHR_descriptor_update_template", 1u,
            "icd_descriptor_templates.a_template_is_remembered_and_sizes_the_blob_it_carries"},
    {"VK_KHR_push_descriptor", 2u,
            "fullstack_templates.a_descriptor_set_updated_through_a_template_reaches_the_host"},
    {"VK_KHR_external_memory_win32", 1u,
            "icd_external_handles.a_handle_exported_is_opened_by_the_device_on_import",
            "VK_KHR_external_memory_fd"},
    {"VK_KHR_external_semaphore_win32", 1u,
            "icd_external_handles.a_semaphore_handle_is_opened_by_the_device_into_the_semaphore_it_names",
            "VK_KHR_external_semaphore_fd"},
    {"VK_KHR_dynamic_rendering", 1u, "reported_extensions.every_feature_structure_an_application_chains_reaches_the_host"},
    {"VK_KHR_synchronization2", 1u, "reported_extensions.every_feature_structure_an_application_chains_reaches_the_host"},
    {"VK_EXT_vertex_attribute_divisor", 3u,
            "reported_extensions.every_feature_structure_an_application_chains_reaches_the_host"},
    {"VK_KHR_image_format_list", 1u, "reported_extensions.an_image_s_format_list_reaches_the_host"},
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
struct InstanceExtension {
    const char *name;
    uint32_t    spec_version;
};
constexpr InstanceExtension reported_instance_extensions[] = {
    {"VK_KHR_get_physical_device_properties2", 2u},
    {"VK_KHR_surface", 25u},
    {"VK_KHR_win32_surface", 6u},
    {"VK_KHR_get_surface_capabilities2", 1u},
};
constexpr uint32_t reported_instance_extension_count =
    sizeof reported_instance_extensions / sizeof reported_instance_extensions[0];
constexpr uint32_t max_chain = 32u;
constexpr uint32_t max_host_extensions = 1024u;
constexpr uint32_t max_allocated_at_once = 1024u;

}

IcdRoot::IcdRoot(BlockSource &memory, Kmt &kmt, ThreadLock &lock, ThreadLock &send_lock, ThreadLock &memory_lock,
                 KeyedLog &log, IcdBinding &binding, WindowSurface &windows, WindowChains &chains,
                 ServiceThread &present_thread, ThreadWake &handed, ThreadWake &shown, ThreadLock &present_lock)
    : memory_(memory), kmt_(kmt), lock_(lock), send_lock_(send_lock), memory_lock_(memory_lock), lines_(log),
      binding_(binding),
      api_version_(vulkan_1_3), before_instance_(local_rows, before_instance_rows),
      entry_points_(local_rows, local_row_count),
      extensions_(reported_extensions, reported_extension_count), batches_(memory), memories_(memory), handles_(memory), shared_handles_(kmt, lock, memory), connections_(memory), pending_(nullptr), spans_(memory),
      entries_(*this), presentation_(memory, kmt, windows, chains, log, *this, entries_, present_thread, handed, shown, present_lock), templates_(memory, lock),
      translation_(lock, handles_, templates_),
      externals_(lock, memory), kernel_refusal_(0u), idle_skipped_(0u), idle_waited_(0u),
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

VkResult IcdRoot::negotiate(uint32_t *version)
{
    AV_SCOPE("IcdRoot::negotiate");
    if (version == nullptr) {
        lines_.write("loader interface negotiation without a version");
        return VK_ERROR_INCOMPATIBLE_DRIVER;
    }
    if (*version < oldest_interface) {
        lines_.note("loader interface version refused:", *version);
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
        lines_.write("instance extensions of a layer asked for; this driver is no layer");
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

Status IcdRoot::open_connection(Connection *&opened, uint64_t luid)
{
    AV_SCOPE("IcdRoot::open_connection");
    const ThreadLockHold release(lock_);
    const Status made = connections_.create(opened, kmt_, memory_, send_lock_, lines_, shared_handles_);
    if (made != Status::ok) return made;
    const Status status = opened->open(luid);
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
    const Status status = open_connection(opened, 0u);
    if (status != Status::ok) {
        lines_.note("vkCreateInstance: no Vulkan connection to an avgpu adapter, status", static_cast<uint64_t>(status));
        return status == Status::no_memory ? VK_ERROR_OUT_OF_HOST_MEMORY : VK_ERROR_INCOMPATIBLE_DRIVER;
    }
    pending_ = opened;
    HostExtensionList asked(memory_);
    const Status taken = asked.take(create->ppEnabledExtensionNames, create->enabledExtensionCount);
    if (taken != Status::ok) {
        lines_.note("vkCreateInstance: the extensions it names are not a list this driver can pass on, count",
             create->enabledExtensionCount);
        release_connection(opened);
        pending_ = nullptr;
        return taken == Status::no_memory ? VK_ERROR_OUT_OF_HOST_MEMORY : VK_ERROR_EXTENSION_NOT_PRESENT;
    }
    VkInstanceCreateInfo forwarded = *create;
    forwarded.enabledExtensionCount = asked.count();
    forwarded.ppEnabledExtensionNames = asked.names();
    const VkResult result = vkicd::generated::Wrap_vkCreateInstance(&forwarded, allocator, instance);
    if (result != VK_SUCCESS) {
        lines_.note("vkCreateInstance: the host refused, result", static_cast<uint32_t>(result));
        return result;
    }
    if (*instance != VK_NULL_HANDLE) {
        span_adapters(forwarded, allocator, *instance);
        return VK_SUCCESS;
    }
    release_connection(opened);
    return VK_ERROR_OUT_OF_HOST_MEMORY;
}

void IcdRoot::span_adapters(const VkInstanceCreateInfo &forwarded, const VkAllocationCallbacks *allocator,
                            VkInstance own)
{
    AV_SCOPE("IcdRoot::span_adapters");
    InstanceSpan *span = nullptr;
    {
        const ThreadLockHold release(lock_);
        if (spans_.create(span, own) != Status::ok) {
            lines_.write("no room to span the other adapters; the instance has the primary display's alone");
            return;
        }
    }
    const Connection *const primary = connection_of(own, LoaderHandle::Kind::instance);
    uint64_t luids[AvgpuAdapters::max_adapters] = {};
    uint32_t count = 0u;
    {
        AvgpuAdapters adapters(kmt_);
        if (adapters.learn() != Status::ok) return;
        count = adapters.luids(luids);
    }
    for (uint32_t index = 0u; index < count; ++index) {
        if (primary != nullptr && luids[index] == primary->luid()) continue;
        Connection  *opened = nullptr;
        const Status status = open_connection(opened, luids[index]);
        if (status != Status::ok) {
            lines_.note("vkCreateInstance: no Vulkan connection to another avgpu adapter, status",
                        static_cast<uint64_t>(status));
            continue;
        }
        pending_ = opened;
        VkInstance     sibling = VK_NULL_HANDLE;
        const VkResult made = vkicd::generated::Wrap_vkCreateInstance(&forwarded, allocator, &sibling);
        if (made != VK_SUCCESS) {
            lines_.note("vkCreateInstance: another adapter's host refused, result", static_cast<uint32_t>(made));
            continue;
        }
        if (sibling == VK_NULL_HANDLE) {
            release_connection(opened);
            continue;
        }
        if (span->add(sibling) != Status::ok) destroy_one_instance(sibling, allocator);
    }
}

InstanceSpan *IcdRoot::span_of(VkInstance instance)
{
    AV_SCOPE("IcdRoot::span_of");
    return spans_.find_if([instance](const InstanceSpan &span) { return span.spans_from(instance); });
}

VkResult IcdRoot::enumerate_physical_devices(VkInstance instance, uint32_t *count, VkPhysicalDevice *physical_devices)
{
    AV_SCOPE("IcdRoot::enumerate_physical_devices");
    InstanceSpan *span = nullptr;
    {
        const ThreadLockHold release(lock_);
        span = span_of(instance);
    }
    if (span == nullptr || span->count() == 1u)
        return vkicd::generated::Wrap_vkEnumeratePhysicalDevices(instance, count, physical_devices);
    uint32_t total = 0u;
    VkResult answer = VK_SUCCESS;
    for (uint32_t index = 0u; index < span->count(); ++index) {
        const VkInstance on = span->at(index);
        uint32_t         has = 0u;
        const VkResult   counted = vkicd::generated::Wrap_vkEnumeratePhysicalDevices(on, &has, nullptr);
        if (counted != VK_SUCCESS) {
            if (index == 0u) return counted;
            continue;
        }
        if (physical_devices == nullptr) {
            total += has;
            continue;
        }
        uint32_t room = *count > total ? *count - total : 0u;
        if (room < has) answer = VK_INCOMPLETE;
        if (room > has) room = has;
        if (room == 0u) continue;
        const VkResult listed = vkicd::generated::Wrap_vkEnumeratePhysicalDevices(on, &room, physical_devices + total);
        if (listed != VK_SUCCESS && listed != VK_INCOMPLETE) return listed;
        for (uint32_t at = 0u; at < room; at++)
            if (physical_devices[total + at] == VK_NULL_HANDLE) return VK_ERROR_OUT_OF_HOST_MEMORY;
        total += room;
    }
    *count = total;
    return answer;
}

VkResult IcdRoot::enumerate_physical_device_groups(GroupEnumerator enumerate, VkInstance instance, uint32_t *count,
                                                   VkPhysicalDeviceGroupProperties *groups)
{
    AV_SCOPE("IcdRoot::enumerate_physical_device_groups");
    InstanceSpan *span = nullptr;
    {
        const ThreadLockHold release(lock_);
        span = span_of(instance);
    }
    if (span == nullptr || span->count() == 1u) return enumerate(instance, count, groups);
    uint32_t total = 0u;
    VkResult answer = VK_SUCCESS;
    for (uint32_t index = 0u; index < span->count(); ++index) {
        const VkInstance on = span->at(index);
        uint32_t         has = 0u;
        const VkResult   counted = enumerate(on, &has, nullptr);
        if (counted != VK_SUCCESS) {
            if (index == 0u) return counted;
            continue;
        }
        if (groups == nullptr) {
            total += has;
            continue;
        }
        uint32_t room = *count > total ? *count - total : 0u;
        if (room < has) answer = VK_INCOMPLETE;
        if (room > has) room = has;
        if (room == 0u) continue;
        const VkResult listed = enumerate(on, &room, groups + total);
        if (listed != VK_SUCCESS && listed != VK_INCOMPLETE) return listed;
        total += room;
    }
    *count = total;
    return answer;
}

VkResult IcdRoot::enumerate_adapter_physical_devices(VkInstance instance, uint64_t luid, uint32_t *count,
                                                     VkPhysicalDevice *physical_devices)
{
    AV_SCOPE("IcdRoot::enumerate_adapter_physical_devices");
    Connection *const connection = connection_of(instance, LoaderHandle::Kind::instance);
    if (connection == nullptr) {
        lines_.write("vk_icdEnumerateAdapterPhysicalDevices on an instance this driver did not make");
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    if (connection->refuses("vk_icdEnumerateAdapterPhysicalDevices")) return VK_ERROR_DEVICE_LOST;
    VkInstance        on = luid == connection->luid() ? instance : VK_NULL_HANDLE;
    Connection       *on_connection = connection;
    VkInstance        spanned[InstanceSpan::max_instances] = {};
    uint32_t          spans = 0u;
    {
        const ThreadLockHold release(lock_);
        const InstanceSpan *const span = span_of(instance);
        for (uint32_t index = 1u; span != nullptr && index < span->count(); ++index) spanned[spans++] = span->at(index);
    }
    for (uint32_t index = 0u; on == VK_NULL_HANDLE && index < spans; ++index) {
        Connection *const spanning = connection_of(spanned[index], LoaderHandle::Kind::instance);
        if (spanning == nullptr || spanning->luid() != luid) continue;
        on = spanned[index];
        on_connection = spanning;
    }
    if (on == VK_NULL_HANDLE) {
        *count = 0u;
        return VK_SUCCESS;
    }
    if (on_connection != connection && on_connection->refuses("vk_icdEnumerateAdapterPhysicalDevices"))
        return VK_ERROR_DEVICE_LOST;
    const VkResult result = vkicd::generated::Wrap_vkEnumeratePhysicalDevices(on, count, physical_devices);
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
        lines_.write("vkEnumerateDeviceExtensionProperties on a physical device this driver did not make");
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    if (made->connection()->refuses("vkEnumerateDeviceExtensionProperties")) return VK_ERROR_DEVICE_LOST;
    if (layer != nullptr) {
        lines_.write("device extensions of a layer asked for; this driver is no layer");
        return VK_ERROR_LAYER_NOT_PRESENT;
    }
    ClientTransport &transport = made->connection()->transport();
    uint32_t         listed = 0u;
    VkResult result = vkproto::av_vkEnumerateDeviceExtensionProperties(transport, translation_, physical_device, nullptr, &listed,
                                                                        nullptr);
    if (result != VK_SUCCESS) return result;
    if (listed == 0u) return extensions_.report(nullptr, 0u, count, properties);
    if (listed > max_host_extensions) {
        lines_.note("vkEnumerateDeviceExtensionProperties: the host lists more device extensions than 1024:", listed);
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }
    auto *const host =
        static_cast<VkExtensionProperties *>(memory_.allocate(listed * sizeof(VkExtensionProperties), Pool::paged));
    if (host == nullptr) return VK_ERROR_OUT_OF_HOST_MEMORY;
    ScopeExit released([&] { memory_.release(host); });
    result = vkproto::av_vkEnumerateDeviceExtensionProperties(transport, translation_, physical_device, nullptr, &listed, host);
    if (result != VK_SUCCESS && result != VK_INCOMPLETE) return result;
    for (uint32_t index = 0u; index < listed; index++)
        if (__builtin_strcmp(host[index].extensionName, dma_buf_extension) == 0)
            made->connection()->note_host_dma_buf();
    return extensions_.report(host, listed, count, properties);
}

void IcdRoot::learn_host_sharing(LoaderHandle &made, VkPhysicalDevice physical_device)
{
    AV_SCOPE("IcdRoot::learn_host_sharing");
    if (made.connection()->host_has_dma_buf()) return;
    ClientTransport &transport = made.connection()->transport();
    uint32_t         listed = 0u;
    if (vkproto::av_vkEnumerateDeviceExtensionProperties(transport, translation_, physical_device, nullptr, &listed,
                                                          nullptr) != VK_SUCCESS)
        return;
    if (listed == 0u || listed > max_host_extensions) return;
    auto *const host =
        static_cast<VkExtensionProperties *>(memory_.allocate(listed * sizeof(VkExtensionProperties), Pool::paged));
    if (host == nullptr) return;
    ScopeExit      released([&] { memory_.release(host); });
    const VkResult result =
        vkproto::av_vkEnumerateDeviceExtensionProperties(transport, translation_, physical_device, nullptr, &listed, host);
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
        lines_.write("vkCreateDevice on a physical device this driver did not make");
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    if (made->connection()->refuses("vkCreateDevice")) return VK_ERROR_DEVICE_LOST;
    const uint32_t refused = extensions_.first_refused(create->ppEnabledExtensionNames, create->enabledExtensionCount);
    if (refused != create->enabledExtensionCount) {
        const char *const name = create->ppEnabledExtensionNames[refused];
        lines_.name("vkCreateDevice names an extension this driver does not report:",
                    name != nullptr ? name : "(null)");
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
    HostExtensionList asked(memory_);
    const Status taken = asked.take(create->ppEnabledExtensionNames, create->enabledExtensionCount);
    if (taken != Status::ok) {
        lines_.note("vkCreateDevice: the extensions it names are not a list this driver can pass on, count",
             create->enabledExtensionCount);
        return taken == Status::no_memory ? VK_ERROR_OUT_OF_HOST_MEMORY : VK_ERROR_EXTENSION_NOT_PRESENT;
    }
    if (asked.named(win32_memory_extension)) {
        learn_host_sharing(*made, physical_device);
        if (made->connection()->host_has_dma_buf()) static_cast<void>(asked.add(dma_buf_extension));
    }
    VkDeviceCreateInfo forwarded = *create;
    forwarded.enabledExtensionCount = asked.count();
    forwarded.ppEnabledExtensionNames = asked.names();
    uint64_t       host = 0u;
    const VkResult result =
        vkproto::av_vkCreateDevice(made->connection()->transport(), translation_, physical_device, &forwarded, allocator, &host);
    if (result != VK_SUCCESS) return result;
    *device = static_cast<VkDevice>(wrap_child(LoaderHandle::Kind::device, host, *made));
    return *device != VK_NULL_HANDLE ? VK_SUCCESS : VK_ERROR_OUT_OF_HOST_MEMORY;
}

void IcdRoot::release_device_memory(LoaderHandle &device)
{
    AV_SCOPE("IcdRoot::release_device_memory");
    const ThreadLockHold release(memory_lock_);
    DeviceMemory *const memory = device.device_memory();
    if (memory == nullptr) return;
    const Status released = memory->release_all();
    if (released != Status::ok) lines_.note("device memory not released whole, status", static_cast<uint64_t>(released));
}

vkproto::HandleTranslation &IcdRoot::wire_handles()
{
    AV_SCOPE("IcdRoot::wire_handles");
    return translation_;
}

VkResult IcdRoot::create_descriptor_update_template(VkDevice device,
                                                    const VkDescriptorUpdateTemplateCreateInfo *create,
                                                    const VkAllocationCallbacks *allocator,
                                                    VkDescriptorUpdateTemplate *made)
{
    AV_SCOPE("IcdRoot::create_descriptor_update_template");
    if (create == nullptr || made == nullptr) return VK_ERROR_INITIALIZATION_FAILED;
    DescriptorTemplateLayout layout;
    if (layout.take(create->pDescriptorUpdateEntries, create->descriptorUpdateEntryCount) != Status::ok) {
        lines_.write("a descriptor update template names entries this driver cannot size");
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    const VkResult result = vkicd::generated::Wrap_vkCreateDescriptorUpdateTemplate(device, create, allocator, made);
    if (result != VK_SUCCESS) return result;
    const VkResult recorded = templates_.record(device, *made, create);
    if (recorded == VK_SUCCESS) return VK_SUCCESS;
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

VkFlags IcdRoot::memory_share_type(const Connection &connection) const
{
    AV_SCOPE("IcdRoot::memory_share_type");
    if (connection.host_has_dma_buf()) return VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT;
    return memory_handles_.host_type();
}

VkResult IcdRoot::publish(LoaderHandle &made, VkDevice device, uint32_t kind, uint64_t size,
                          VkDeviceMemory memory, VkSemaphore semaphore)
{
    AV_SCOPE("IcdRoot::publish");
    const uint32_t guest_device = made.connection()->device();
    SharedAllocationData carrier_data(
        AllocationDescriptor(AllocationKind::client, true, SurfaceLayout(), SharedObjectRecord::page_size,
                             ScanoutTarget()),
        SharedObjectKind(kind), size);
    uint32_t     carrier = 0u;
    const Status created = kmt_.create_shared_allocation(
        guest_device, carrier_data.lend(), static_cast<uint32_t>(SharedAllocationData::encoded_size), carrier);
    if (created != Status::ok) {
        refused("vkAllocateMemory: no allocation to mint a shared object's identity into, NTSTATUS");
        lines_.note("vkAllocateMemory: the carrier was asked for on guest device", guest_device);
        lines_.note("vkAllocateMemory: the carrier's private driver data was bytes", SharedAllocationData::encoded_size);
        return VK_ERROR_TOO_MANY_OBJECTS;
    }
    ScopeExit unwind([&] { static_cast<void>(kmt_.destroy_shared_allocation(guest_device, carrier)); });
    if (carrier_data.reread(SharedAllocationData::encoded_size) != Status::ok || !carrier_data.assigned()) {
        lines_.write("the miniport minted no identity into a shared object's carrier");
        return VK_ERROR_INVALID_EXTERNAL_HANDLE;
    }
    SharedObjectCall     call(made.connection()->transport());
    const uint64_t       wire_handle = memory != VK_NULL_HANDLE ? HandleBits(memory).value()
                                                                : HandleBits(semaphore).value();
    const Status         published = call.share(carrier_data.object_id(), kind, size, wire_handle);
    if (published != Status::ok) {
        lines_.note("vkAllocateMemory: the device would not publish a shared object, status", call.device_status());
        return VK_ERROR_TOO_MANY_OBJECTS;
    }
    uint64_t     handle = 0u;
    const Status shared = shared_handles_.mint(guest_device, carrier, carrier_data.object_id(), handle);
    if (shared != Status::ok) {
        if (shared == Status::no_memory) {
            lines_.write("no memory to keep a shared object's NT handle");
            return VK_ERROR_OUT_OF_HOST_MEMORY;
        }
        refused("vkAllocateMemory: no NT handle for a shared object, NTSTATUS");
        return VK_ERROR_TOO_MANY_OBJECTS;
    }
    if (!externals_.remember(device, memory, semaphore, carrier_data.object_id(), size, handle, carrier)) {
        lines_.write("no memory to remember a shared object; it is given back");
        shared_handles_.forget(handle);
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }
    unwind.dismiss();
    return VK_SUCCESS;
}

void IcdRoot::release_shared(LoaderHandle &made, const ExternalObjectSlot &row)
{
    AV_SCOPE("IcdRoot::release_shared");
    shared_handles_.forget(row.handle_);
    static_cast<void>(kmt_.destroy_shared_allocation(made.connection()->device(), row.carrier_));
}

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
        lines_.write("vkGetMemoryWin32HandleKHR names memory this process never declared exportable");
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
    uint64_t named = externals_.handle_of_semaphore(request->semaphore_);
    if (named == 0u) {
        if (request->semaphore_ == VK_NULL_HANDLE) return VK_ERROR_INVALID_EXTERNAL_HANDLE;
        if (made->connection()->refuses("vkGetSemaphoreWin32HandleKHR")) return VK_ERROR_DEVICE_LOST;
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
    lines_.note(what, kernel_refusal_);
}

VkResult IcdRoot::last_kernel_status(VkDevice device, uint32_t *status)
{
    AV_SCOPE("IcdRoot::last_kernel_status");
    if (status == nullptr) return VK_ERROR_INITIALIZATION_FAILED;
    *status = 0u;
    if (connection_of(device, LoaderHandle::Kind::device) == nullptr)
        return VK_ERROR_INITIALIZATION_FAILED;
    *status = kernel_refusal_;
    return VK_SUCCESS;
}

VkResult IcdRoot::memory_handle_properties(VkDevice device, void *)
{
    AV_SCOPE("IcdRoot::memory_handle_properties");
    if (allocating_device(device) == nullptr) return VK_ERROR_INVALID_EXTERNAL_HANDLE;
    lines_.write("vkGetMemoryWin32HandlePropertiesKHR: a handle this driver did not export names nothing");
    return VK_ERROR_INVALID_EXTERNAL_HANDLE;
}

VkResult IcdRoot::import_semaphore_handle(VkDevice device, const ImportSemaphoreWin32HandleRecord *request)
{
    AV_SCOPE("IcdRoot::import_semaphore_handle");
    LoaderHandle *const made = allocating_device(device);
    if (made == nullptr) return VK_ERROR_INVALID_EXTERNAL_HANDLE;
    if (made->connection()->refuses("vkImportSemaphoreWin32HandleKHR")) return VK_ERROR_DEVICE_LOST;
    if (request == nullptr || request->name_ != nullptr || request->semaphore_ == VK_NULL_HANDLE)
        return VK_ERROR_INVALID_EXTERNAL_HANDLE;
    if (foreign_enum(request->type_) != static_cast<int32_t>(import_semaphore_win32_handle_info) ||
        request->handle_type_ != opaque_win32_semaphore)
        return VK_ERROR_INVALID_EXTERNAL_HANDLE;
    const ThreadLockHold release(memory_lock_);
    Connection          &connection = *made->connection();
    OpenedObjects       &opens = connection.opened_objects();
    const uint64_t       semaphore = HandleBits(request->semaphore_).value();
    const uint64_t       handle = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(request->handle_));
    static_cast<void>(opens.give_back(connection.transport(), SharedObjectKind::monitored_fence, semaphore));
    uint64_t       opened = 0u;
    const VkResult answered = opened_answer(opens.open(connection.transport(), connection.device(), handle,
                                                       SharedObjectKind::monitored_fence, 0u, semaphore, opened),
                                            "vkImportSemaphoreWin32HandleKHR");
    if (answered != VK_SUCCESS) return answered;
    if (opened == semaphore) return VK_SUCCESS;
    lines_.write("a semaphore's import answered another semaphore; it is given back");
    static_cast<void>(opens.give_back(connection.transport(), SharedObjectKind::monitored_fence, opened));
    return VK_ERROR_INVALID_EXTERNAL_HANDLE;
}

VkResult IcdRoot::opened_answer(Status opened, const char *api)
{
    AV_SCOPE("IcdRoot::opened_answer");
    if (opened == Status::ok) return VK_SUCCESS;
    if (opened == Status::no_memory) return VK_ERROR_OUT_OF_HOST_MEMORY;
    if (opened == Status::not_found) {
        refused("a Win32 handle this process may not open, NTSTATUS");
        lines_.name("a handle this process may not open was named to", api);
    }
    return VK_ERROR_INVALID_EXTERNAL_HANDLE;
}

void IcdRoot::destroy_semaphore(VkDevice device, VkSemaphore semaphore, const VkAllocationCallbacks *allocator)
{
    AV_SCOPE("IcdRoot::destroy_semaphore");
    LoaderHandle *const made = allocating_device(device);
    if (made == nullptr) return;
    {
        const ThreadLockHold release(memory_lock_);
        Connection          &connection = *made->connection();
        static_cast<void>(connection.opened_objects().give_back(connection.transport(),
                                                                SharedObjectKind::monitored_fence,
                                                                HandleBits(semaphore).value()));
        ExternalObjectSlot row = {};
        if (externals_.take_semaphore(semaphore, row)) release_shared(*made, row);
    }
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
        properties->externalMemoryProperties = VkExternalMemoryProperties{};
        return;
    }
    VkPhysicalDeviceExternalBufferInfo asked = *info;
    asked.pNext = nullptr;
    asked.handleType = static_cast<VkExternalMemoryHandleTypeFlagBits>(memory_share_type(*connection));
    VkExternalBufferProperties answered = {};
    answered.sType = VK_STRUCTURE_TYPE_EXTERNAL_BUFFER_PROPERTIES;
    vkproto::av_vkGetPhysicalDeviceExternalBufferProperties(connection->transport(), translation_, physical_device, &asked,
                                                             &answered);
    properties->externalMemoryProperties.externalMemoryFeatures =
        answered.externalMemoryProperties.externalMemoryFeatures;
    properties->externalMemoryProperties.exportFromImportedHandleTypes = 0u;
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
    vkproto::av_vkGetPhysicalDeviceExternalSemaphoreProperties(connection->transport(), translation_, physical_device, &asked,
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
    const uint64_t size = SharedBytes(info.allocationSize).pages();
    if (size == 0u) return VK_ERROR_INVALID_EXTERNAL_HANDLE;
    const ImportMemoryWin32HandleRecord *const importing = imported_memory_record(info.pNext);
    if (importing != nullptr) return imported_allocation(made, *importing, size, memory);
    MemoryAllocateChain chain;
    const Status        built = chain.take(info, memory_handles_, memory_share_type(*made.connection()));
    if (built != Status::ok) {
        lines_.note("vkAllocateMemory: the chain of a shared allocation is not one this driver can send, status",
             static_cast<uint64_t>(built));
        return VK_ERROR_INVALID_EXTERNAL_HANDLE;
    }
    const VkResult allocated = vkproto::av_vkAllocateMemory(made.connection()->transport(), translation_, device,
                                                             chain.forwarded(), allocator, &memory);
    if (allocated != VK_SUCCESS) return allocated;
    const VkResult published = publish(made, device, SharedObjectKind::memory, size, memory, VK_NULL_HANDLE);
    if (published == VK_SUCCESS) return VK_SUCCESS;
    vkproto::av_vkFreeMemory(made.connection()->transport(), translation_, device, memory, allocator);
    memory = VK_NULL_HANDLE;
    return published;
}

VkResult IcdRoot::imported_allocation(LoaderHandle &made, const ImportMemoryWin32HandleRecord &importing,
                                      uint64_t size, VkDeviceMemory &memory)
{
    AV_SCOPE("IcdRoot::imported_allocation");
    if (importing.name_ != nullptr) return VK_ERROR_INVALID_EXTERNAL_HANDLE;
    if (importing.handle_type_ != opaque_win32_memory) return VK_ERROR_INVALID_EXTERNAL_HANDLE;
    Connection    &connection = *made.connection();
    uint64_t       opened = 0u;
    const VkResult answered = opened_answer(
        connection.opened_objects().open(connection.transport(), connection.device(),
                                         static_cast<uint64_t>(reinterpret_cast<uintptr_t>(importing.handle_)),
                                         SharedObjectKind::memory, size, made.host(), opened),
        "vkAllocateMemory");
    if (answered == VK_SUCCESS) memory = HandleBits(opened).as<VkDeviceMemory>();
    return answered;
}

void IcdRoot::destroy_device(VkDevice device, const VkAllocationCallbacks *allocator)
{
    AV_SCOPE("IcdRoot::destroy_device");
    templates_.forget_device(device);
    LoaderHandle *const made = handle_of(device, LoaderHandle::Kind::device);
    if (made == nullptr) return;
    {
        const ThreadLockHold release(memory_lock_);
        ExternalObjectSlot   row = {};
        while (externals_.take_any_of_device(device, row)) release_shared(*made, row);
        made->connection()->opened_objects().give_back_into(made->connection()->transport(), made->host());
    }
    presentation_.forget_device(device);
    release_device_memory(*made);
    release_idle_fences(*made);
    lines_.note("queue idle waits answered without the host", idle_skipped_);
    lines_.note("queue idle waits sent to the host", idle_waited_);
    vkicd::generated::Wrap_vkDestroyDevice(device, allocator);
    destroy_wrapper(device, LoaderHandle::Kind::device);
}

void IcdRoot::release_idle_fences(LoaderHandle &device)
{
    AV_SCOPE("IcdRoot::release_idle_fences");
    for (uint32_t index = 0u;; index++) {
        LoaderHandle *const queue = child_of(device, LoaderHandle::Kind::queue, index);
        if (queue == nullptr) return;
        if (queue->idle_fence() == 0u) continue;
        vkproto::av_vkDestroyFence(device.connection()->transport(), translation_, static_cast<VkDevice>(static_cast<void *>(&device)),
                                    HandleBits(queue->idle_fence()).as<VkFence>(), nullptr);
        queue->keep_idle_fence(0u);
    }
}

void IcdRoot::destroy_instance(VkInstance instance, const VkAllocationCallbacks *allocator)
{
    AV_SCOPE("IcdRoot::destroy_instance");
    InstanceSpan *span = nullptr;
    {
        const ThreadLockHold release(lock_);
        span = span_of(instance);
    }
    if (span != nullptr) {
        for (uint32_t index = 1u; index < span->count(); ++index) destroy_one_instance(span->at(index), allocator);
        const ThreadLockHold release(lock_);
        static_cast<void>(spans_.destroy(span));
    }
    destroy_one_instance(instance, allocator);
}

void IcdRoot::destroy_one_instance(VkInstance instance, const VkAllocationCallbacks *allocator)
{
    AV_SCOPE("IcdRoot::destroy_one_instance");
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
    if (connection.refuses(entry)) return;
    VkPhysicalDeviceMemoryProperties host = {};
    vkproto::av_vkGetPhysicalDeviceMemoryProperties(connection.transport(), translation_, physical_device, &host);
    if (connection.refuses(entry)) return;
    const Status built = types.build(host);
    if (built != Status::ok) lines_.note("the host's memory types were not taken, status", static_cast<uint64_t>(built));
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
    lines_.write("memory of a device this driver did not make through a physical device");
    return nullptr;
}

DeviceMemory *IcdRoot::memory_of(LoaderHandle &device)
{
    AV_SCOPE("IcdRoot::memory_of");
    const ThreadLockHold release(lock_);
    if (device.device_memory() != nullptr) return device.device_memory();
    DeviceMemory *made = nullptr;
    if (memories_.create(made, kmt_, *device.connection(), translation_, lines_) != Status::ok) {
        lines_.write("no memory to keep a device's allocations in");
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
    if (made->connection()->refuses("vkAllocateMemory")) return VK_ERROR_DEVICE_LOST;
    const ThreadLockHold release(memory_lock_);
    if (shares_memory(info->pNext)) return shared_allocation(*made, device, *info, allocator, *memory);
    DeviceMemory *const owned = memory_of(*made);
    if (owned == nullptr) return VK_ERROR_OUT_OF_HOST_MEMORY;
    const auto physical = static_cast<VkPhysicalDevice>(static_cast<void *>(made->parent()));
    return owned->allocate(physical, device, *info, allocator, *memory);
}

VkResult IcdRoot::memory_host_pointer_properties(VkDevice device, VkExternalMemoryHandleTypeFlagBits type,
                                                 const void *pointer, VkMemoryHostPointerPropertiesEXT *properties)
{
    AV_SCOPE("IcdRoot::memory_host_pointer_properties");
    if (properties == nullptr) return VK_ERROR_INVALID_EXTERNAL_HANDLE;
    properties->memoryTypeBits = 0u;
    LoaderHandle *const made = allocating_device(device);
    if (made == nullptr) return VK_ERROR_DEVICE_LOST;
    const ThreadLockHold release(memory_lock_);
    DeviceMemory *const owned = memory_of(*made);
    if (owned == nullptr) return VK_ERROR_OUT_OF_HOST_MEMORY;
    const auto physical = static_cast<VkPhysicalDevice>(static_cast<void *>(made->parent()));
    return owned->host_pointer_types(physical, type, pointer, properties->memoryTypeBits);
}

void IcdRoot::free_memory(VkDevice device, VkDeviceMemory memory, const VkAllocationCallbacks *allocator)
{
    AV_SCOPE("IcdRoot::free_memory");
    LoaderHandle *const made = allocating_device(device);
    if (made == nullptr) return;
    const ThreadLockHold release(memory_lock_);
    if (made->connection()->opened_objects().give_back(made->connection()->transport(), SharedObjectKind::memory,
                                                       HandleBits(memory).value()))
        return;
    ExternalObjectSlot row = {};
    if (externals_.take_memory(memory, row)) release_shared(*made, row);
    DeviceMemory *const owned = memory_of(*made);
    if (owned != nullptr) owned->free(device, memory, allocator);
}

VkResult IcdRoot::map_memory(VkDevice device, VkDeviceMemory memory, VkDeviceSize offset, VkDeviceSize size,
                             VkMemoryMapFlags flags, void **data)
{
    AV_SCOPE("IcdRoot::map_memory");
    LoaderHandle *const made = allocating_device(device);
    if (made == nullptr) return VK_ERROR_MEMORY_MAP_FAILED;
    if (made->connection()->refuses("vkMapMemory")) return VK_ERROR_DEVICE_LOST;
    const ThreadLockHold release(memory_lock_);
    const DeviceMemory *const owned = made->device_memory();
    if (owned == nullptr) return VK_ERROR_MEMORY_MAP_FAILED;
    return owned->map(memory, offset, size, flags, *data);
}

VkResult IcdRoot::map_memory2(VkDevice device, const VkMemoryMapInfoKHR *info, void **data)
{
    AV_SCOPE("IcdRoot::map_memory2");
    if (info == nullptr || data == nullptr) return VK_ERROR_MEMORY_MAP_FAILED;
    if (info->pNext != nullptr) {
        lines_.write("vkMapMemory2 names something this driver cannot place");
        return VK_ERROR_MEMORY_MAP_FAILED;
    }
    return map_memory(device, info->memory, info->offset, info->size, info->flags, data);
}

VkResult IcdRoot::mapped_ranges(VkDevice device, const char *entry)
{
    AV_SCOPE("IcdRoot::mapped_ranges");
    Connection *const connection = connection_of(device, LoaderHandle::Kind::device);
    if (connection == nullptr) {
        lines_.write("mapped ranges of a device this driver did not make");
        return VK_ERROR_DEVICE_LOST;
    }
    return connection->refuses(entry) ? VK_ERROR_DEVICE_LOST : VK_SUCCESS;
}

void IcdRoot::physical_device_properties(VkPhysicalDevice physical_device, VkPhysicalDeviceProperties *properties)
{
    AV_SCOPE("IcdRoot::physical_device_properties");
    Connection *const connection = connection_of(physical_device, LoaderHandle::Kind::physical_device);
    if (connection == nullptr) return;
    PhysicalDeviceView view(connection->transport(), translation_, connection->luid(), connection->adapter_name(), api_version_,
                            lines_);
    view.properties(physical_device, properties);
}

void IcdRoot::physical_device_properties2(VkPhysicalDevice physical_device, VkPhysicalDeviceProperties2 *properties)
{
    AV_SCOPE("IcdRoot::physical_device_properties2");
    Connection *const connection = connection_of(physical_device, LoaderHandle::Kind::physical_device);
    if (connection == nullptr) return;
    PhysicalDeviceView view(connection->transport(), translation_, connection->luid(), connection->adapter_name(), api_version_,
                            lines_);
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
    const ThreadLockHold release(lock_);
    if (connections_.destroy(connection) != Status::ok) lines_.write("release of a connection never opened");
}

void *IcdRoot::wrap(LoaderHandle::Kind kind, uint64_t host, Connection *connection)
{
    AV_SCOPE("IcdRoot::wrap");
    const ThreadLockHold release(lock_);
    LoaderHandle *const parent = kind == LoaderHandle::Kind::instance ? nullptr : handles_.instance_of(connection);
    return wrap_under(kind, host, connection, parent);
}

LoaderHandle *IcdRoot::wrap_under(LoaderHandle::Kind kind, uint64_t host, Connection *connection,
                                  LoaderHandle *parent)
{
    AV_SCOPE("IcdRoot::wrap_under");
    LoaderHandle *handle = nullptr;
    const Status  status = handles_.wrap(kind, host, connection, parent, handle);
    if (status != Status::ok) {
        lines_.note("no handle for host object, status", static_cast<uint64_t>(status));
        return nullptr;
    }
    if (kind != LoaderHandle::Kind::command_buffer || handle->batch() != nullptr) return handle;
    CommandBatch *batch = nullptr;
    if (batches_.create(batch, memory_, *connection) != Status::ok) {
        lines_.note("no batch for host command buffer", host);
        static_cast<void>(handles_.destroy(handle));
        return nullptr;
    }
    handle->record_into(batches_, *batch);
    return handle;
}

void *IcdRoot::wrap_child(LoaderHandle::Kind kind, uint64_t host, LoaderHandle &parent)
{
    AV_SCOPE("IcdRoot::wrap_child");
    const ThreadLockHold release(lock_);
    return wrap_under(kind, host, parent.connection(), &parent);
}

LoaderHandle *IcdRoot::handle_of(void *handle, LoaderHandle::Kind kind)
{
    AV_SCOPE("IcdRoot::handle_of");
    const ThreadLockHold release(lock_);
    return handles_.made(handle, kind);
}

LoaderHandle *IcdRoot::child_of(const LoaderHandle &parent, LoaderHandle::Kind kind, uint32_t index)
{
    AV_SCOPE("IcdRoot::child_of");
    const ThreadLockHold release(lock_);
    return handles_.child(parent, kind, index);
}

Connection *IcdRoot::connection_of(void *handle, LoaderHandle::Kind kind)
{
    AV_SCOPE("IcdRoot::connection_of");
    const ThreadLockHold release(lock_);
    const LoaderHandle *const made = handles_.made(handle, kind);
    return made != nullptr ? made->connection() : nullptr;
}

void IcdRoot::destroy_wrapper(void *handle, LoaderHandle::Kind kind)
{
    AV_SCOPE("IcdRoot::destroy_wrapper");
    const ThreadLockHold release(lock_);
    if (handles_.destroy(handles_.made(handle, kind)) != Status::ok)
        lines_.write("destroy of a handle this driver did not make");
}

CommandBatch &IcdRoot::record_target(VkCommandBuffer command_buffer)
{
    AV_SCOPE("IcdRoot::record_target");
    const ThreadLockHold release(lock_);
    return *handles_.made(command_buffer, LoaderHandle::Kind::command_buffer)->batch();
}

void IcdRoot::device_queue(VkDevice device, uint32_t family, uint32_t index, VkQueue *queue)
{
    AV_SCOPE("IcdRoot::device_queue");
    LoaderHandle *const made = handle_of(device, LoaderHandle::Kind::device);
    if (made == nullptr) return;
    uint64_t host = 0u;
    vkproto::av_vkGetDeviceQueue(made->connection()->transport(), translation_, device, family, index, &host);
    *queue = static_cast<VkQueue>(wrap_child(LoaderHandle::Kind::queue, host, *made));
}

void IcdRoot::device_queue2(VkDevice device, const VkDeviceQueueInfo2 *info, VkQueue *queue)
{
    AV_SCOPE("IcdRoot::device_queue2");
    LoaderHandle *const made = handle_of(device, LoaderHandle::Kind::device);
    if (made == nullptr) return;
    uint64_t host = 0u;
    vkproto::av_vkGetDeviceQueue2(made->connection()->transport(), translation_, device, info, &host);
    *queue = static_cast<VkQueue>(wrap_child(LoaderHandle::Kind::queue, host, *made));
}

VkResult IcdRoot::allocate_command_buffers(VkDevice device, const VkCommandBufferAllocateInfo *info,
                                           VkCommandBuffer *command_buffers)
{
    AV_SCOPE("IcdRoot::allocate_command_buffers");
    LoaderHandle *const made = handle_of(device, LoaderHandle::Kind::device);
    if (made == nullptr) {
        lines_.write("vkAllocateCommandBuffers on a device this driver did not make");
        return VK_ERROR_DEVICE_LOST;
    }
    if (made->connection()->refuses("vkAllocateCommandBuffers")) return VK_ERROR_DEVICE_LOST;
    const uint32_t count = info->commandBufferCount;
    if (count > max_allocated_at_once) {
        lines_.note("vkAllocateCommandBuffers: more command buffers at once than", max_allocated_at_once);
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }
    uint64_t       hosts[max_allocated_at_once] = {};
    const VkResult result = vkproto::av_vkAllocateCommandBuffers(made->connection()->transport(), translation_, device, info, hosts);
    if (result != VK_SUCCESS) return result;
    for (uint32_t wrapped = 0u; wrapped < count; wrapped++) {
        command_buffers[wrapped] =
            static_cast<VkCommandBuffer>(wrap_child(LoaderHandle::Kind::command_buffer, hosts[wrapped], *made));
        if (command_buffers[wrapped] != VK_NULL_HANDLE) continue;
        free_command_buffers(device, info->commandPool, wrapped, command_buffers);
        lines_.note("vkAllocateCommandBuffers: host command buffers left in their pool:", count - wrapped);
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
    vkproto::av_vkFreeCommandBuffers(connection->transport(), translation_, device, pool, count, command_buffers);
    for (uint32_t index = 0u; index < count; index++)
        if (command_buffers[index] != VK_NULL_HANDLE)
            destroy_wrapper(command_buffers[index], LoaderHandle::Kind::command_buffer);
}

VkResult IcdRoot::begin_command_buffer(VkCommandBuffer command_buffer, const VkCommandBufferBeginInfo *info)
{
    AV_SCOPE("IcdRoot::begin_command_buffer");
    if (connection_of(command_buffer, LoaderHandle::Kind::command_buffer)->refuses("vkBeginCommandBuffer"))
        return VK_ERROR_DEVICE_LOST;
    CommandBatch &batch = record_target(command_buffer);
    batch.discard();
    if (vkproto::av_vkBeginCommandBuffer(batch, translation_, command_buffer, info) == VK_SUCCESS) return VK_SUCCESS;
    lines_.write("vkBeginCommandBuffer: no memory to record into");
    return VK_ERROR_OUT_OF_HOST_MEMORY;
}

VkResult IcdRoot::end_command_buffer(VkCommandBuffer command_buffer)
{
    AV_SCOPE("IcdRoot::end_command_buffer");
    CommandBatch &batch = record_target(command_buffer);
    if (connection_of(command_buffer, LoaderHandle::Kind::command_buffer)->refuses("vkEndCommandBuffer")) {
        batch.discard();
        return VK_ERROR_DEVICE_LOST;
    }
    const VkResult recorded = vkproto::av_vkEndCommandBuffer(batch, translation_, command_buffer);
    const Status   shipped = batch.end();
    if (shipped == Status::ok) return recorded;
    lines_.note("vkEndCommandBuffer: batch not shipped, status", static_cast<uint64_t>(shipped));
    if (batch.refused() != 0u) lines_.note("vkEndCommandBuffer: a command larger than its room, id", batch.refused());
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
        lines_.write("vkWaitForFences on a device this driver did not make");
        return VK_ERROR_DEVICE_LOST;
    }
    if (made->connection()->refuses("vkWaitForFences")) return VK_ERROR_DEVICE_LOST;
    FenceWaitProbe     probe(made->connection()->transport(), translation_, device, count, fences, wait_all);
    HostCompletionWait wait(kmt_, timeout);
    static_cast<void>(wait.run(probe));
    return probe.result();
}

VkResult IcdRoot::wait_semaphores(VkDevice device, const VkSemaphoreWaitInfo *info, uint64_t timeout)
{
    AV_SCOPE("IcdRoot::wait_semaphores");
    LoaderHandle *const made = handle_of(device, LoaderHandle::Kind::device);
    if (made == nullptr) {
        lines_.write("vkWaitSemaphores on a device this driver did not make");
        return VK_ERROR_DEVICE_LOST;
    }
    if (made->connection()->refuses("vkWaitSemaphores")) return VK_ERROR_DEVICE_LOST;
    SemaphoreWaitProbe probe(made->connection()->transport(), translation_, device, info);
    HostCompletionWait wait(kmt_, timeout);
    static_cast<void>(wait.run(probe));
    return probe.result();
}

VkResult IcdRoot::query_pool_results(VkDevice device, VkQueryPool pool, uint32_t first, uint32_t count, size_t size,
                                     void *data, VkDeviceSize stride, VkQueryResultFlags flags)
{
    AV_SCOPE("IcdRoot::query_pool_results");
    LoaderHandle *const made = handle_of(device, LoaderHandle::Kind::device);
    if (made == nullptr) {
        lines_.write("vkGetQueryPoolResults on a device this driver did not make");
        return VK_ERROR_DEVICE_LOST;
    }
    if (made->connection()->refuses("vkGetQueryPoolResults")) return VK_ERROR_DEVICE_LOST;
    ClientTransport &transport = made->connection()->transport();
    if ((flags & VK_QUERY_RESULT_WAIT_BIT) == 0u)
        return vkproto::av_vkGetQueryPoolResults(transport, translation_, device, pool, first, count, size, data, stride, flags);
    QueryWaitProbe     probe(transport, translation_, kmt_, device, pool, first, count, size, data, stride, flags);
    HostCompletionWait wait(kmt_, SlicedWait::forever);
    static_cast<void>(wait.run(probe));
    return probe.result();
}

VkResult IcdRoot::queue_wait_idle(VkQueue queue)
{
    AV_SCOPE("IcdRoot::queue_wait_idle");
    LoaderHandle *const made = handle_of(queue, LoaderHandle::Kind::queue);
    if (made == nullptr || made->parent()->kind() != LoaderHandle::Kind::device) {
        lines_.write("vkQueueWaitIdle on a queue this driver did not make through a device");
        return VK_ERROR_DEVICE_LOST;
    }
    if (made->connection()->refuses("vkQueueWaitIdle")) return VK_ERROR_DEVICE_LOST;
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
        const VkResult created = vkproto::av_vkCreateFence(transport, translation_, device, &info, nullptr, &fence);
        if (created != VK_SUCCESS) return created;
        made->keep_idle_fence(HandleBits(fence).value());
    } else {
        const VkResult reset = vkproto::av_vkResetFences(transport, translation_, device, 1u, &fence);
        if (reset != VK_SUCCESS) return reset;
    }
    const VkResult submitted = vkproto::av_vkQueueSubmit(transport, translation_, queue, 0u, nullptr, fence);
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
    if (made->connection()->refuses("vkAcquireNextImageKHR")) return VK_ERROR_DEVICE_LOST;
    LoaderHandle *const queue = child_of(*made, LoaderHandle::Kind::queue, 0u);
    if (queue == nullptr) {
        lines_.write("vkAcquireNextImageKHR has nothing to signal on: the device handed out no queue");
        return VK_ERROR_DEVICE_LOST;
    }
    VkSubmitInfo submit = {};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.signalSemaphoreCount = semaphore != VK_NULL_HANDLE ? 1u : 0u;
    submit.pSignalSemaphores = &semaphore;
    return vkproto::av_vkQueueSubmit(made->connection()->transport(), translation_, static_cast<VkQueue>(static_cast<void *>(queue)),
                                      1u, &submit, fence);
}

VkResult IcdRoot::device_wait_idle(VkDevice device)
{
    AV_SCOPE("IcdRoot::device_wait_idle");
    LoaderHandle *const made = handle_of(device, LoaderHandle::Kind::device);
    if (made == nullptr) {
        lines_.write("vkDeviceWaitIdle on a device this driver did not make");
        return VK_ERROR_DEVICE_LOST;
    }
    if (made->connection()->refuses("vkDeviceWaitIdle")) return VK_ERROR_DEVICE_LOST;
    for (uint32_t index = 0u;; index++) {
        LoaderHandle *const queue = child_of(*made, LoaderHandle::Kind::queue, index);
        if (queue == nullptr) return VK_SUCCESS;
        const VkResult idle = queue_wait_idle(static_cast<VkQueue>(static_cast<void *>(queue)));
        if (idle != VK_SUCCESS) return idle;
    }
}

VkResult IcdRoot::force_context_fault(VkDevice device)
{
    AV_SCOPE("IcdRoot::force_context_fault");
    LoaderHandle *const made = handle_of(device, LoaderHandle::Kind::device);
    if (made == nullptr) {
        lines_.write("vkAvForceContextFault on a device this driver did not make");
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    Connection &connection = *made->connection();
    if (connection.refuses("vkAvForceContextFault")) return VK_ERROR_DEVICE_LOST;
    if (ContextFault(connection.transport()).send() != Status::ok) {
        lines_.write("the host answered a truncated vkCreateInstance");
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
    const ReportGate gate(lock_, writer, "avvk");
    if (!gate.held()) return;
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
    if (pending_ != nullptr || !handles_.invariants_hold() || handles_on_connections() != handles_.live()) return false;
    if (!batches_.invariants_hold() || batches_.live() != handles_.batches()) return false;
    if (!batches_.all_of([](const CommandBatch &batch) { return batch.invariants_hold(); })) return false;
    if (!memories_.invariants_hold() || memories_.live() != handles_.memories()) return false;
    if (!memories_.all_of([](const DeviceMemory &memory) { return memory.invariants_hold(); })) return false;
    if (!spans_.invariants_hold() || !spans_.all_of([](const InstanceSpan &span) { return span.invariants_hold(); }))
        return false;
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

bool IcdRoot::present_context(VkDevice device, VkPhysicalDevice &physical, uint64_t &host_device,
                              Connection *&connection)
{
    AV_SCOPE("IcdRoot::present_context");
    LoaderHandle *const made = handle_of(device, LoaderHandle::Kind::device);
    if (made == nullptr) return false;
    LoaderHandle *const parent = made->parent();
    physical = static_cast<VkPhysicalDevice>(static_cast<void *>(parent));
    host_device = made->host();
    connection = made->connection();
    return true;
}

}
