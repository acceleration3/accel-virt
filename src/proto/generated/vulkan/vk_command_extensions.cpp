// THIS FILE IS GENERATED. DO NOT EDIT.
// Regenerate with: cmake --build build --target regenerate-vulkan
//                  (or: python -m av_codegen --xml ... --out generated/)


#include "vk_command_extensions.h"

namespace av::vkproto {

namespace {

struct Row {
    uint32_t id;
    CommandOwner owner;
    uint16_t first;
    uint16_t count;
};

constexpr Row kRows[] = {
    {0x00010081u, CommandOwner::device, 0u, 2u},
    {0x000100C3u, CommandOwner::device, 2u, 2u},
    {0x000100C4u, CommandOwner::device, 4u, 2u},
    {0x00010100u, CommandOwner::device, 6u, 2u},
    {0x0001011Eu, CommandOwner::device, 8u, 1u},
    {0x0001011Fu, CommandOwner::device, 9u, 1u},
    {0x00010120u, CommandOwner::device, 10u, 1u},
    {0x00010122u, CommandOwner::device, 11u, 2u},
    {0x00010123u, CommandOwner::device, 13u, 2u},
    {0x00010126u, CommandOwner::device, 15u, 2u},
    {0x00010127u, CommandOwner::device, 17u, 1u},
    {0x0001012Au, CommandOwner::device, 18u, 1u},
    {0x0001012Cu, CommandOwner::device, 19u, 1u},
    {0x0001012Du, CommandOwner::device, 20u, 1u},
    {0x0001012Eu, CommandOwner::device, 21u, 1u},
    {0x0001012Fu, CommandOwner::device, 22u, 1u},
    {0x00010130u, CommandOwner::device, 23u, 1u},
    {0x00010131u, CommandOwner::device, 24u, 1u},
    {0x00010132u, CommandOwner::device, 25u, 1u},
    {0x00010133u, CommandOwner::device, 26u, 1u},
    {0x00010134u, CommandOwner::device, 27u, 1u},
    {0x00010135u, CommandOwner::device, 28u, 2u},
    {0x00010136u, CommandOwner::device, 30u, 2u},
    {0x00010137u, CommandOwner::device, 32u, 2u},
    {0x00010138u, CommandOwner::device, 34u, 2u},
    {0x00010139u, CommandOwner::device, 36u, 2u},
    {0x0001013Au, CommandOwner::device, 38u, 2u},
    {0x0001013Bu, CommandOwner::device, 40u, 2u},
    {0x0001013Cu, CommandOwner::device, 42u, 2u},
    {0x0001013Du, CommandOwner::device, 44u, 2u},
    {0x0001013Eu, CommandOwner::device, 46u, 2u},
    {0x0001013Fu, CommandOwner::device, 48u, 2u},
    {0x00010140u, CommandOwner::device, 50u, 2u},
    {0x00010141u, CommandOwner::device, 52u, 2u},
    {0x00010144u, CommandOwner::device, 54u, 2u},
    {0x00010145u, CommandOwner::device, 56u, 2u},
    {0x00010146u, CommandOwner::device, 58u, 2u},
    {0x00010147u, CommandOwner::device, 60u, 2u},
    {0x0001014Cu, CommandOwner::device, 62u, 2u},
    {0x0001014Du, CommandOwner::device, 64u, 1u},
    {0x0001014Fu, CommandOwner::device, 65u, 2u},
    {0x00010150u, CommandOwner::device, 67u, 1u},
    {0x00010151u, CommandOwner::device, 68u, 2u},
    {0x00010152u, CommandOwner::device, 70u, 2u},
    {0x00010153u, CommandOwner::device, 72u, 2u},
    {0x00010155u, CommandOwner::device, 74u, 2u},
    {0x00010156u, CommandOwner::device, 76u, 2u},
    {0x00010157u, CommandOwner::device, 78u, 2u},
    {0x00010158u, CommandOwner::device, 80u, 1u},
    {0x00010159u, CommandOwner::device, 81u, 2u},
    {0x0001015Au, CommandOwner::device, 83u, 2u},
    {0x0001015Bu, CommandOwner::device, 85u, 2u},
    {0x0001015Du, CommandOwner::device, 87u, 2u},
    {0x00010160u, CommandOwner::device, 89u, 2u},
    {0x00010161u, CommandOwner::device, 91u, 2u},
    {0x00010162u, CommandOwner::device, 93u, 2u},
    {0x00010164u, CommandOwner::device, 95u, 1u},
    {0x00010165u, CommandOwner::device, 96u, 1u},
    {0x00010166u, CommandOwner::device, 97u, 1u},
    {0x00010167u, CommandOwner::device, 98u, 1u},
    {0x00010168u, CommandOwner::device, 99u, 1u},
    {0x00010169u, CommandOwner::device, 100u, 1u},
    {0x0001016Au, CommandOwner::device, 101u, 1u},
    {0x0001016Bu, CommandOwner::device, 102u, 1u},
    {0x0001016Cu, CommandOwner::device, 103u, 1u},
    {0x0001016Du, CommandOwner::device, 104u, 1u},
    {0x0001016Eu, CommandOwner::device, 105u, 1u},
    {0x0001016Fu, CommandOwner::device, 106u, 1u},
    {0x00010170u, CommandOwner::device, 107u, 1u},
    {0x00010171u, CommandOwner::device, 108u, 1u},
    {0x00010172u, CommandOwner::device, 109u, 1u},
    {0x00010173u, CommandOwner::device, 110u, 1u},
    {0x00010174u, CommandOwner::device, 111u, 1u},
    {0x00010175u, CommandOwner::device, 112u, 1u},
    {0x00010176u, CommandOwner::device, 113u, 1u},
    {0x00010179u, CommandOwner::device, 114u, 1u},
    {0x0001017Au, CommandOwner::device, 115u, 1u},
    {0x0001017Eu, CommandOwner::device, 116u, 1u},
    {0x00010180u, CommandOwner::device, 117u, 2u},
    {0x00010183u, CommandOwner::device, 119u, 1u},
    {0x00010184u, CommandOwner::device, 120u, 1u},
    {0x00010185u, CommandOwner::device, 121u, 1u},
    {0x00010187u, CommandOwner::device, 122u, 1u},
    {0x00010188u, CommandOwner::device, 123u, 1u},
    {0x00010189u, CommandOwner::device, 124u, 1u},
    {0x0001018Au, CommandOwner::device, 125u, 2u},
    {0x0001018Cu, CommandOwner::device, 127u, 1u},
    {0x0001018Du, CommandOwner::device, 128u, 1u},
    {0x0001018Eu, CommandOwner::device, 129u, 1u},
    {0x0001018Fu, CommandOwner::device, 130u, 1u},
    {0x00010190u, CommandOwner::device, 131u, 1u},
    {0x00010191u, CommandOwner::device, 132u, 1u},
    {0x00010192u, CommandOwner::device, 133u, 1u},
    {0x00010193u, CommandOwner::device, 134u, 1u},
    {0x00010194u, CommandOwner::device, 135u, 1u},
    {0x00010195u, CommandOwner::device, 136u, 1u},
    {0x00010196u, CommandOwner::device, 137u, 1u},
    {0x00010197u, CommandOwner::device, 138u, 1u},
    {0x00010198u, CommandOwner::device, 139u, 1u},
    {0x00010199u, CommandOwner::device, 140u, 1u},
    {0x0001019Au, CommandOwner::device, 141u, 1u},
    {0x0001019Bu, CommandOwner::device, 142u, 1u},
    {0x0001019Cu, CommandOwner::device, 143u, 1u},
    {0x0001019Du, CommandOwner::device, 144u, 1u},
    {0x0001019Eu, CommandOwner::device, 145u, 1u},
    {0x0001019Fu, CommandOwner::device, 146u, 1u},
    {0x000101A0u, CommandOwner::device, 147u, 1u},
    {0x000101A2u, CommandOwner::device, 148u, 1u},
    {0x000101A3u, CommandOwner::device, 149u, 1u},
    {0x000101A4u, CommandOwner::device, 150u, 1u},
    {0x000101A7u, CommandOwner::device, 151u, 1u},
    {0x000101A8u, CommandOwner::device, 152u, 1u},
    {0x000101A9u, CommandOwner::device, 153u, 1u},
    {0x000101AAu, CommandOwner::device, 154u, 1u},
    {0x000101ACu, CommandOwner::device, 155u, 1u},
    {0x000101ADu, CommandOwner::device, 156u, 1u},
    {0x000101AEu, CommandOwner::device, 157u, 1u},
    {0x000101AFu, CommandOwner::device, 158u, 1u},
    {0x000101B0u, CommandOwner::device, 159u, 1u},
    {0x000101B1u, CommandOwner::device, 160u, 1u},
    {0x000101B2u, CommandOwner::device, 161u, 1u},
    {0x000101B3u, CommandOwner::device, 162u, 1u},
    {0x000101B4u, CommandOwner::device, 163u, 1u},
    {0x000101B5u, CommandOwner::device, 164u, 1u},
    {0x000101BAu, CommandOwner::device, 165u, 1u},
    {0x000101BBu, CommandOwner::device, 166u, 1u},
};

constexpr uint16_t kSlots[] = {
    30u, 49u, 30u, 49u, 30u, 49u, 29u, 45u, 4u, 4u, 3u, 8u, 18u, 8u, 18u, 18u,
    20u, 21u, 18u, 21u, 21u, 21u, 21u, 21u, 13u, 13u, 13u, 45u, 9u, 18u, 9u, 18u,
    9u, 18u, 9u, 18u, 9u, 18u, 9u, 18u, 9u, 18u, 9u, 18u, 9u, 18u, 9u, 18u,
    9u, 18u, 9u, 18u, 9u, 18u, 9u, 18u, 6u, 18u, 9u, 18u, 9u, 18u, 9u, 18u,
    37u, 9u, 18u, 12u, 9u, 18u, 9u, 18u, 9u, 18u, 9u, 18u, 9u, 18u, 9u, 18u,
    47u, 9u, 18u, 9u, 18u, 9u, 18u, 9u, 18u, 9u, 18u, 9u, 18u, 9u, 18u, 47u,
    47u, 21u, 21u, 21u, 10u, 10u, 21u, 10u, 21u, 28u, 47u, 18u, 28u, 21u, 28u, 18u,
    21u, 21u, 28u, 28u, 21u, 10u, 11u, 47u, 47u, 47u, 18u, 10u, 21u, 2u, 24u, 33u,
    33u, 42u, 44u, 44u, 42u, 42u, 44u, 44u, 42u, 44u, 19u, 19u, 19u, 19u, 19u, 19u,
    15u, 34u, 35u, 35u, 36u, 34u, 36u, 0u, 43u, 43u, 14u, 43u, 43u, 43u, 5u, 43u,
    16u, 46u, 1u, 1u, 24u, 17u, 17u,
};

constexpr const char* kDeviceNames[] = {
    "VK_AMD_anti_lag",
    "VK_AMD_buffer_marker",
    "VK_EXT_calibrated_timestamps",
    "VK_EXT_color_write_enable",
    "VK_EXT_conditional_rendering",
    "VK_EXT_depth_bias_control",
    "VK_EXT_depth_clamp_control",
    "VK_EXT_extended_dynamic_state",
    "VK_EXT_extended_dynamic_state2",
    "VK_EXT_extended_dynamic_state3",
    "VK_EXT_host_image_copy",
    "VK_EXT_image_compression_control",
    "VK_EXT_line_rasterization",
    "VK_EXT_mesh_shader",
    "VK_EXT_multi_draw",
    "VK_EXT_pageable_device_local_memory",
    "VK_EXT_sample_locations",
    "VK_EXT_shader_module_identifier",
    "VK_EXT_shader_object",
    "VK_EXT_transform_feedback",
    "VK_EXT_vertex_input_dynamic_state",
    "VK_KHR_acceleration_structure",
    "VK_KHR_bind_memory2",
    "VK_KHR_buffer_device_address",
    "VK_KHR_calibrated_timestamps",
    "VK_KHR_cooperative_matrix",
    "VK_KHR_copy_commands2",
    "VK_KHR_create_renderpass2",
    "VK_KHR_deferred_host_operations",
    "VK_KHR_descriptor_update_template",
    "VK_KHR_device_group",
    "VK_KHR_draw_indirect_count",
    "VK_KHR_dynamic_rendering",
    "VK_KHR_dynamic_rendering_local_read",
    "VK_KHR_external_fence_fd",
    "VK_KHR_external_memory_fd",
    "VK_KHR_external_semaphore_fd",
    "VK_KHR_fragment_shading_rate",
    "VK_KHR_get_memory_requirements2",
    "VK_KHR_maintenance1",
    "VK_KHR_maintenance3",
    "VK_KHR_maintenance4",
    "VK_KHR_maintenance5",
    "VK_KHR_maintenance6",
    "VK_KHR_pipeline_binary",
    "VK_KHR_push_descriptor",
    "VK_KHR_ray_tracing_maintenance1",
    "VK_KHR_ray_tracing_pipeline",
    "VK_KHR_sampler_ycbcr_conversion",
    "VK_KHR_swapchain",
    "VK_KHR_synchronization2",
    "VK_KHR_timeline_semaphore",
};

constexpr const char* kInstanceNames[] = {
    "VK_KHR_device_group_creation",
    "VK_KHR_external_fence_capabilities",
    "VK_KHR_external_memory_capabilities",
    "VK_KHR_external_semaphore_capabilities",
    "VK_KHR_get_physical_device_properties2",
};

uint16_t slot_of(const char* const* names, uint32_t count, const char* name) noexcept {
    for (uint32_t i = 0; i < count; ++i) {
        if (__builtin_strcmp(names[i], name) == 0) { return static_cast<uint16_t>(i); }
    }
    return kNoExtensionSlot;
}

}  // namespace

CommandExtensions command_extensions(uint32_t cmd_id) noexcept {
    uint32_t low = 0;
    uint32_t high = static_cast<uint32_t>(sizeof(kRows) / sizeof(kRows[0]));
    while (low < high) {
        const uint32_t mid = low + (high - low) / 2u;
        if (kRows[mid].id == cmd_id) {
            return CommandExtensions{kRows[mid].owner, kRows[mid].count, kSlots + kRows[mid].first};
        }
        if (kRows[mid].id < cmd_id) { low = mid + 1u; } else { high = mid; }
    }
    return CommandExtensions{CommandOwner::open, 0u, nullptr};
}

uint16_t device_extension_slot(const char* name) noexcept {
    return slot_of(kDeviceNames, kDeviceExtensionSlots, name);
}

uint16_t instance_extension_slot(const char* name) noexcept {
    return slot_of(kInstanceNames, kInstanceExtensionSlots, name);
}

}  // namespace av::vkproto
