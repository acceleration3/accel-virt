#include "executor/shim/d3d12_capabilities.hpp"

#include "core/trace/function_scope.hpp"
#include "executor/shim/d3d12_newer_interfaces.hpp"
#include "executor/shim/d3d_headers.hpp"
#include "proto/d3d/caps12_layout.hpp"
#include "proto/d3d/dxgi_format.hpp"

namespace av {

namespace {

constexpr uint32_t level_12_2 = 0xc200u;
constexpr uint32_t shader_model_6_8 = 0x68u;
constexpr int      feature_options9 = 37;
constexpr int      feature_options11 = 40;
constexpr int      feature_options14 = 43;
constexpr int      feature_options21 = 53;
constexpr uint32_t descriptor_heap_types = 4u;
constexpr uint32_t queue_list_direct = 1u;
constexpr uint32_t queue_list_compute = 4u;
constexpr uint32_t queue_list_copy = 8u;
constexpr uint32_t queue_3d = 1u;
constexpr uint32_t queue_compute = 2u;
constexpr uint32_t queue_copy = 4u;

struct Options9 {
    BOOL mesh_shader_pipeline_stats;
    BOOL mesh_shader_full_range_render_target_array_index;
    BOOL atomic_int64_on_typed_resource;
    BOOL atomic_int64_on_group_shared;
    BOOL derivatives_in_mesh_and_amplification_shaders;
    int  wave_mma_tier;
};

struct Options11 {
    BOOL atomic_int64_on_descriptor_heap_resource;
};

struct Options14 {
    BOOL advanced_texture_ops;
    BOOL writeable_msaa_textures;
    BOOL independent_front_and_back_stencil_ref_mask;
};

struct Options21 {
    int  work_graphs_tier;
    int  execute_indirect_tier;
    BOOL sample_cmp_gradient_and_bias;
    BOOL extended_command_info;
};

template <typename Options>
bool ask(ID3D12Device &device, int feature, Options &answer)
{
    AV_SCOPE("ask");
    answer = Options{};
    return SUCCEEDED(device.CheckFeatureSupport(static_cast<D3D12_FEATURE>(feature), &answer, sizeof(answer)));
}

}

D3d12Capabilities::D3d12Capabilities() : caps_{}, formats_()
{
    AV_SCOPE("D3d12Capabilities::D3d12Capabilities");
}

void D3d12Capabilities::measure(ID3D12Device &device)
{
    AV_SCOPE("D3d12Capabilities::measure");
    caps_ = d3d12proto::Caps12{};
    caps_.layout = caps12_layout;
    measure_options(device);
    measure_formats(device);
}

void D3d12Capabilities::measure_options(ID3D12Device &device)
{
    AV_SCOPE("D3d12Capabilities::measure_options");
    const D3D_FEATURE_LEVEL asked[] = {static_cast<D3D_FEATURE_LEVEL>(level_12_2), D3D_FEATURE_LEVEL_12_1,
                                       D3D_FEATURE_LEVEL_12_0, D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0};
    D3D12_FEATURE_DATA_FEATURE_LEVELS levels = {};
    levels.NumFeatureLevels = static_cast<UINT>(sizeof(asked) / sizeof(asked[0]));
    levels.pFeatureLevelsRequested = asked;
    if (SUCCEEDED(device.CheckFeatureSupport(D3D12_FEATURE_FEATURE_LEVELS, &levels, sizeof(levels))))
        caps_.feature_level = static_cast<uint32_t>(levels.MaxSupportedFeatureLevel);
    D3D12_FEATURE_DATA_SHADER_MODEL model = {};
    model.HighestShaderModel = static_cast<D3D_SHADER_MODEL>(shader_model_6_8);
    if (SUCCEEDED(device.CheckFeatureSupport(D3D12_FEATURE_SHADER_MODEL, &model, sizeof(model))))
        caps_.shader_model = static_cast<uint32_t>(model.HighestShaderModel);
    D3D12_FEATURE_DATA_D3D12_OPTIONS options = {};
    if (SUCCEEDED(device.CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS, &options, sizeof(options)))) {
        caps_.resource_binding_tier = static_cast<uint32_t>(options.ResourceBindingTier);
        uint32_t tiled_tier = 0u;
        __builtin_memcpy(&tiled_tier, &options.TiledResourcesTier, sizeof tiled_tier);
        caps_.tiled_resources_tier = tiled_tier;
        caps_.resource_heap_tier = static_cast<uint32_t>(options.ResourceHeapTier);
        caps_.conservative_rasterization_tier = static_cast<uint32_t>(options.ConservativeRasterizationTier);
        caps_.typed_uav_load_additional_formats = options.TypedUAVLoadAdditionalFormats ? 1u : 0u;
        caps_.rovs = options.ROVsSupported ? 1u : 0u;
        caps_.double_ops = options.DoublePrecisionFloatShaderOps ? 1u : 0u;
        caps_.output_merger_logic_op = options.OutputMergerLogicOp ? 1u : 0u;
        caps_.min_precision = static_cast<uint32_t>(options.MinPrecisionSupport);
        caps_.ps_stencil_ref = options.PSSpecifiedStencilRefSupported ? 1u : 0u;
        caps_.vp_rt_array_without_gs =
            options.VPAndRTArrayIndexFromAnyShaderFeedingRasterizerSupportedWithoutGSEmulation ? 1u : 0u;
    }
    D3D12_FEATURE_DATA_D3D12_OPTIONS3 options3 = {};
    if (SUCCEEDED(device.CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS3, &options3, sizeof(options3)))) {
        caps_.barycentrics = options3.BarycentricsSupported ? 1u : 0u;
        caps_.view_instancing_tier = static_cast<uint32_t>(options3.ViewInstancingTier);
        const uint32_t lists = static_cast<uint32_t>(options3.WriteBufferImmediateSupportFlags);
        caps_.write_immediate_queues = ((lists & queue_list_direct) != 0u ? queue_3d : 0u) |
                                       ((lists & queue_list_compute) != 0u ? queue_compute : 0u) |
                                       ((lists & queue_list_copy) != 0u ? queue_copy : 0u);
    }
    D3D12_FEATURE_DATA_D3D12_OPTIONS2 options2 = {};
    if (SUCCEEDED(device.CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS2, &options2, sizeof(options2))))
        caps_.depth_bounds = options2.DepthBoundsTestSupported ? 1u : 0u;
    d3d12new::Options6 options6 = {};
    if (SUCCEEDED(device.CheckFeatureSupport(static_cast<D3D12_FEATURE>(d3d12new::feature_options6), &options6, sizeof(options6)))) {
        caps_.vrs_tier = static_cast<uint32_t>(options6.variable_shading_rate_tier);
        caps_.vrs_additional_rates = options6.additional_shading_rates ? 1u : 0u;
        caps_.vrs_tile_size = options6.shading_rate_image_tile_size;
        caps_.vrs_per_primitive_viewport = options6.per_primitive_shading_rate_with_viewport_indexing ? 1u : 0u;
    }
    d3d12new::Options12 options12 = {};
    if (SUCCEEDED(device.CheckFeatureSupport(static_cast<D3D12_FEATURE>(d3d12new::feature_options12), &options12, sizeof(options12)))) {
        caps_.enhanced_barriers = options12.enhanced_barriers ? 1u : 0u;
        caps_.relaxed_format_casting = options12.relaxed_format_casting ? 1u : 0u;
    }
    d3d12new::Options10 options10 = {};
    if (SUCCEEDED(device.CheckFeatureSupport(static_cast<D3D12_FEATURE>(d3d12new::feature_options10), &options10, sizeof(options10))))
        caps_.vrs_sum_combiner = options10.sum_combiner ? 1u : 0u;
    d3d12new::Options7 options7 = {};
    if (SUCCEEDED(device.CheckFeatureSupport(static_cast<D3D12_FEATURE>(d3d12new::feature_options7), &options7, sizeof(options7))))
        caps_.mesh_shader_tier = static_cast<uint32_t>(options7.mesh_shader_tier);
    D3D12_FEATURE_DATA_D3D12_OPTIONS4 options4 = {};
    if (SUCCEEDED(device.CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS4, &options4, sizeof(options4))))
        caps_.native_16bit_ops = options4.Native16BitShaderOpsSupported ? 1u : 0u;
    Options9 options9;
    if (ask(device, feature_options9, options9)) {
        caps_.atomic_int64_typed = options9.atomic_int64_on_typed_resource ? 1u : 0u;
        caps_.atomic_int64_group_shared = options9.atomic_int64_on_group_shared ? 1u : 0u;
    }
    Options11 options11;
    if (ask(device, feature_options11, options11))
        caps_.atomic_int64_heap = options11.atomic_int64_on_descriptor_heap_resource ? 1u : 0u;
    Options14 options14;
    if (ask(device, feature_options14, options14)) {
        caps_.advanced_texture_ops = options14.advanced_texture_ops ? 1u : 0u;
        caps_.writeable_msaa_textures = options14.writeable_msaa_textures ? 1u : 0u;
    }
    Options21 options21;
    if (ask(device, feature_options21, options21)) {
        caps_.sample_cmp_gradient_bias = options21.sample_cmp_gradient_and_bias ? 1u : 0u;
        caps_.extended_command_info = options21.extended_command_info ? 1u : 0u;
    }
    D3D12_FEATURE_DATA_D3D12_OPTIONS1 waves = {};
    if (SUCCEEDED(device.CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS1, &waves, sizeof(waves)))) {
        caps_.wave_ops = waves.WaveOps ? 1u : 0u;
        caps_.wave_lane_min = waves.WaveLaneCountMin;
        caps_.wave_lane_max = waves.WaveLaneCountMax;
        caps_.total_lanes = waves.TotalLaneCount;
        caps_.int64_ops = waves.Int64ShaderOps ? 1u : 0u;
    }
    D3D12_FEATURE_DATA_GPU_VIRTUAL_ADDRESS_SUPPORT address = {};
    if (SUCCEEDED(device.CheckFeatureSupport(D3D12_FEATURE_GPU_VIRTUAL_ADDRESS_SUPPORT, &address, sizeof(address)))) {
        caps_.max_gpu_va_bits_per_resource = address.MaxGPUVirtualAddressBitsPerResource;
        caps_.max_gpu_va_bits_per_process = address.MaxGPUVirtualAddressBitsPerProcess;
    }
    for (uint32_t type = 0u; type < descriptor_heap_types; type++)
        caps_.descriptor_sizes[type] =
            device.GetDescriptorHandleIncrementSize(static_cast<D3D12_DESCRIPTOR_HEAP_TYPE>(type));
}

