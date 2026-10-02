#include "um/d3d/d3d9_host_caps.hpp"

#include "core/trace/function_scope.hpp"
#include "guest/kmt/kmt.hpp"
#include "proto/context/umd_adapter_info.hpp"
#include "proto/generated/d3d9/d3d9_dispatch_client.h"

#include <string.h>

namespace av::d3d9 {

namespace {

/* The queries the host's Direct3D 9 runs (D3DDDIQUERYTYPE); the three timestamp
 * ones go together or not at all (reverse-engineering.md §2.3). */
constexpr D3DDDIQUERYTYPE queries[] = {D3DDDIQUERYTYPE_VCACHE,         D3DDDIQUERYTYPE_EVENT,
                                       D3DDDIQUERYTYPE_OCCLUSION,      D3DDDIQUERYTYPE_TIMESTAMP,
                                       D3DDDIQUERYTYPE_TIMESTAMPDISJOINT, D3DDDIQUERYTYPE_TIMESTAMPFREQ};
constexpr UINT query_count = sizeof(queries) / sizeof(queries[0]);
/* D3DPMISCCAPS_FOGINFVF in Direct3D 8's header: the runtime requires it of a
 * shader-model-2 driver and the Direct3D 9 header no longer names it. */
constexpr DWORD fog_in_fvf = 0x2000u;
constexpr DWORD line_pattern_rep = 0x4u;          /* D3DPMISCCAPS_LINEPATTERNREP */
constexpr DWORD no_vsdt_ubyte4 = 0x80u;            /* D3DVTXPCAPS_NO_VSDT_UBYTE4 */
constexpr DWORD managed_device = 0x40u;            /* D3DCAPS3's "managed device" bit */
/* d3dhal.h's DevCaps a driver reports and an application never sees: vertex
 * and index buffers in video memory, and volume locks of a sub-box. Without
 * the first two, CVertexBuffer::Create and its index twin quietly move every
 * default-pool buffer to system memory, and every draw copies its vertices
 * from the guest (d3d9.dll @10048b04). */
constexpr DWORD hw_vertex_buffer = 0x02000000u;    /* D3DDEVCAPS_HWVERTEXBUFFER */
constexpr DWORD hw_index_buffer = 0x04000000u;     /* D3DDEVCAPS_HWINDEXBUFFER */
constexpr DWORD sub_volume_lock = 0x08000000u;     /* D3DDEVCAPS_SUBVOLUMELOCK */
constexpr DWORD most_render_targets = 4u;
constexpr uint32_t last_multisample = 16u;
constexpr uint32_t multisample_bits_from = 1u;      /* D3DMULTISAMPLE_NONMASKABLE */
constexpr uint32_t words_per_format = 2u;

}  // namespace

D3d9HostCaps::D3d9HostCaps()
    : caps_{}, formats_(), qualities_{}, quality_count_(0u), snapshots_(false), learned_(false)
{
    AV_SCOPE("D3d9HostCaps::D3d9HostCaps");
}

bool D3d9HostCaps::learn(ClientTransport &transport, Kmt &kmt, uint32_t adapter)
{
    AV_SCOPE("D3d9HostCaps::learn");
    if (learned_) return true;
    uint32_t hresult = 1u, vendor = 0u, device = 0u, subsystem = 0u, revision = 0u, memory_mb = 0u;
    uint64_t answered = 0u;
    if (!d3d9proto::av_d3d9_QueryCaps9(transport, &hresult, &vendor, &device, &subsystem, &revision, &memory_mb,
                                       &caps_, sizeof(caps_), &answered) ||
        hresult != 0u || answered != sizeof(caps_))
        return false;
    uint32_t asked[d3d::D3d9FormatOps::max_formats] = {};
    uint32_t support[d3d::D3d9FormatOps::max_formats * words_per_format] = {};
    const uint32_t count = formats_.formats_to_ask();
    for (uint32_t index = 0u; index < count; index++) asked[index] = formats_.format_to_ask(index);
    if (!d3d9proto::av_d3d9_QueryFormats9(transport, count, asked, support, sizeof(support), &answered) ||
        answered != uint64_t{count} * words_per_format * sizeof(uint32_t))
        return false;
    for (uint32_t index = 0u; index < count; index++) {
        formats_.record(asked[index], support[index * words_per_format], support[index * words_per_format + 1u]);
        /* Each multisample type the format renders at, and NONMASKABLE with them. */
        const uint32_t types = support[index * words_per_format + 1u];
        for (uint32_t type = multisample_bits_from; type <= last_multisample && types != 0u; type++) {
            if (type != multisample_bits_from && (types & (1u << type)) == 0u) continue;
            uint32_t answer = 1u, levels = 0u;
            if (!d3d9proto::av_d3d9_QueryMultisample9(transport, asked[index], type, &answer, &levels)) return false;
            if (answer == 0u && quality_count_ < max_qualities)
                qualities_[quality_count_++] = Quality{asked[index], type, levels};
        }
    }
    fix_up();
    UmdAdapterInfo info;
    snapshots_ = kmt.query_adapter_features(adapter, info) == Status::ok &&
                 info.offers(UmdAdapterInfo::feature_present_snapshots);
    learned_ = true;
    return true;
}

/* DXVK answers as an application sees a device; d3d9.dll judges a driver,
 * and disables the HAL over bits an application never reads. */
void D3d9HostCaps::fix_up()
{
    AV_SCOPE("D3d9HostCaps::fix_up");
    caps_.DeviceType = D3DDEVTYPE_HAL;
    caps_.AdapterOrdinal = 0u;
    caps_.Caps2 |= D3DCAPS2_DYNAMICTEXTURES;
    caps_.Caps3 &= ~managed_device;
    caps_.DevCaps |= hw_vertex_buffer | hw_index_buffer | sub_volume_lock;
    caps_.DevCaps2 |= D3DDEVCAPS2_STREAMOFFSET;
    caps_.RasterCaps |= D3DPRASTERCAPS_SCISSORTEST;
    caps_.PrimitiveMiscCaps = (caps_.PrimitiveMiscCaps | fog_in_fvf) & ~line_pattern_rep;
    caps_.VertexProcessingCaps &= ~no_vsdt_ubyte4;
    if (caps_.NumSimultaneousRTs == 0u) caps_.NumSimultaneousRTs = 1u;
    if (caps_.NumSimultaneousRTs > most_render_targets) caps_.NumSimultaneousRTs = most_render_targets;
}

bool D3d9HostCaps::learned() const
{
    AV_SCOPE("D3d9HostCaps::learned");
    return learned_;
}

bool D3d9HostCaps::present_snapshots() const
{
    AV_SCOPE("D3d9HostCaps::present_snapshots");
    return snapshots_;
}

const D3DCAPS9 &D3d9HostCaps::caps() const
{
    AV_SCOPE("D3d9HostCaps::caps");
    return caps_;
}

HRESULT D3d9HostCaps::copy_out(const D3DDDIARG_GETCAPS &args, const void *data, size_t bytes) const
{
    AV_SCOPE("D3d9HostCaps::copy_out");
    if (args.pData == nullptr || args.DataSize < bytes) return E_INVALIDARG;
    memcpy(args.pData, data, bytes);
    return S_OK;
}

HRESULT D3d9HostCaps::answer_formats(const D3DDDIARG_GETCAPS &args) const
{
    AV_SCOPE("D3d9HostCaps::answer_formats");
    const size_t bytes = size_t{formats_.count()} * sizeof(FORMATOP);
    if (args.pData == nullptr || args.DataSize < bytes) return E_INVALIDARG;
    auto *const out = static_cast<FORMATOP *>(args.pData);
    for (uint32_t index = 0u; index < formats_.count(); index++) {
        const d3d::D3d9FormatOps::Op &op = formats_.at(index);
        out[index].Format = static_cast<D3DDDIFORMAT>(op.format_);
        out[index].Operations = op.operations_;
        out[index].FlipMsTypes = op.multisample_;
        out[index].BltMsTypes = op.multisample_;
        out[index].PrivateFormatBitCount = 0u;
    }
    return S_OK;
}

HRESULT D3d9HostCaps::answer_quality(const D3DDDIARG_GETCAPS &args) const
{
    AV_SCOPE("D3d9HostCaps::answer_quality");
    if (args.pData == nullptr || args.DataSize < sizeof(DDIMULTISAMPLEQUALITYLEVELSDATA)) return E_INVALIDARG;
    auto *const asked = static_cast<DDIMULTISAMPLEQUALITYLEVELSDATA *>(args.pData);
    asked->QualityLevels = 0u;
    for (uint32_t index = 0u; index < quality_count_; index++)
        if (qualities_[index].format == static_cast<uint32_t>(asked->Format) &&
            qualities_[index].type == static_cast<uint32_t>(asked->MsType))
            asked->QualityLevels = qualities_[index].levels;
    return S_OK;
}

/* Only what d3d9.dll asks of a Direct3D 9 driver is answered; the DirectDraw,
 * Direct3D 3-8, video and content-protection types are refused, as nothing
 * here offers what they describe. */
HRESULT D3d9HostCaps::answer(const D3DDDIARG_GETCAPS &args) const
{
    AV_SCOPE("D3d9HostCaps::answer");
    const UINT formats = formats_.count();
    const UINT gamma = 0u;
    switch (args.Type) {
    case D3DDDICAPS_GETD3D9CAPS: return copy_out(args, &caps_, sizeof(caps_));
    case D3DDDICAPS_GETFORMATCOUNT: return copy_out(args, &formats, sizeof(formats));
    case D3DDDICAPS_GETFORMATDATA: return answer_formats(args);
    case D3DDDICAPS_GETD3DQUERYCOUNT: return copy_out(args, &query_count, sizeof(query_count));
    case D3DDDICAPS_GETD3DQUERYDATA: return copy_out(args, queries, sizeof(queries));
    case D3DDDICAPS_GETGAMMARAMPCAPS: return copy_out(args, &gamma, sizeof(gamma));
    case D3DDDICAPS_GETMULTISAMPLEQUALITYLEVELS: return answer_quality(args);
    default: return E_INVALIDARG;
    }
}

}  // namespace av::d3d9
