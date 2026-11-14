#include "executor/shim/d3d12_capabilities.hpp"

#include "core/trace/function_scope.hpp"
#include "executor/shim/d3d_headers.hpp"
#include "proto/d3d/dxgi_format.hpp"

namespace av {

namespace {

constexpr uint32_t level_12_2 = 0xc200u;
constexpr uint32_t shader_model_6_6 = 0x66u;
constexpr uint32_t descriptor_heap_types = 4u;

}

D3d12Capabilities::D3d12Capabilities() : caps_{}, formats_()
{
    AV_SCOPE("D3d12Capabilities::D3d12Capabilities");
}

void D3d12Capabilities::measure(ID3D12Device &device)
{
    AV_SCOPE("D3d12Capabilities::measure");
    caps_ = d3d12proto::Caps12{};
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
    model.HighestShaderModel = static_cast<D3D_SHADER_MODEL>(shader_model_6_6);
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
