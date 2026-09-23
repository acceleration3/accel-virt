#include "um/d3d/d3d11_host_caps.hpp"

#include "core/support/scope_exit.hpp"
#include "core/trace/function_scope.hpp"
#include "guest/transport/guest_connection.hpp"
#include "guest/transport/kmt_transport.hpp"
#include "proto/context/sub_api.hpp"
#include "proto/generated/d3d/d3d_dispatch_client.h"

namespace av::d3d11 {

namespace {

/* The one question answered; small enough for the stack. */
constexpr uint8_t  ring_order = 16u;
constexpr uint32_t reply_bytes = 64u * 1024u;
constexpr size_t   staging_bytes = 16u * 1024u;
constexpr uint32_t level_12_1 = 0xc100u;
constexpr uint32_t level_12_0 = 0xc000u;
constexpr uint32_t level_11_1 = 0xb100u;
constexpr uint32_t level_11_0 = 0xb000u;

}  // namespace

D3d11HostCaps::D3d11HostCaps() : caps_{}, formats_{}, learned_(false)
{
    AV_SCOPE("D3d11HostCaps::D3d11HostCaps");
}

bool D3d11HostCaps::learn(Kmt &kmt, uint32_t adapter)
{
    AV_SCOPE("D3d11HostCaps::learn");
    if (learned_) return true;
    GuestConnection connection(kmt);
    if (connection.open(adapter) != Status::ok) return false;
    ScopeExit closed([&] { connection.close(); });
    static uint8_t staging[staging_bytes];
    KmtTransport transport(connection, SubApi::d3d, ring_order, reply_bytes, staging, sizeof(staging));
    if (transport.initialize() != Status::ok) return false;
    ScopeExit torn([&] { transport.teardown(); });
    uint32_t hresult = 1u, level = 0u;
    if (!d3dproto::rvk_d3d_CreateDevice11(transport, 0u, 0u, &hresult, &level) || hresult != 0u) return false;
    uint64_t answered = 0u;
    if (!d3dproto::rvk_d3d_QueryCaps11(transport, &caps_, formats_, sizeof(formats_), &answered)) return false;
    learned_ = true;
    return true;
}

bool D3d11HostCaps::learned() const
{
    AV_SCOPE("D3d11HostCaps::learned");
    return learned_;
}

uint32_t D3d11HostCaps::feature_level() const
{
    AV_SCOPE("D3d11HostCaps::feature_level");
    return caps_.feature_level;
}

UINT D3d11HostCaps::pipeline_levels() const
{
    AV_SCOPE("D3d11HostCaps::pipeline_levels");
    const uint32_t level = caps_.feature_level;
    UINT levels = D3D11DDI_ENCODE_3DPIPELINESUPPORT_CAP(D3D11DDI_3DPIPELINELEVEL_10_0) |
                  D3D11DDI_ENCODE_3DPIPELINESUPPORT_CAP(D3D11DDI_3DPIPELINELEVEL_10_1) |
                  D3D11DDI_ENCODE_3DPIPELINESUPPORT_CAP(D3D11_1DDI_3DPIPELINELEVEL_9_1) |
                  D3D11DDI_ENCODE_3DPIPELINESUPPORT_CAP(D3D11_1DDI_3DPIPELINELEVEL_9_2) |
                  D3D11DDI_ENCODE_3DPIPELINESUPPORT_CAP(D3D11_1DDI_3DPIPELINELEVEL_9_3);
    if (level >= level_11_0) levels |= D3D11DDI_ENCODE_3DPIPELINESUPPORT_CAP(D3D11DDI_3DPIPELINELEVEL_11_0);
    if (level >= level_11_1) levels |= D3D11DDI_ENCODE_3DPIPELINESUPPORT_CAP(D3D11_1DDI_3DPIPELINELEVEL_11_1);
    if (level >= level_12_0) levels |= D3D11DDI_ENCODE_3DPIPELINESUPPORT_CAP(D3DWDDM2_0DDI_3DPIPELINELEVEL_12_0);
    if (level >= level_12_1) levels |= D3D11DDI_ENCODE_3DPIPELINESUPPORT_CAP(D3DWDDM2_0DDI_3DPIPELINELEVEL_12_1);
    return levels;
}

HRESULT D3d11HostCaps::answer(const D3D10_2DDIARG_GETCAPS &args) const
{
    AV_SCOPE("D3d11HostCaps::answer");
    if (args.pData == nullptr) return E_INVALIDARG;
    UINT *const word = static_cast<UINT *>(args.pData);
    switch (static_cast<UINT>(args.Type)) {
    case D3D11DDICAPS_THREADING:
        /* Neither free-threaded nor command lists: the runtime serializes the
         * device and emulates deferred contexts, and the wire stays one
         * immediate stream. */
        *word = 0u;
        return S_OK;
    case D3D11DDICAPS_SHADER:
        *word = (caps_.double_precision_float_shader_ops != 0u ? D3D11DDICAPS_SHADER_DOUBLES : 0u) |
                (caps_.compute_shaders_on_10x != 0u
                     ? D3D11DDICAPS_SHADER_COMPUTE_PLUS_RAW_AND_STRUCTURED_BUFFERS_IN_SHADER_4_X
                     : 0u) |
                (caps_.ps_specified_stencil_ref != 0u ? D3D11DDICAPS_SHADER_SPECIFIED_STENCIL_REF : 0u) |
                (caps_.typed_uav_load_additional_formats != 0u ? D3D11DDICAPS_SHADER_TYPED_UAV_LOAD_ADDITIONAL_FORMATS
                                                               : 0u) |
                (caps_.rovs != 0u ? D3D11DDICAPS_SHADER_ROVS : 0u);
        return S_OK;
    case D3D11DDICAPS_3DPIPELINESUPPORT: *word = pipeline_levels(); return S_OK;
    case D3D11_1DDICAPS_D3D11_OPTIONS: {
        if (args.DataSize < sizeof(D3D11_1DDI_D3D11_OPTIONS_DATA)) return E_INVALIDARG;
        auto *const options = static_cast<D3D11_1DDI_D3D11_OPTIONS_DATA *>(args.pData);
        options->OutputMergerLogicOp = caps_.output_merger_logic_op != 0u;
        options->AssignDebugBinarySupport = FALSE;
        return S_OK;
    }
    case D3D11_1DDICAPS_ARCHITECTURE_INFO: *word = FALSE; return S_OK;
    case D3D11_1DDICAPS_SHADER_MIN_PRECISION_SUPPORT: {
        if (args.DataSize < sizeof(D3D11_DDI_SHADER_MIN_PRECISION_SUPPORT_DATA)) return E_INVALIDARG;
        auto *const precision = static_cast<D3D11_DDI_SHADER_MIN_PRECISION_SUPPORT_DATA *>(args.pData);
        precision->PixelShaderMinPrecision = caps_.pixel_shader_min_precision;
        precision->AllOtherStagesMinPrecision = caps_.all_other_shader_stages_min_precision;
        return S_OK;
    }
    case D3DWDDM1_3DDICAPS_D3D11_OPTIONS1: *word = 0u; return S_OK;
    case D3DWDDM1_3DDICAPS_MARKER: *word = 0u; return S_OK;
    case D3DWDDM2_0DDICAPS_D3D11_OPTIONS2: *word = caps_.conservative_rasterization_tier; return S_OK;
    case D3DWDDM2_0DDICAPS_D3D11_OPTIONS3: *word = caps_.vp_and_rt_array_index_from_any_shader; return S_OK;
    case D3DWDDM2_0DDICAPS_MEMORY_ARCHITECTURE: {
        if (args.DataSize < sizeof(D3DWDDM2_0DDI_MEMORY_ARCHITECTURE_CAPS)) return E_INVALIDARG;
        auto *const memory = static_cast<D3DWDDM2_0DDI_MEMORY_ARCHITECTURE_CAPS *>(args.pData);
        memory->UMA = FALSE;
        memory->CacheCoherent = FALSE;
        return S_OK;
    }
    case D3DWDDM2_0DDICAPS_GPUVA_CAPS: *word = 40u; return S_OK;
    case D3DWDDM2_2DDICAPS_SHADERCACHE: *word = FALSE; return S_OK;
    default: return E_INVALIDARG;
    }
}

UINT D3d11HostCaps::format_support(DXGI_FORMAT format) const
{
    AV_SCOPE("D3d11HostCaps::format_support");
    const uint32_t index = static_cast<uint32_t>(format);
    return index < format_count ? formats_[index].support : 0u;
}

UINT D3d11HostCaps::quality_levels(DXGI_FORMAT format, UINT samples) const
{
    AV_SCOPE("D3d11HostCaps::quality_levels");
    const uint32_t index = static_cast<uint32_t>(format);
    if (index >= format_count || samples == 0u || (samples & (samples - 1u)) != 0u) return 0u;
    const uint32_t step = static_cast<uint32_t>(__builtin_ctz(samples));
    return (formats_[index].sample_counts & (1u << step)) != 0u ? formats_[index].quality_levels : 0u;
}

}  // namespace av::d3d11
