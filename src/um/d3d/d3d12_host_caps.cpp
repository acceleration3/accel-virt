#include "um/d3d/d3d12_host_caps.hpp"

#include "core/trace/function_scope.hpp"
#include "icd/d3d/caps_reply.hpp"
#include "proto/client/client_transport.hpp"
#include "proto/generated/d3d12/d3d12_dispatch_client.h"

namespace av::d3d12 {

namespace {

/* The questions asked; small enough for the stack. */

constexpr uint32_t level_11_0 = 0xb000u;
constexpr uint32_t level_11_1 = 0xb100u;
/* D3D_SHADER_MODEL 5.1 and 6.5, and the DDI's release values: 5.1, then each
 * 6.x as 0x60005 + 0x10 * x. */
constexpr uint32_t api_model_6_0 = 0x60u;
constexpr uint32_t api_model_6_5 = 0x65u;
constexpr uint32_t ddi_model_5_1 = 0x50015u;
constexpr uint32_t ddi_model_6_0 = 0x60005u;
constexpr uint32_t ddi_model_step = 0x10u;
constexpr uint32_t address_bits = 40u;
/* Row-major layout alignments: what a texture copy footprint is aligned to. */
constexpr UINT16   row_major_element = 16u;
constexpr UINT16   row_major_alignment = 256u;
/* OPTIONS_0102's ceilings: vkd3d-proton's own view heap floor, and the sampler
 * count the runtime takes as the least a driver may report. */
constexpr UINT     view_heap_ceiling = 1000000u;
constexpr UINT     sampler_heap_ceiling = 4000u;
/* 1, 2, 4, 8 and 16 samples, a bit each. */
constexpr UINT     no_output_sample_counts = 0x1fu;

}  // namespace

D3d12HostCaps::D3d12HostCaps() : caps_{}, formats_(), learned_(false)
{
    AV_SCOPE("D3d12HostCaps::D3d12HostCaps");
}

bool D3d12HostCaps::learn(ClientTransport &transport, Kmt &, uint32_t)
{
    AV_SCOPE("D3d12HostCaps::learn");
    if (learned_) return true;
    uint32_t hresult = 1u;
    if (!d3d12proto::av_d3d12_OpenDevice12(transport, 0u, &hresult, &caps_) || hresult != 0u) return false;
    for (uint32_t format = 1u; format < format_count; format++) {
        uint32_t support1 = 0u, support2 = 0u, levels = 0u;
        if (!d3d12proto::av_d3d12_QueryFormat12(transport, format, 1u, &support1, &support2, &levels)) return false;
        formats_.record(format, support1, support2);
    }
    if (!learn_samples(transport)) return false;
    learned_ = true;
    return true;
}

/* Level counts past one sample, asked only of the formats the host would
 * multisample at all: a question each, and there are hundreds. */
bool D3d12HostCaps::learn_samples(ClientTransport &transport)
{
    AV_SCOPE("D3d12HostCaps::learn_samples");
    for (uint32_t format = 1u; format < format_count; format++) {
        if (!formats_.multisample_candidate(format)) continue;
        for (uint32_t step = 1u; step < d3d::DxgiFormatTable::sample_steps; step++) {
            uint32_t support1 = 0u, support2 = 0u, levels = 0u;
            if (!d3d12proto::av_d3d12_QueryFormat12(transport, format, 1u << step, &support1, &support2, &levels))
                return false;
            formats_.record_samples(format, 1u << step, levels);
        }
    }
    formats_.settle();
    return true;
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
    return formats_.format_support(static_cast<uint32_t>(format));
}

uint32_t D3d12HostCaps::pipeline_level() const
{
    AV_SCOPE("D3d12HostCaps::pipeline_level");
    if (caps_.feature_level >= level_11_1) return D3D12DDI_3DPIPELINELEVEL_11_1;
    if (caps_.feature_level >= level_11_0) return D3D12DDI_3DPIPELINELEVEL_11_0;
    return D3D12DDI_3DPIPELINELEVEL_1_0_CORE;
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
    answer.ResourceHeapTier = static_cast<D3D12DDI_RESOURCE_HEAP_TIER>(caps_.resource_heap_tier);
    answer.TiledResourcesTier = D3D12DDI_TILED_RESOURCES_TIER_NOT_SUPPORTED;
    answer.CrossNodeSharingTier = D3D12DDI_CROSS_NODE_SHARING_TIER_NOT_SUPPORTED;
    return fill(data, bytes, &answer, sizeof(answer));
}

HRESULT D3d12HostCaps::shader(void *data, UINT bytes) const
{
    AV_SCOPE("D3d12HostCaps::shader");
    D3D12DDI_SHADER_CAPS_0084 answer = {};
    answer.DoubleOps = caps_.double_ops != 0u;
    answer.TypedUAVLoadAdditionalFormats = caps_.typed_uav_load_additional_formats != 0u;
    answer.WaveOps = caps_.wave_ops != 0u;
    answer.WaveLaneCountMin = caps_.wave_lane_min;
    answer.WaveLaneCountMax = caps_.wave_lane_max;
    answer.TotalLaneCount = caps_.total_lanes;
    answer.Int64Ops = caps_.int64_ops != 0u;
    return fill(data, bytes, &answer, sizeof(answer));
}

/* Asked twice: once for the count, then with room for the list. 5.1 is always
 * there and the 6.x releases follow without a gap, which D3D12Core demands. */
HRESULT D3d12HostCaps::shader_models(void *data, UINT bytes) const
{
    AV_SCOPE("D3d12HostCaps::shader_models");
    if (data == nullptr || bytes < sizeof(D3D12DDI_D3D12_SHADER_MODELS_DATA_0011)) return E_INVALIDARG;
    const auto &asked = *static_cast<const D3D12DDI_D3D12_SHADER_MODELS_DATA_0011 *>(data);
    if (asked.pNumShaderModelsSupported == nullptr) return E_INVALIDARG;
    const uint32_t highest = caps_.shader_model < api_model_6_5 ? caps_.shader_model : api_model_6_5;
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
        /* A discrete GPU behind PCIe: IO coherent, not UMA (d3d12core refuses
         * a driver that says otherwise on x64). */
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
        /* No device-dependent layouts or swizzles, no tile-based renderer,
         * no false positives, no queue priorities, not compute-only, no
         * hardware queues: every one of them zero. */
        return fill(args.pData, bytes, nullptr, 0u);
    case D3D12DDICAPS_TYPE_OPTIONS_0102: {
        /* The DDI's ceilings, which the runtime clamps to the API's own: it
         * refuses a device reporting the API's 2048 samplers or a view heap
         * smaller than its 1000000 ("Driver's MaxViewDescriptorHeapSize is
         * too small"). No-output rendering at 1 to 16 samples is required at
         * 11_1. */
        D3D12DDI_OPTIONS_DATA_0102 answer = {};
        answer.SupportedSampleCountsWithNoOutputs = no_output_sample_counts;
        answer.MaxSamplerDescriptorHeapSize = sampler_heap_ceiling;
        answer.MaxSamplerDescriptorHeapSizeWithStaticSamplers = sampler_heap_ceiling;
        answer.MaxViewDescriptorHeapSize = view_heap_ceiling;
        return fill(args.pData, bytes, &answer, sizeof(answer));
    }
    case D3D12DDICAPS_TYPE_OPTIONS_0110: {
        /* The tier has no zero: 1_0 is the floor, and all-zero is out of range. */
        D3D12DDI_OPTIONS_DATA_0110 answer = {};
        answer.ExecuteIndirectTier = D3D12DDI_EXECUTE_INDIRECT_TIER_1_0;
        return fill(args.pData, bytes, &answer, sizeof(answer));
    }
    default:
        /* Every other question is about a feature this driver does not
         * expose, and all zeroes is each one's "none": a refusal is what the
         * runtime cannot take for some of them. */
        return fill(args.pData, bytes, nullptr, 0u);
    }
}

}  // namespace av::d3d12
