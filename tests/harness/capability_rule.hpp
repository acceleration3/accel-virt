/* Why FakeDxgadapter refused an adapter's capabilities - one value per check
 * DXGADAPTER::Initialize @ 1401a2818 applies, in the order it applies them
 * (docs/ddi-sequences.md, section 2.4). */
#ifndef AV_CAPABILITY_RULE_HPP
#define AV_CAPABILITY_RULE_HPP

namespace av {

enum class CapabilityRule : unsigned {
    none,
    driver_caps_query_failed,              /* "Miniport failed DdiQueryAdapterInfo(DXGKQAITYPE_DRIVERCAPS)" */
    multi_engine_aware_missing,            /* "SchedulingCaps.MultiEngineAware is not set by WDDMv2 driver" */
    too_many_nodes,                        /* "Exceeded maximum number of 64 nodes on pre-WDDM 2.5 adapter" */
    frame_buffer_save_size,                /* "Failed to query frame buffer save area size" / not a page multiple */
    misc_caps_reserved,                    /* "Driver should not set MiscCaps.Reserved bits" */
    vsync_control_without_control_interrupt3,
    unknown_wddm_version,                  /* "Miniport returned unknown WDDM version" */
    wddm_1_2_feature_below_1_2,
    wddm_1_3_feature_below_1_3,
    hybrid_integrated_below_2_0,
    cross_adapter_tiers,                   /* "CrossAdapterResource{Texture,Scanout} ... lower tier support" */
    hybrid_without_cross_adapter_resource,
    both_hybrid,
    hybrid_integrated_without_outputs,
    per_engine_tdr_without_ddis,
    kernel_mode_command_buffer_without_render_km,
    runtime_power_without_ddis,
    overlay_without_planes,                /* "SupportMultiPlaneOverlay ... doesn't report any overlay planes" */
    too_many_overlay_planes,               /* "Driver reports more than the supported number of overlay planes" */
    overlay_without_ddis,
    overlay_without_direct_flip,
    color_transform_levels,                /* "support higher level of colorSpaceTransform but not lower levels" */
    color_transform_without_target_gamma,
    diagnostic_ddis_unpaired,              /* "only one of DdiQueryDiagnosticTypesSupport and DdiControlDiagnosticReporting" */
    native_fence_caps_query_failed,        /* "Failed to get DXGK_NATIVE_FENCE_CAPS" */
    wddm_1_2_full_adapter_without_direct_flip,
    wddm_1_3_full_adapter_without_independent_flip,
    render_ddis_incomplete,                /* "Not all the render relevant DDIs are NULL" / "did not provide required DDIs" */
    preemption_without_multi_engine,       /* CreateRenderCore: PreemptionAware or CancelCommandAware alone */
    multi_engine_without_nodes,            /* CreateRenderCore: MultiEngineAware with no asymmetric node */
    swizzling_without_ddis,                /* NumberOfSwizzlingRanges without Acquire/ReleaseSwizzlingRange */
};

}  // namespace av

#endif  // AV_CAPABILITY_RULE_HPP
