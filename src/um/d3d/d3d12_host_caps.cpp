#include "um/d3d/d3d12_host_caps.hpp"

#include "core/trace/function_scope.hpp"
#include "icd/common/adapter_memory.hpp"
#include "icd/d3d/caps_reply.hpp"
#include "proto/client/client_transport.hpp"
#include "proto/d3d/caps12_layout.hpp"
#include "proto/generated/d3d/d3d_wire_types.h"
#include "proto/generated/d3d12/d3d12_dispatch_client.h"

namespace av::d3d12 {

namespace {

constexpr uint32_t level_11_0 = 0xb000u;
constexpr uint32_t level_11_1 = 0xb100u;
constexpr uint32_t level_12_0 = 0xc000u;
constexpr uint32_t level_12_1 = 0xc100u;
constexpr uint32_t binding_tier_12_0 = 2u;
constexpr uint32_t tiled_tier_12_0 = 2u;
constexpr uint32_t most_tiled_tier = 2u;
constexpr uint32_t most_view_instancing_tier = 3u;
constexpr uint32_t most_vrs_tier = 2u;
constexpr uint32_t mesh_tier_1 = 10u;
constexpr uint32_t api_model_6_0 = 0x60u;
constexpr uint32_t api_model_6_8 = 0x68u;
constexpr uint32_t ddi_model_5_1 = 0x50015u;
constexpr uint32_t ddi_model_6_0 = 0x60005u;
constexpr uint32_t ddi_model_step = 0x10u;
constexpr uint32_t address_bits = 40u;
constexpr UINT16   row_major_element = 16u;
constexpr UINT16   row_major_alignment = 256u;
constexpr UINT     view_heap_ceiling = 1000000u;
constexpr UINT     sampler_heap_ceiling = 4000u;
constexpr UINT     no_output_sample_counts = 0x1fu;

}

D3d12HostCaps::D3d12HostCaps() : caps_{}, formats_(), local_budget_(0u), learned_(false)
{
    AV_SCOPE("D3d12HostCaps::D3d12HostCaps");
}

bool D3d12HostCaps::learn(ClientTransport &transport, Kmt &kmt, uint32_t adapter)
{
    AV_SCOPE("D3d12HostCaps::learn");
    if (learned_) return true;
    uint32_t             hresult = 1u;
    uint64_t             answered = 0u;
    d3dproto::FormatCaps rows[DxgiFormat::count] = {};
    if (!d3d12proto::av_d3d12_QueryCaps12(transport, &hresult, &caps_, rows, sizeof(rows), &answered) ||
        hresult != 0u)
        return false;
    if (caps_.layout != caps12_layout) return false;
    formats_.learn(rows, static_cast<uint32_t>(answered / sizeof(rows[0])));
    icd::AdapterMemory memory(kmt, adapter);
    local_budget_ = memory.read() == Status::ok ? memory.budget(MemorySegmentGroup::local) : 0u;
    learned_ = true;
    return true;
}

uint64_t D3d12HostCaps::local_budget() const
{
    AV_SCOPE("D3d12HostCaps::local_budget");
    return local_budget_;
}

UINT D3d12HostCaps::quality_levels(DXGI_FORMAT format, UINT samples) const
{
    AV_SCOPE("D3d12HostCaps::quality_levels");
    return formats_.quality_levels(static_cast<uint32_t>(format), samples);
}

bool D3d12HostCaps::learned() const
{
    AV_SCOPE("D3d12HostCaps::learned");
    return learned_;
}

UINT D3d12HostCaps::format_support(DXGI_FORMAT format) const
{
    AV_SCOPE("D3d12HostCaps::format_support");
    const uint32_t asked = static_cast<uint32_t>(format);
    return formats_.format_support(asked) | (tiled_tier() != 0u ? formats_.tiled_support(asked) : 0u);
}

uint32_t D3d12HostCaps::pipeline_level() const
{
    AV_SCOPE("D3d12HostCaps::pipeline_level");
    if (caps_.feature_level >= level_12_1 && caps_.conservative_rasterization_tier >= 1u && caps_.rovs != 0u &&
        caps_.resource_binding_tier >= binding_tier_12_0 && tiled_tier() >= tiled_tier_12_0 &&
        caps_.typed_uav_load_additional_formats != 0u && caps_.vp_rt_array_without_gs != 0u)
        return D3D12DDI_3DPIPELINELEVEL_12_1;
    if (caps_.feature_level >= level_12_0 && caps_.resource_binding_tier >= binding_tier_12_0 &&
        tiled_tier() >= tiled_tier_12_0 && caps_.typed_uav_load_additional_formats != 0u)
        return D3D12DDI_3DPIPELINELEVEL_12_0;
    if (caps_.feature_level >= level_11_1) return D3D12DDI_3DPIPELINELEVEL_11_1;
    if (caps_.feature_level >= level_11_0) return D3D12DDI_3DPIPELINELEVEL_11_0;
    return D3D12DDI_3DPIPELINELEVEL_1_0_CORE;
}

uint32_t D3d12HostCaps::tiled_tier() const
{
    AV_SCOPE("D3d12HostCaps::tiled_tier");
    return caps_.tiled_resources_tier < most_tiled_tier ? caps_.tiled_resources_tier : most_tiled_tier;
}

HRESULT D3d12HostCaps::fill(void *data, UINT bytes, const void *answer, UINT answer_bytes) const
{
    AV_SCOPE("D3d12HostCaps::fill");
    return d3d::CapsReply(data, bytes).give_prefix(answer, answer_bytes) ? S_OK : E_INVALIDARG;
}

HRESULT D3d12HostCaps::options(void *data, UINT bytes) const
{
    AV_SCOPE("D3d12HostCaps::options");
    D3D12DDI_D3D12_OPTIONS_DATA_0089 answer = {};
    answer.ResourceBindingTier = static_cast<D3D12DDI_RESOURCE_BINDING_TIER>(caps_.resource_binding_tier);
    answer.ConservativeRasterizationTier =
        static_cast<D3D12DDI_CONSERVATIVE_RASTERIZATION_TIER>(caps_.conservative_rasterization_tier);
    answer.VPAndRTArrayIndexFromAnyShaderFeedingRasterizerSupportedWithoutGSEmulation = caps_.vp_rt_array_without_gs != 0u;
    answer.BarycentricsSupported = caps_.barycentrics != 0u;
    answer.ResourceHeapTier = static_cast<D3D12DDI_RESOURCE_HEAP_TIER>(caps_.resource_heap_tier);
    answer.TiledResourcesTier = static_cast<D3D12DDI_TILED_RESOURCES_TIER>(tiled_tier());
    answer.CrossNodeSharingTier = D3D12DDI_CROSS_NODE_SHARING_TIER_NOT_SUPPORTED;
    answer.OutputMergerLogicOp = caps_.output_merger_logic_op != 0u;
    answer.DepthBoundsTestSupported = caps_.depth_bounds != 0u;
    answer.ViewInstancingTier = static_cast<D3D12DDI_VIEW_INSTANCING_TIER>(
        caps_.view_instancing_tier < most_view_instancing_tier ? caps_.view_instancing_tier : most_view_instancing_tier);
    const uint32_t shading_tier = caps_.vrs_tier < most_vrs_tier ? caps_.vrs_tier : most_vrs_tier;
    answer.VariableShadingRateTier = static_cast<D3D12DDI_VARIABLE_SHADING_RATE_TIER>(shading_tier);
    answer.AdditionalShadingRatesSupported = shading_tier >= 2u && caps_.vrs_additional_rates != 0u;
    answer.PerPrimitiveShadingRateSupportedWithViewportIndexing =
        shading_tier >= 2u && caps_.vrs_per_primitive_viewport != 0u;
    answer.ShadingRateImageTileSize = shading_tier >= 2u ? caps_.vrs_tile_size : 0u;
    answer.VariableRateShadingSumCombinerSupported = shading_tier >= 2u && caps_.vrs_sum_combiner != 0u;
    answer.WriteBufferImmediateQueueFlags = static_cast<D3D12DDI_COMMAND_QUEUE_FLAGS>(caps_.write_immediate_queues);
    answer.CopyQueueTimestampQueriesSupported = caps_.copy_queue_timestamps != 0u;
    answer.EnhancedBarriersSupported = caps_.enhanced_barriers != 0u;
    answer.MeshShaderTier = caps_.mesh_shader_tier >= mesh_tier_1 ? D3D12DDI_MESH_SHADER_TIER_1
                                                                  : D3D12DDI_MESH_SHADER_TIER_NOT_SUPPORTED;
    return fill(data, bytes, &answer, sizeof(answer));
}

HRESULT D3d12HostCaps::shader(void *data, UINT bytes) const
{
    AV_SCOPE("D3d12HostCaps::shader");
    D3D12DDI_SHADER_CAPS_0084 answer = {};
    answer.MinPrecision = static_cast<D3D12DDI_SHADER_MIN_PRECISION>(caps_.min_precision);
    answer.ShaderSpecifiedStencilRef = caps_.ps_stencil_ref != 0u;
    answer.ROVs = caps_.rovs != 0u;
    answer.Native16BitOps = caps_.native_16bit_ops != 0u;
    answer.AtomicInt64OnTypedResource = false;
    answer.AtomicInt64OnGroupShared = caps_.atomic_int64_group_shared != 0u;
    answer.DoubleOps = caps_.double_ops != 0u;
    answer.TypedUAVLoadAdditionalFormats = caps_.typed_uav_load_additional_formats != 0u;
    answer.WaveOps = caps_.wave_ops != 0u;
    answer.WaveLaneCountMin = caps_.wave_lane_min;
    answer.WaveLaneCountMax = caps_.wave_lane_max;
    answer.TotalLaneCount = caps_.total_lanes;
    answer.Int64Ops = caps_.int64_ops != 0u;
    return fill(data, bytes, &answer, sizeof(answer));
}

HRESULT D3d12HostCaps::shader_models(void *data, UINT bytes) const
{
    AV_SCOPE("D3d12HostCaps::shader_models");
    if (data == nullptr || bytes < sizeof(D3D12DDI_D3D12_SHADER_MODELS_DATA_0011)) return E_INVALIDARG;
    const auto &asked = *static_cast<const D3D12DDI_D3D12_SHADER_MODELS_DATA_0011 *>(data);
    if (asked.pNumShaderModelsSupported == nullptr) return E_INVALIDARG;
    const uint32_t highest = caps_.shader_model < api_model_6_8 ? caps_.shader_model : api_model_6_8;
    const UINT     models = highest >= api_model_6_0 ? 2u + (highest - api_model_6_0) : 1u;
    if (asked.pShaderModelsSupported == nullptr) {
        *asked.pNumShaderModelsSupported = models;
        return S_OK;
    }
    if (*asked.pNumShaderModelsSupported < models) return E_INVALIDARG;
    asked.pShaderModelsSupported[0] = static_cast<D3D12DDI_SHADER_MODEL>(ddi_model_5_1);
    for (UINT index = 1u; index < models; index++)
        asked.pShaderModelsSupported[index] =
            static_cast<D3D12DDI_SHADER_MODEL>(ddi_model_6_0 + (index - 1u) * ddi_model_step);
    *asked.pNumShaderModelsSupported = models;
    return S_OK;
}

HRESULT D3d12HostCaps::answer(const D3D12DDIARG_GETCAPS &args) const
{
    AV_SCOPE("D3d12HostCaps::answer");
    const UINT bytes = args.DataSize;
    switch (static_cast<UINT>(args.Type)) {
    case D3D12DDICAPS_TYPE_0081_3DPIPELINESUPPORT1: {
        if (args.pData == nullptr || bytes < sizeof(D3D12DDI_3DPIPELINESUPPORT1_DATA_0081)) return E_INVALIDARG;
        auto &levels = *static_cast<D3D12DDI_3DPIPELINESUPPORT1_DATA_0081 *>(args.pData);
        const uint32_t ours = pipeline_level();
        const uint32_t theirs = static_cast<uint32_t>(levels.HighestRuntimeSupportedFeatureLevel);
        levels.MaximumDriverSupportedFeatureLevel = static_cast<D3D12DDI_3DPIPELINELEVEL>(theirs < ours ? theirs : ours);
        return S_OK;
    }
    case D3D12DDICAPS_TYPE_3DPIPELINESUPPORT: {
        const uint32_t level = pipeline_level();
        return fill(args.pData, bytes, &level, sizeof(level));
    }
    case D3D12DDICAPS_TYPE_D3D12_OPTIONS: return options(args.pData, bytes);
    case D3D12DDICAPS_TYPE_SHADER: return shader(args.pData, bytes);
    case D3D12DDICAPS_TYPE_0011_SHADER_MODELS: return shader_models(args.pData, bytes);
    case D3D12DDICAPS_TYPE_GPUVA_CAPS: {
        const D3D12DDI_GPUVA_CAPS_0004 answer = {address_bits};
        return fill(args.pData, bytes, &answer, sizeof(answer));
    }
    case D3D12DDICAPS_TYPE_MEMORY_ARCHITECTURE: {
        D3D12DDI_MEMORY_ARCHITECTURE_CAPS_0041 answer = {};
        answer.IOCoherent = TRUE;
        return fill(args.pData, bytes, &answer, sizeof(answer));
    }
    case D3D12DDICAPS_TYPE_TEXTURE_LAYOUT_SETS: {
        D3D12DDI_ROW_MAJOR_LAYOUT_CAPS answer = {};
        answer.SubCaps[0] = {row_major_element, row_major_alignment, row_major_alignment, row_major_alignment};
        return fill(args.pData, bytes, &answer, sizeof(answer));
    }
    case D3D12DDICAPS_TYPE_TEXTURE_LAYOUT:
    case D3D12DDICAPS_TYPE_TEXTURE_LAYOUT1:
    case D3D12DDICAPS_TYPE_0022_TEXTURE_LAYOUT:
    case D3D12DDICAPS_TYPE_ARCHITECTURE_INFO:
    case D3D12DDICAPS_TYPE_0022_CPU_PAGE_TABLE_FALSE_POSITIVES:
    case D3D12DDICAPS_TYPE_0023_UMD_BASED_COMMAND_QUEUE_PRIORITY:
    case D3D12DDICAPS_TYPE_0033_ADAPTER_COMPUTE_ONLY:
    case D3D12DDICAPS_TYPE_0050_HARDWARE_SCHEDULING_CAPS:
    case D3D12DDICAPS_TYPE_EXECUTECOMMANDLISTS_PARALLELISM:
        return fill(args.pData, bytes, nullptr, 0u);
    case D3D12DDICAPS_TYPE_OPTIONS_0102: {
        D3D12DDI_OPTIONS_DATA_0102 answer = {};
        answer.SupportedSampleCountsWithNoOutputs = no_output_sample_counts;
        answer.MaxSamplerDescriptorHeapSize = sampler_heap_ceiling;
        answer.MaxSamplerDescriptorHeapSizeWithStaticSamplers = sampler_heap_ceiling;
        answer.MaxViewDescriptorHeapSize = view_heap_ceiling;
        return fill(args.pData, bytes, &answer, sizeof(answer));
    }
    case D3D12DDICAPS_TYPE_OPTIONS_0110: {
        D3D12DDI_OPTIONS_DATA_0110 answer = {};
        answer.ExecuteIndirectTier = D3D12DDI_EXECUTE_INDIRECT_TIER_1_0;
        return fill(args.pData, bytes, &answer, sizeof(answer));
    }
    default: return answer_later(args);
    }
}

HRESULT D3d12HostCaps::answer_later(const D3D12DDIARG_GETCAPS &args) const
{
    AV_SCOPE("D3d12HostCaps::answer_later");
    const UINT bytes = args.DataSize;
    switch (static_cast<UINT>(args.Type)) {
    case D3D12DDI_FEATURE_D3D12_PREDICATION_106: {
        D3D12DDI_FEATURE_DATA_PREDICATION_106 answer = {};
        answer.Supported = TRUE;
        return fill(args.pData, bytes, &answer, sizeof(answer));
    }
    case D3D12DDI_FEATURE_PLACED_RESOURCE_SUPPORT_INFO_106: return placed_resources(args.pData, bytes);
    case D3D12DDICAPS_TYPE_0103_WAVE_MMA: return wave_mma(args.pData, bytes);
    case D3D12DDICAPS_TYPE_0030_PROTECTED_RESOURCE_SESSION_SUPPORT: return protected_sessions(args.pData, bytes);
    case D3D12DDICAPS_TYPE_QUERY_META_COMMAND_CAPS_0061: return E_INVALIDARG;
    default: return answer_options(args);
    }
}

HRESULT D3d12HostCaps::answer_options(const D3D12DDIARG_GETCAPS &args) const
{
    AV_SCOPE("D3d12HostCaps::answer_options");
    const UINT bytes = args.DataSize;
    switch (static_cast<UINT>(args.Type)) {
    case D3D12DDICAPS_TYPE_OPTIONS_0093: {
        D3D12DDI_OPTIONS_DATA_0093 answer = {};
        answer.AdvancedTextureOpsSupported = caps_.advanced_texture_ops != 0u;
        answer.WriteableMSAATexturesSupported = caps_.writeable_msaa_textures != 0u;
        return fill(args.pData, bytes, &answer, sizeof(answer));
    }
    case D3D12DDI_FEATURE_HARDWARE_COPY_106: {
        D3D12DDI_FEATURE_DATA_HARDWARE_COPY_106 answer = {};
        answer.Supported = FALSE;
        return fill(args.pData, bytes, &answer, sizeof(answer));
    }
    case D3D12DDICAPS_TYPE_SHADER_MODEL_6_8_OPTIONS_0110: {
        D3D12DDI_SHADER_MODEL_6_8_OPTIONS_0110 answer = {};
        answer.SampleCmpGradientAndBiasSupported = caps_.sample_cmp_gradient_bias != 0u;
        answer.ExtendedCommandInfoSupported = caps_.extended_command_info != 0u;
        return fill(args.pData, bytes, &answer, sizeof(answer));
    }
    case D3D12DDICAPS_TYPE_OPTIONS_0090:
    case D3D12DDICAPS_TYPE_OPTIONS_0091:
    case D3D12DDICAPS_TYPE_OPTIONS_0098:
    case D3D12DDICAPS_TYPE_OPTIONS_0101:
    case D3D12DDICAPS_TYPE_OPTIONS_0109:
    case D3D12DDICAPS_TYPE_OPTIONS1_0103:
    case D3D12DDICAPS_TYPE_SAMPLER_FEEDBACK_0073:
    case D3D12DDICAPS_TYPE_0073_SUPPORT_BATCHED_MARKERS:
    case D3D12DDICAPS_TYPE_0074_PROTECTED_RESOURCE_SESSION_TYPE_COUNT:
    case D3D12DDICAPS_TYPE_0074_PROTECTED_RESOURCE_SESSION_TYPES:
    case D3D12DDICAPS_TYPE_SWIZZLE_PATTERN:
    case D3D12DDICAPS_TYPE_0022_SWIZZLE_PATTERN:
    case D3D12DDICAPS_TYPE_0030_CRYPTO_SESSION_SUPPORT:
    case D3D12DDICAPS_TYPE_0030_CONTENT_PROTECTION_SYSTEM_COUNT:
    case D3D12DDICAPS_TYPE_0030_CONTENT_PROTECTION_SYSTEM_SUPPORT:
    case D3D12DDICAPS_TYPE_0030_CRYPTO_SESSION_TRANSFORM_SUPPORT:
    default: return fill(args.pData, bytes, nullptr, 0u);
    }
}

HRESULT D3d12HostCaps::placed_resources(void *data, UINT bytes) const
{
    AV_SCOPE("D3d12HostCaps::placed_resources");
    if (data == nullptr || bytes < sizeof(D3D12DDI_FEATURE_DATA_PLACED_RESOURCE_SUPPORT_INFO_106)) return E_INVALIDARG;
    auto &info = *static_cast<D3D12DDI_FEATURE_DATA_PLACED_RESOURCE_SUPPORT_INFO_106 *>(data);
    info.Supported = info.ResourceType == D3D12DDI_RT_BUFFER || format_support(info.Format) != 0u;
    return S_OK;
}

HRESULT D3d12HostCaps::wave_mma(void *data, UINT bytes) const
{
    AV_SCOPE("D3d12HostCaps::wave_mma");
    if (data == nullptr || bytes < sizeof(D3D12DDI_WAVE_MMA_DATA_0103)) return E_INVALIDARG;
    auto &mma = *static_cast<D3D12DDI_WAVE_MMA_DATA_0103 *>(data);
    mma.Supported = FALSE;
    mma.K = 0u;
    mma.AccumDataTypes = D3D12DDI_WAVE_MMA_ACCUM_DATATYPE_NONE;
    mma.RequiredWaveLaneCountMin = 0u;
    mma.RequiredWaveLaneCountMax = 0u;
    return S_OK;
}

HRESULT D3d12HostCaps::protected_sessions(void *data, UINT bytes) const
{
    AV_SCOPE("D3d12HostCaps::protected_sessions");
    if (data == nullptr || bytes < sizeof(D3D12DDI_PROTECTED_RESOURCE_SESSION_SUPPORT_DATA_0030)) return E_INVALIDARG;
    static_cast<D3D12DDI_PROTECTED_RESOURCE_SESSION_SUPPORT_DATA_0030 *>(data)->Support =
        D3D12DDI_PROTECTED_RESOURCE_SESSION_SUPPORT_FLAG_0030_NONE;
    return S_OK;
}

}
