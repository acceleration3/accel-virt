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
    uint8_t staging[staging_bytes];
    KmtTransport transport(connection, SubApi::d3d, ring_order, reply_bytes, staging, sizeof(staging));
    if (transport.initialize() != Status::ok) return false;
    ScopeExit torn([&] { transport.teardown(); });
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

bool D3d11HostCaps::typeless(DXGI_FORMAT format) const
{
    AV_SCOPE("D3d11HostCaps::typeless");
    switch (format) {
    case DXGI_FORMAT_R32G32B32A32_TYPELESS:
    case DXGI_FORMAT_R32G32B32_TYPELESS:
    case DXGI_FORMAT_R16G16B16A16_TYPELESS:
    case DXGI_FORMAT_R32G32_TYPELESS:
    case DXGI_FORMAT_R32G8X24_TYPELESS:
    case DXGI_FORMAT_R10G10B10A2_TYPELESS:
    case DXGI_FORMAT_R8G8B8A8_TYPELESS:
    case DXGI_FORMAT_R16G16_TYPELESS:
    case DXGI_FORMAT_R32_TYPELESS:
    case DXGI_FORMAT_R24G8_TYPELESS:
    case DXGI_FORMAT_R8G8_TYPELESS:
    case DXGI_FORMAT_R16_TYPELESS:
    case DXGI_FORMAT_R8_TYPELESS:
    case DXGI_FORMAT_BC1_TYPELESS:
    case DXGI_FORMAT_BC2_TYPELESS:
    case DXGI_FORMAT_BC3_TYPELESS:
    case DXGI_FORMAT_BC4_TYPELESS:
    case DXGI_FORMAT_BC5_TYPELESS:
    case DXGI_FORMAT_B8G8R8A8_TYPELESS:
    case DXGI_FORMAT_B8G8R8X8_TYPELESS:
    case DXGI_FORMAT_BC6H_TYPELESS:
    case DXGI_FORMAT_BC7_TYPELESS: return true;
    default: return false;
    }
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
    /* Not 12_0 or 12_1 through Direct3D 11: the runtime refuses a feature
     * level 12 driver that reports no tiled resources tier 2 ("FL12+ driver
     * incorrectly did not report support for tiled resources tier 2+", its
     * journal), and the tile-mapping commands do not cross this wire. */
    if (level >= level_11_1) levels |= D3D11DDI_ENCODE_3DPIPELINESUPPORT_CAP(D3D11_1DDI_3DPIPELINELEVEL_11_1);
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
    default:
        /* Every other question is about a feature this driver does not
         * expose - texture layouts, swizzle patterns, ASTC - and all zeroes is
         * each one's "none": the runtime refuses a driver that fails one
         * outright (its journal: "Driver failed GetCaps with
         * D3DWDDM2_2DDICAPS_TEXTURE_LAYOUT and NULL pInfo"). */
        for (UINT index = 0u; index < args.DataSize; index++) static_cast<uint8_t *>(args.pData)[index] = 0u;
        return S_OK;
    }
}