void D3d12Capabilities::measure_formats(ID3D12Device &device)
{
    AV_SCOPE("D3d12Capabilities::measure_formats");
    constexpr uint32_t multisample_counts = sizeof(d3dproto::FormatCaps::quality_levels) / sizeof(uint32_t);
    for (uint32_t format = 1u; format < DxgiFormat::count; format++) {
        d3dproto::FormatCaps              row = {};
        D3D12_FEATURE_DATA_FORMAT_SUPPORT support = {};
        support.Format = static_cast<DXGI_FORMAT>(format);
        if (SUCCEEDED(device.CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT, &support, sizeof(support)))) {
            __builtin_memcpy(&row.support, &support.Support1, sizeof row.support);
            __builtin_memcpy(&row.support2, &support.Support2, sizeof row.support2);
        }
        const uint32_t asked =
            (row.support & D3D12_FORMAT_SUPPORT1_MULTISAMPLE_RENDERTARGET) != 0u ? multisample_counts : 0u;
        for (uint32_t step = 0u; step < asked; step++) {
            D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS levels = {};
            levels.Format = support.Format;
            levels.SampleCount = 2u << step;
            if (SUCCEEDED(device.CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &levels,
                                                     sizeof(levels))))
                row.quality_levels[step] = levels.NumQualityLevels;
        }
        formats_.answer(format, row);
    }
}

const d3d12proto::Caps12 &D3d12Capabilities::caps() const
{
    AV_SCOPE("D3d12Capabilities::caps");
    return caps_;
}

const FormatCapsRows &D3d12Capabilities::formats() const
{
    AV_SCOPE("D3d12Capabilities::formats");
    return formats_;
}

}
