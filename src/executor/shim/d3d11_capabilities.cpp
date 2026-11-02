#include "executor/shim/d3d11_capabilities.hpp"

#include "core/trace/function_scope.hpp"
#include "executor/shim/d3d_headers.hpp"
#include "proto/d3d/dxgi_format.hpp"

namespace av {

namespace {
constexpr uint32_t sample_count_steps = 6u;

}

D3d11Capabilities::D3d11Capabilities() : caps_{}, formats_()
{
    AV_SCOPE("D3d11Capabilities::D3d11Capabilities");
}

void D3d11Capabilities::measure(ID3D11Device5 &device, uint32_t feature_level)
{
    AV_SCOPE("D3d11Capabilities::measure");
    caps_ = d3dproto::Caps11{};
    caps_.feature_level = feature_level;
    measure_options(device);
    measure_formats(device);
}

void D3d11Capabilities::measure_options(ID3D11Device5 &device)
{
    AV_SCOPE("D3d11Capabilities::measure_options");
    D3D11_FEATURE_DATA_THREADING threading = {};
    if (SUCCEEDED(device.CheckFeatureSupport(D3D11_FEATURE_THREADING, &threading, sizeof(threading)))) {
        caps_.driver_concurrent_creates = static_cast<uint32_t>(threading.DriverConcurrentCreates);
        caps_.driver_command_lists = 0u;
    }
    D3D11_FEATURE_DATA_DOUBLES doubles = {};
    if (SUCCEEDED(device.CheckFeatureSupport(D3D11_FEATURE_DOUBLES, &doubles, sizeof(doubles))))
        caps_.double_precision_float_shader_ops = static_cast<uint32_t>(doubles.DoublePrecisionFloatShaderOps);
    D3D11_FEATURE_DATA_D3D10_X_HARDWARE_OPTIONS x_options = {};
    if (SUCCEEDED(device.CheckFeatureSupport(D3D11_FEATURE_D3D10_X_HARDWARE_OPTIONS, &x_options, sizeof(x_options))))
        caps_.compute_shaders_on_10x = static_cast<uint32_t>(x_options.ComputeShaders_Plus_RawAndStructuredBuffers_Via_Shader_4_x);
    D3D11_FEATURE_DATA_D3D11_OPTIONS options = {};
    if (SUCCEEDED(device.CheckFeatureSupport(D3D11_FEATURE_D3D11_OPTIONS, &options, sizeof(options)))) {
        caps_.output_merger_logic_op = static_cast<uint32_t>(options.OutputMergerLogicOp);
        caps_.uav_only_rendering_forced_sample_count = static_cast<uint32_t>(options.UAVOnlyRenderingForcedSampleCount);
        caps_.discard_apis_seen_by_driver = static_cast<uint32_t>(options.DiscardAPIsSeenByDriver);
        caps_.flags_for_update_and_copy_seen_by_driver = static_cast<uint32_t>(options.FlagsForUpdateAndCopySeenByDriver);
        caps_.clear_view = static_cast<uint32_t>(options.ClearView);
        caps_.copy_with_overlap = static_cast<uint32_t>(options.CopyWithOverlap);
        caps_.constant_buffer_partial_update = static_cast<uint32_t>(options.ConstantBufferPartialUpdate);
        caps_.constant_buffer_offsetting = static_cast<uint32_t>(options.ConstantBufferOffsetting);
        caps_.map_no_overwrite_on_dynamic_constant_buffer = static_cast<uint32_t>(options.MapNoOverwriteOnDynamicConstantBuffer);
        caps_.map_no_overwrite_on_dynamic_buffer_srv = static_cast<uint32_t>(options.MapNoOverwriteOnDynamicBufferSRV);
        caps_.multisample_rtv_with_forced_sample_count_one = static_cast<uint32_t>(options.MultisampleRTVWithForcedSampleCountOne);
        caps_.sad4_shader_instructions = static_cast<uint32_t>(options.SAD4ShaderInstructions);
        caps_.extended_doubles_shader_instructions = static_cast<uint32_t>(options.ExtendedDoublesShaderInstructions);
        caps_.extended_resource_sharing = static_cast<uint32_t>(options.ExtendedResourceSharing);
    }
    D3D11_FEATURE_DATA_ARCHITECTURE_INFO architecture = {};
    if (SUCCEEDED(device.CheckFeatureSupport(D3D11_FEATURE_ARCHITECTURE_INFO, &architecture, sizeof(architecture))))
        caps_.tile_based_deferred_renderer = static_cast<uint32_t>(architecture.TileBasedDeferredRenderer);
    D3D11_FEATURE_DATA_SHADER_MIN_PRECISION_SUPPORT precision = {};
    if (SUCCEEDED(device.CheckFeatureSupport(D3D11_FEATURE_SHADER_MIN_PRECISION_SUPPORT, &precision,
                                             sizeof(precision)))) {
        caps_.pixel_shader_min_precision = static_cast<uint32_t>(precision.PixelShaderMinPrecision);
        caps_.all_other_shader_stages_min_precision = static_cast<uint32_t>(precision.AllOtherShaderStagesMinPrecision);
    }
    D3D11_FEATURE_DATA_D3D11_OPTIONS2 options2 = {};
    if (SUCCEEDED(device.CheckFeatureSupport(D3D11_FEATURE_D3D11_OPTIONS2, &options2, sizeof(options2)))) {
        caps_.ps_specified_stencil_ref = static_cast<uint32_t>(options2.PSSpecifiedStencilRefSupported);
        caps_.typed_uav_load_additional_formats = static_cast<uint32_t>(options2.TypedUAVLoadAdditionalFormats);
        caps_.rovs = static_cast<uint32_t>(options2.ROVsSupported);
        caps_.conservative_rasterization_tier = static_cast<uint32_t>(options2.ConservativeRasterizationTier);
        caps_.tiled_resources_tier = 0u;
        caps_.map_on_default_textures = 0u;
        caps_.standard_swizzle = 0u;
        caps_.unified_memory_architecture = 0u;
    }
    D3D11_FEATURE_DATA_D3D11_OPTIONS3 options3 = {};
    if (SUCCEEDED(device.CheckFeatureSupport(D3D11_FEATURE_D3D11_OPTIONS3, &options3, sizeof(options3))))
        caps_.vp_and_rt_array_index_from_any_shader = static_cast<uint32_t>(options3.VPAndRTArrayIndexFromAnyShaderFeedingRasterizer);
    caps_.extended_nv12_shared_texture = 0u;
    caps_.shared_resource_tier = 0u;
}

void D3d11Capabilities::measure_formats(ID3D11Device5 &device)
{
    AV_SCOPE("D3d11Capabilities::measure_formats");
    constexpr uint32_t multisample_counts = sizeof(d3dproto::FormatCaps::quality_levels) / sizeof(uint32_t);
    for (uint32_t format = 0u; format < DxgiFormat::count; format++) {
        d3dproto::FormatCaps row = {};
        UINT support = 0u;
        if (SUCCEEDED(device.CheckFormatSupport(static_cast<DXGI_FORMAT>(format), &support))) row.support = support;
        D3D11_FEATURE_DATA_FORMAT_SUPPORT2 support2 = {};
        support2.InFormat = static_cast<DXGI_FORMAT>(format);
        if (SUCCEEDED(device.CheckFeatureSupport(D3D11_FEATURE_FORMAT_SUPPORT2, &support2, sizeof(support2))))
            row.support2 = support2.OutFormatSupport2;
        uint32_t counts = 0u, most = 0u;
        for (uint32_t step = 0u; step < sample_count_steps; step++) {
            UINT levels = 0u;
            if (FAILED(device.CheckMultisampleQualityLevels(static_cast<DXGI_FORMAT>(format), 1u << step, &levels)) ||
                levels == 0u)
                continue;
            counts |= 1u << step;
            if (levels > most) most = levels;
        }
        for (uint32_t step = 0u; step < multisample_counts; step++)
            row.quality_levels[step] = (counts & (2u << step)) != 0u ? most : 0u;
        if ((counts & ~1u) == 0u)
            row.support &= ~static_cast<uint32_t>(D3D11_FORMAT_SUPPORT_MULTISAMPLE_RESOLVE |
                                                  D3D11_FORMAT_SUPPORT_MULTISAMPLE_RENDERTARGET |
                                                  D3D11_FORMAT_SUPPORT_MULTISAMPLE_LOAD);
        formats_.answer(format, row);
    }
}

const d3dproto::Caps11 &D3d11Capabilities::caps() const
{
    AV_SCOPE("D3d11Capabilities::caps");
    return caps_;
}

const FormatCapsRows &D3d11Capabilities::formats() const
{
    AV_SCOPE("D3d11Capabilities::formats");
    return formats_;
}

}
