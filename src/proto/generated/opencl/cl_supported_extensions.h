// THIS FILE IS GENERATED. DO NOT EDIT.
// Regenerate with: python -m rvk_codegen.opencl --xml cl.xml --out ...

#pragma once

#include <stddef.h>

namespace av::clproto {

// Every extension whose commands this build answers - over the
// wire or in the driver - and whose own dependencies it answers
// too. An extension needing an entry point that does neither is
// not one this driver can offer, whatever the host reports.
inline constexpr const char* kCarriedExtensions[] = {
    "cl_amd_device_attribute_query",
    "cl_arm_controlled_kernel_termination",
    "cl_arm_get_core_id",
    "cl_arm_job_slot_selection",
    "cl_arm_printf",
    "cl_arm_protected_memory_allocation",
    "cl_arm_scheduling_controls",
    "cl_ext_cxx_for_opencl",
    "cl_ext_float_atomics",
    "cl_ext_image_raw10_raw12",
    "cl_ext_image_unorm_int_2_101010",
    "cl_ext_immutable_memory_objects",
    "cl_img_cached_allocations",
    "cl_img_mem_properties",
    "cl_img_yuv_image",
    "cl_intel_advanced_motion_estimation",
    "cl_intel_command_queue_families",
    "cl_intel_create_mem_object_properties",
    "cl_intel_device_attribute_query",
    "cl_intel_device_partition_by_names",
    "cl_intel_device_side_avc_motion_estimation",
    "cl_intel_driver_diagnostics",
    "cl_intel_egl_image_yuv",
    "cl_intel_exec_by_local_thread",
    "cl_intel_mem_alloc_buffer_location",
    "cl_intel_mem_channel_property",
    "cl_intel_mem_force_host_memory",
    "cl_intel_motion_estimation",
    "cl_intel_packed_yuv",
    "cl_intel_planar_yuv",
    "cl_intel_queue_no_sync_operations",
    "cl_intel_required_subgroup_size",
    "cl_intel_sharing_format_query",
    "cl_intel_simultaneous_sharing",
    "cl_khr_3d_image_writes",
    "cl_khr_async_work_group_copy_fence",
    "cl_khr_byte_addressable_store",
    "cl_khr_depth_images",
    "cl_khr_device_enqueue_local_arg_types",
    "cl_khr_device_uuid",
    "cl_khr_expect_assume",
    "cl_khr_extended_async_copies",
    "cl_khr_extended_bit_ops",
    "cl_khr_extended_versioning",
    "cl_khr_external_memory",
    "cl_khr_external_memory_dma_buf",
    "cl_khr_external_memory_opaque_fd",
    "cl_khr_external_memory_win32",
    "cl_khr_fp16",
    "cl_khr_fp64",
    "cl_khr_gl_depth_images",
    "cl_khr_gl_msaa_sharing",
    "cl_khr_gl_sharing",
    "cl_khr_global_int32_base_atomics",
    "cl_khr_global_int32_extended_atomics",
    "cl_khr_image2d_from_buffer",
    "cl_khr_initialize_memory",
    "cl_khr_int64_base_atomics",
    "cl_khr_int64_extended_atomics",
    "cl_khr_integer_dot_product",
    "cl_khr_kernel_clock",
    "cl_khr_local_int32_base_atomics",
    "cl_khr_local_int32_extended_atomics",
    "cl_khr_mipmap_image",
    "cl_khr_mipmap_image_writes",
    "cl_khr_pci_bus_info",
    "cl_khr_priority_hints",
    "cl_khr_select_fprounding_mode",
    "cl_khr_spir",
    "cl_khr_spirv_extended_debug_info",
    "cl_khr_spirv_linkonce_odr",
    "cl_khr_spirv_no_integer_wrap_decoration",
    "cl_khr_srgb_image_writes",
    "cl_khr_subgroup_ballot",
    "cl_khr_subgroup_clustered_reduce",
    "cl_khr_subgroup_extended_types",
    "cl_khr_subgroup_named_barrier",
    "cl_khr_subgroup_non_uniform_arithmetic",
    "cl_khr_subgroup_non_uniform_vote",
    "cl_khr_subgroup_rotate",
    "cl_khr_subgroup_shuffle",
    "cl_khr_subgroup_shuffle_relative",
    "cl_khr_throttle_hints",
    "cl_khr_work_group_uniform_arithmetic",
    "cl_nv_device_attribute_query",
    "cl_qcom_android_native_buffer_host_ptr",
    "cl_qcom_ext_host_ptr_iocoherent",
    "cl_qcom_ion_host_ptr",
};

inline constexpr size_t kCarriedExtensionCount =
    sizeof(kCarriedExtensions) / sizeof(kCarriedExtensions[0]);

[[nodiscard]] inline bool extension_is_carried(const char* name) noexcept {
    if (name == nullptr) return false;
    for (size_t i = 0; i < kCarriedExtensionCount; ++i) {
        if (__builtin_strcmp(name, kCarriedExtensions[i]) == 0) return true;
    }
    return false;
}

}  // namespace av::clproto
