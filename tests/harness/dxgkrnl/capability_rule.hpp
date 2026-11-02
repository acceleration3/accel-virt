#ifndef AV_CAPABILITY_RULE_HPP
#define AV_CAPABILITY_RULE_HPP

namespace av {

enum class CapabilityRule : unsigned {
    none,
    driver_caps_query_failed,
    multi_engine_aware_missing,
    too_many_nodes,
    frame_buffer_save_size,
    misc_caps_reserved,
    vsync_control_without_control_interrupt3,
    unknown_wddm_version,
    wddm_1_2_feature_below_1_2,
    wddm_1_3_feature_below_1_3,
    hybrid_integrated_below_2_0,
    cross_adapter_tiers,
    hybrid_without_cross_adapter_resource,
    both_hybrid,
    hybrid_integrated_without_outputs,
    per_engine_tdr_without_ddis,
    kernel_mode_command_buffer_without_render_km,
    runtime_power_without_ddis,
    overlay_without_planes,
    too_many_overlay_planes,
    overlay_without_ddis,
    overlay_without_direct_flip,
    color_transform_levels,
    color_transform_without_target_gamma,
    diagnostic_ddis_unpaired,
    native_fence_caps_query_failed,
    wddm_1_2_full_adapter_without_direct_flip,
    wddm_1_3_full_adapter_without_independent_flip,
    render_ddis_incomplete,
    wddm_2_without_flip_on_vsync_mmio,
    preemption_without_multi_engine,
    multi_engine_without_nodes,
    swizzling_without_ddis,
    wddm_3_2_display_without_monitor_link_info,
};

}

#endif