UINT D3d11HostCaps::format_support(DXGI_FORMAT format) const
{
    AV_SCOPE("D3d11HostCaps::format_support");
    const uint32_t index = static_cast<uint32_t>(format);
    if (index >= format_count) return 0u;
    /* The host answers in the API's D3D11_FORMAT_SUPPORT{,2} bits and the DDI
     * asks in its own, which are numbered differently: each is carried across
     * by name. */
    struct Carry {
        uint32_t api;
        bool     second;
        UINT     ddi;
    };
    constexpr Carry carried[] = {
        {0x200u, false, D3D10_DDI_FORMAT_SUPPORT_SHADER_SAMPLE},
        {0x4000u, false, D3D10_DDI_FORMAT_SUPPORT_RENDERTARGET},
        {0x8000u, false, D3D10_DDI_FORMAT_SUPPORT_BLENDABLE},
        {0x200000u, false, D3D10_DDI_FORMAT_SUPPORT_MULTISAMPLE_RENDERTARGET},
        {0x400000u, false, D3D10_DDI_FORMAT_SUPPORT_MULTISAMPLE_LOAD},
        {0x2u, false, D3D11_1DDI_FORMAT_SUPPORT_VERTEX_BUFFER},
        {0x2000000u, false, D3D11_1DDI_FORMAT_SUPPORT_UAV_WRITES},
        {0x1u, false, D3D11_1DDI_FORMAT_SUPPORT_BUFFER},
        {0x800000u, false, D3D11_1DDI_FORMAT_SUPPORT_SHADER_GATHER},
        {0x80000u, false, D3D11_1DDI_FORMAT_SUPPORT_DISPLAY},
        {0x100u, true, D3D11_1DDI_FORMAT_SUPPORT_OUTPUT_MERGER_LOGIC_OP},
        {0x40u, true, D3DWDDM2_0DDI_FORMAT_SUPPORT_UAV_READS},
    };
    const d3dproto::FormatCaps &row = formats_[index];
    UINT support = 0u;
    for (const Carry &carry : carried)
        if (((carry.second ? row.support2 : row.support) & carry.api) != 0u) support |= carry.ddi;
    /* The DDI has no depth-stencil bit: a depth format's "render target" is
     * its depth-stencil view - but only a depth format's, never the typeless
     * and view formats of its family, which the runtime refuses to see as
     * renderable (it stops at R32_FLOAT_X8X24_TYPELESS). */
    const bool depth = format == DXGI_FORMAT_D32_FLOAT_S8X24_UINT || format == DXGI_FORMAT_D32_FLOAT ||
                       format == DXGI_FORMAT_D24_UNORM_S8_UINT || format == DXGI_FORMAT_D16_UNORM;
    constexpr uint32_t api_depth_stencil = 0x10000u;
    if (depth && (row.support & api_depth_stencil) != 0u) support |= D3D10_DDI_FORMAT_SUPPORT_RENDERTARGET;
    /* A depth format is sampled through its view format, never itself, and
     * Vulkan's answer - which can sample depth - is not Direct3D's. */
    if (depth)
        support &= ~static_cast<UINT>(D3D10_DDI_FORMAT_SUPPORT_SHADER_SAMPLE | D3D10_DDI_FORMAT_SUPPORT_MULTISAMPLE_LOAD |
                                      D3D11_1DDI_FORMAT_SUPPORT_SHADER_GATHER | D3D11_1DDI_FORMAT_SUPPORT_BUFFER |
                                      D3D11_1DDI_FORMAT_SUPPORT_VERTEX_BUFFER | D3D11_1DDI_FORMAT_SUPPORT_UAV_WRITES |
                                      D3DWDDM2_0DDI_FORMAT_SUPPORT_UAV_READS);
    /* Blending and multisampled rendering are rendering; multisampled use at
     * no count past one is a table the runtime refuses the driver over
     * ("MSAA quality reported to be 0"). */
    const bool renders = (support & D3D10_DDI_FORMAT_SUPPORT_RENDERTARGET) != 0u;
    const bool counted = (row.sample_counts & ~1u) != 0u && row.quality_levels != 0u;
    if (!renders)
        support &= ~static_cast<UINT>(D3D10_DDI_FORMAT_SUPPORT_BLENDABLE |
                                      D3D10_DDI_FORMAT_SUPPORT_MULTISAMPLE_RENDERTARGET);
    if (!counted)
        support &= ~static_cast<UINT>(D3D10_DDI_FORMAT_SUPPORT_MULTISAMPLE_RENDERTARGET |
                                      D3D10_DDI_FORMAT_SUPPORT_MULTISAMPLE_LOAD);
    return support;
}

UINT D3d11HostCaps::quality_levels(DXGI_FORMAT format, UINT samples) const
{
    AV_SCOPE("D3d11HostCaps::quality_levels");
    const uint32_t index = static_cast<uint32_t>(format);
    if (index >= format_count || samples == 0u || (samples & (samples - 1u)) != 0u) return 0u;
    /* One sample is every format's one quality level. Past one, the runtime
     * holds each format family to one story (LLOCompleteLayerConstruction): a
     * typeless parent answers what its multisampled children do, a child that
     * claims multisampled rendering answers the parent's levels, and a child
     * that does not claim it answers none - "MSAA quality reported to be 0" is
     * its complaint about a non-zero answer there. */
    if (samples == 1u) return 1u;
    const bool multisampled = typeless(format) ||
                              (format_support(format) & D3D10_DDI_FORMAT_SUPPORT_MULTISAMPLE_RENDERTARGET) != 0u;
    if (!multisampled) return 0u;
    const uint32_t step = static_cast<uint32_t>(__builtin_ctz(samples));
    return (formats_[index].sample_counts & (1u << step)) != 0u ? formats_[index].quality_levels : 0u;
}

}  // namespace av::d3d11
