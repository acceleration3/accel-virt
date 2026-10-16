#include "um/d3d/d3d9_host_caps.hpp"

#include "core/trace/function_scope.hpp"
#include "icd/d3d/caps_reply.hpp"
#include "guest/kmt/kmt.hpp"
#include "proto/escape/umd_adapter_info.hpp"
#include "proto/generated/d3d9/d3d9_dispatch_client.h"


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

}  // namespace

D3d9HostCaps::D3d9HostCaps()
    : caps_{}, formats_(), rows_{}, snapshots_(false), learned_(false)
{
    AV_SCOPE("D3d9HostCaps::D3d9HostCaps");
}

bool D3d9HostCaps::learn(ClientTransport &transport, Kmt &kmt, uint32_t adapter)
{
    AV_SCOPE("D3d9HostCaps::learn");
    if (learned_) return true;
    uint32_t       hresult = 1u;
    uint64_t       caps_answered = 0u, rows_answered = 0u;
    uint32_t       asked[d3d::D3d9FormatOps::max_formats] = {};
    const uint32_t count = formats_.formats_to_ask();
    for (uint32_t index = 0u; index < count; index++) asked[index] = formats_.format_to_ask(index);
    if (!d3d9proto::av_d3d9_QueryCaps9(transport, count, asked, &hresult, &caps_, sizeof(caps_), &caps_answered,
                                       rows_, sizeof(rows_), &rows_answered) ||
        hresult != 0u || caps_answered != sizeof(caps_) || rows_answered != uint64_t{count} * sizeof(rows_[0]))
        return false;
    for (uint32_t index = 0u; index < count; index++)
        formats_.record(asked[index], rows_[index].support, rows_[index].multisample);
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
    return d3d::CapsReply(args.pData, args.DataSize).give_whole(data, bytes) ? S_OK : E_INVALIDARG;
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

/* A type past the row's (or NONE, type 0) and a format not asked have no
 * quality levels. */
HRESULT D3d9HostCaps::answer_quality(const D3DDDIARG_GETCAPS &args) const
{
    AV_SCOPE("D3d9HostCaps::answer_quality");
    if (args.pData == nullptr || args.DataSize < sizeof(DDIMULTISAMPLEQUALITYLEVELSDATA)) return E_INVALIDARG;
    auto *const    asked = static_cast<DDIMULTISAMPLEQUALITYLEVELSDATA *>(args.pData);
    const uint32_t type = static_cast<uint32_t>(asked->MsType);
    constexpr uint32_t types = sizeof(d3d9proto::FormatCaps9::quality_levels) / sizeof(uint32_t);
    asked->QualityLevels = 0u;
    if (type == 0u || type > types) return S_OK;
    for (uint32_t index = 0u; index < formats_.formats_to_ask(); index++)
        if (formats_.format_to_ask(index) == static_cast<uint32_t>(asked->Format))
            asked->QualityLevels = rows_[index].quality_levels[type - 1u];
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
