#include "um/d3d/d3d12_host_caps.hpp"

#include "core/support/scope_exit.hpp"
#include "core/trace/function_scope.hpp"
#include "guest/transport/guest_connection.hpp"
#include "guest/transport/kmt_transport.hpp"
#include "proto/context/sub_api.hpp"
#include "proto/generated/d3d12/d3d12_dispatch_client.h"

namespace av::d3d12 {

namespace {

/* The questions asked; small enough for the stack. */
constexpr uint8_t  ring_order = 16u;
constexpr uint32_t reply_bytes = 64u * 1024u;
constexpr size_t   staging_bytes = 16u * 1024u;

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

/* D3D12_FORMAT_SUPPORT1 and 2 bits, and the DDI's bit each becomes. */
struct SupportBit {
    uint32_t api;
    uint32_t second; /* 1 for a FORMAT_SUPPORT2 bit */
    UINT     ddi;
};
constexpr SupportBit support_bits[] = {
    {0x1u, 0u, D3D12DDI_FORMAT_SUPPORT_BUFFER},
    {0x2u, 0u, D3D12DDI_FORMAT_SUPPORT_VERTEX_BUFFER},
    {0x200u, 0u, D3D12DDI_FORMAT_SUPPORT_SHADER_SAMPLE},
    {0x4000u, 0u, D3D12DDI_FORMAT_SUPPORT_RENDERTARGET},
    {0x8000u, 0u, D3D12DDI_FORMAT_SUPPORT_BLENDABLE},
    {0x80000u, 0u, D3D12DDI_FORMAT_SUPPORT_DISPLAY},
    {0x800000u, 0u, D3D12DDI_FORMAT_SUPPORT_SHADER_GATHER},
    {0x2000000u, 0u, D3D12DDI_FORMAT_SUPPORT_UAV_WRITES},
    {0x40u, 1u, D3D12DDI_FORMAT_SUPPORT_UAV_READS},
    {0x100u, 1u, D3D12DDI_FORMAT_SUPPORT_OUTPUT_MERGER_LOGIC_OP},
};

}  // namespace

D3d12HostCaps::D3d12HostCaps() : caps_{}, support1_{}, support2_{}, learned_(false)
{
    AV_SCOPE("D3d12HostCaps::D3d12HostCaps");
}

bool D3d12HostCaps::learn(Kmt &kmt, uint32_t adapter)
{
    AV_SCOPE("D3d12HostCaps::learn");
    if (learned_) return true;
    GuestConnection connection(kmt);
    if (connection.open(adapter) != Status::ok) return false;
    ScopeExit closed([&] { connection.close(); });
    uint8_t      staging[staging_bytes];
    KmtTransport transport(connection, SubApi::d3d, ring_order, reply_bytes, staging, sizeof(staging));
    if (transport.initialize() != Status::ok) return false;
    ScopeExit torn([&] { transport.teardown(); });
    uint32_t hresult = 1u;
    if (!d3d12proto::rvk_d3d12_OpenDevice12(transport, 0u, &hresult, &caps_) || hresult != 0u) return false;
    for (uint32_t format = 1u; format < format_count; format++) {
        uint32_t levels = 0u;
        if (!d3d12proto::rvk_d3d12_QueryFormat12(transport, format, 1u, &support1_[format], &support2_[format],
                                                 &levels))
            return false;
    }
    learned_ = true;
    return true;
}

bool D3d12HostCaps::learned() const
{
    AV_SCOPE("D3d12HostCaps::learned");
    return learned_;
}

UINT D3d12HostCaps::format_support(DXGI_FORMAT format) const
{
    AV_SCOPE("D3d12HostCaps::format_support");
    const uint32_t index = static_cast<uint32_t>(format);
    if (index == 0u || index >= format_count) return 0u;
    UINT made = 0u;
    for (const SupportBit &bit : support_bits) {
        const uint32_t word = bit.second != 0u ? support2_[index] : support1_[index];
        if ((word & bit.api) != 0u) made |= bit.ddi;
    }
    return made;
}

UINT D3d12HostCaps::descriptor_size(uint32_t type) const
{
    AV_SCOPE("D3d12HostCaps::descriptor_size");
    return type < 4u ? caps_.descriptor_sizes[type] : 0u;
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
    if (data == nullptr) return E_INVALIDARG;
    uint8_t *const out = static_cast<uint8_t *>(data);
    for (UINT index = 0u; index < bytes; index++) out[index] = 0u;
    const UINT copied = bytes < answer_bytes ? bytes : answer_bytes;
    __builtin_memcpy(out, answer, copied);
    return S_OK;
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
    default: return E_INVALIDARG;
    }
}

}  // namespace av::d3d12
