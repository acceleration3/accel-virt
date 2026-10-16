/* D3d9Device's lifetime, state, shaders, queries and state blocks
 * (d3d9_device.hpp). */
#include "um/d3d/d3d9_device.hpp"

#include "core/platform/block_source.hpp"
#include "core/log/log_line.hpp"
#include "core/trace/function_scope.hpp"
#include "core/log/log_sink.hpp"
#include "icd/common/pieces.hpp"
#include "proto/generated/d3d9/d3d9_dispatch_client.h"
#include "um/common/guest_setting.hpp"
#include "um/d3d/d3d9_host_caps.hpp"
#include "um/d3d/d3d_session.hpp"

#include <string.h>

namespace av::d3d9 {

using d3d::D3d9StateEntry;
using Kind = d3d::D3d9StateEntry::Kind;

namespace {

constexpr uint32_t stage_vertex = 0u;
/* What a lost device's line is logged once under. */
/* Calls the trace logs, and what says it is off. */
constexpr uint32_t trace_most = 20000u;
constexpr uint32_t trace_none = ~0u;
constexpr uint32_t stage_pixel = 1u;
constexpr uint32_t components = 4u;
/* D3DHAL_STATESET* (reverse-engineering.md §4.4). */
constexpr UINT stateset_begin = 0u;
constexpr UINT stateset_end = 1u;
constexpr UINT stateset_delete = 2u;
constexpr UINT stateset_execute = 3u;
constexpr UINT stateset_capture = 4u;
/* D3DDDIDEVINFOID_VCACHE's answer, as a driver that does no cache
 * optimisation of its own gives it. */
constexpr DWORD vcache_pattern = 0x48434143u;   /* 'CACH' */
/* d3d9.h's, which a driver does not include. */
constexpr uint32_t issue_end = 0x1u;              /* D3DISSUE_END */
constexpr uint32_t issue_begin = 0x2u;            /* D3DISSUE_BEGIN */
constexpr uint32_t get_data_flush = 0x1u;         /* get_data_flush */

/* GetData's answer per query type (D3DQUERYTYPE's documented sizes). */
uint32_t query_bytes(D3DDDIQUERYTYPE type)
{
    AV_SCOPE("query_bytes");
    switch (type) {
    case D3DDDIQUERYTYPE_VCACHE: return sizeof(D3DDDIDEVINFO_VCACHE);
    case D3DDDIQUERYTYPE_TIMESTAMP:
    case D3DDDIQUERYTYPE_TIMESTAMPFREQ: return sizeof(UINT64);
    default: return sizeof(DWORD);
    }
}

}  // namespace

D3d9Device::D3d9Device(d3d::D3dSession &session, BlockSource &memory, LogSink &log, const D3d9HostCaps &caps,
                       d3d::UmdAdapter &adapter)
    : session_(session), memory_(memory), log_(log), lines_(memory, log, "avumd: "), refused_(lines_), removal_(), caps_(caps), adapter_(adapter), lock_(), channel_(),
      follower_(), resources_(memory), queries_(memory), blocks_(memory), recording_(nullptr), batch_(), shadow_(),
      callbacks_{}, kernel_(), allocations_(kernel_, lines_), context_(nullptr), viewport_{}, z_range_{0.0f, 1.0f}, scissor_{},
      scissor_set_(false), streams_{}, bound_{}, indices_(nullptr), index_stride_(0u), user_indices_(nullptr),
      user_index_bytes_(0u), bound_indices_(0u), vertex_ring_{}, index16_ring_{}, index32_ring_{},
      readback_(nullptr), readback_bytes_(0u), traced_(trace_none),
      call_began_(0u), busy_(0u), calls_(0u), draws_{}, rebased_(0u), null_shaders_(0u), written_(0u)
{
    AV_SCOPE("D3d9Device::D3d9Device");
}

/* The runtime destroys its resources before the device; whatever is left is
 * the device's to give back. The host lets go of a destroyed device's
 * objects with the session. */
D3d9Device::~D3d9Device()
{
    AV_SCOPE("D3d9Device::~D3d9Device");
    const auto any = [](const D3d9Resource &) { return true; };
    for (D3d9Resource *left = resources_.find_if(any); left != nullptr; left = resources_.find_if(any)) {
        release_resource(*left);
        static_cast<void>(resources_.destroy(left));
    }
    if (channel_.constructed()) {
        const Ring *const rings[] = {&vertex_ring_, &index16_ring_, &index32_ring_};
        for (const Ring *ring : rings)
            if (ring->id != 0u) static_cast<void>(d3d9proto::av_d3d9_Destroy9(wire(), ring->id));
        static_cast<void>(channel_.object().close9());
        static_cast<void>(channel_.object().flush());
        channel_.destroy();
        session_.leave();
    }
    if (readback_ != nullptr) memory_.release(readback_);
    if (follower_.constructed()) follower_.destroy();
    kernel_.destroy_context(context_);
}

HRESULT D3d9Device::initialize(uint32_t kmt_adapter, const D3DDDIARG_CREATEDEVICE &create)
{
    AV_SCOPE("D3d9Device::initialize");
    traced_ = GuestSetting("AV_D3D9_TRACE").value().on() ? 0u : trace_none;
    LARGE_INTEGER now = {};
    QueryPerformanceCounter(&now);
    trace("device", static_cast<uint32_t>(reinterpret_cast<uintptr_t>(this)), create.Version, create.Flags.Value,
          static_cast<uint32_t>(now.QuadPart / 10000));
    if (create.pCallbacks == nullptr || create.pDeviceFuncs == nullptr) return E_INVALIDARG;
    callbacks_ = *create.pCallbacks;
    kernel_.bind(&callbacks_, create.hDevice);
    const HRESULT context = allocations_.make_context(context_);
    if (FAILED(context)) return context;
    const HRESULT joined = session_.join(kmt_adapter);
    if (FAILED(joined)) return joined;
    static_cast<void>(follower_.construct(kernel_.callbacks(), kernel_.runtime(), session_.transport()));
    static_cast<void>(channel_.construct(session_.stream(), session_.lock(), session_.next_id()));
    uint32_t hresult = 1u;
    if (!channel_.object().open9(hresult) || hresult != 0u) {
        log_.write("avumd: the host made no Direct3D 9 device");
        channel_.destroy();
        session_.leave();
        return E_FAIL;
    }
    return S_OK;
}

d3d::UmdAdapter &D3d9Device::adapter() const
{
    AV_SCOPE("D3d9Device::adapter");
    return adapter_;
}

void D3d9Device::lock_calls()
{
    AV_SCOPE("D3d9Device::lock_calls");
    lock_.acquire();
    if (traced_ < trace_most) call_began_ = microseconds();
}

void D3d9Device::unlock_calls()
{
    AV_SCOPE("D3d9Device::unlock_calls");
    if (traced_ < trace_most && call_began_ != 0u) {
        busy_ += microseconds() - call_began_;
        calls_++;
    }
    lock_.release();
}

d3d::D3dDeviceChannel &D3d9Device::wire()
{
    AV_SCOPE("D3d9Device::wire");
    return channel_.object();
}

uint32_t D3d9Device::next_id()
{
    AV_SCOPE("D3d9Device::next_id");
    return session_.next_id();
}

D3d9Resource *D3d9Device::resource(HANDLE handle) const
{
    AV_SCOPE("D3d9Device::resource");
    return handle != nullptr ? resources_.find(handle) : nullptr;
}

void D3d9Device::trace(const char *call, uint32_t a, uint32_t b, uint32_t c, uint32_t d)
{
    AV_SCOPE("D3d9Device::trace");
    if (traced_ >= trace_most) return;
    traced_++;
    LogLine line(lines_);
    line.text("d3d9 ").text(call);
    const uint32_t numbers[4] = {a, b, c, d};
    for (const uint32_t number : numbers) line.text(" ").number(number);
    line.write();
}

void D3d9Device::note_failure(const char *call, uint32_t hresult)
{
    AV_SCOPE("D3d9Device::note_failure");
    constexpr uint32_t still_drawing = 0x8876021Cu;   /* D3DERR_WASSTILLDRAWING */
    /* Said whether or not the device traces, once for each call and answer:
     * an application that retries one forever says it once. */
    if (hresult != still_drawing) refused_.failed(call, static_cast<int32_t>(hresult));
}

uint64_t D3d9Device::microseconds() const
{
    AV_SCOPE("D3d9Device::microseconds");
    LARGE_INTEGER now = {}, frequency = {};
    QueryPerformanceCounter(&now);
    QueryPerformanceFrequency(&frequency);
    if (frequency.QuadPart == 0) return 0u;
    /* Whole seconds and the ticks left over, each scaled on its own: the
     * product of the whole count and a million passes 64 bits after three
     * weeks of uptime on a 10 MHz counter. */
    const auto ticks = static_cast<uint64_t>(now.QuadPart);
    const auto per_second = static_cast<uint64_t>(frequency.QuadPart);
    return ticks / per_second * 1000000u + ticks % per_second * 1000000u / per_second;
}

void D3d9Device::trace_wait(const char *call, uint64_t started, uint32_t detail)
{
    AV_SCOPE("D3d9Device::trace_wait");
    constexpr uint64_t worth_noting = 2000u;
    if (traced_ >= trace_most) return;
    const uint64_t took = microseconds() - started;
    if (took >= worth_noting)
        trace(call, static_cast<uint32_t>(took), detail, static_cast<uint32_t>(microseconds() / 1000u), 0u);
}

uint32_t D3d9Device::id_of(HANDLE handle) const
{
    AV_SCOPE("D3d9Device::id_of");
    return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(handle));
}

HANDLE D3d9Device::handle_of(uint32_t id) const
{
    AV_SCOPE("D3d9Device::handle_of");
    return reinterpret_cast<HANDLE>(static_cast<uintptr_t>(id));
}

/* The first call that could not be sent says why; every later one follows
 * from it. */
HRESULT D3d9Device::lost(const char *call)
{
    AV_SCOPE("D3d9Device::lost");
    removal_.remove(static_cast<d3d::Hresult>(D3DDDIERR_DEVICEREMOVED));
    return static_cast<HRESULT>(removal_.check(log_, call));
}

void D3d9Device::not_implemented(const char *call)
{
    AV_SCOPE("D3d9Device::not_implemented");
    refused_.unserved(call);
}

/* ---------------------------------------------------------------- state */

/* A device's first states are every state there is, more than one command
 * carries (d3d9.toml's limits): they go in pieces of whole entries. */
bool D3d9Device::send_states()
{
    AV_SCOPE("D3d9Device::send_states");
    if (!batch_.pending()) return true;
    const bool sent =
        send_pieces(d3d9proto::av_d3d9_SetRenderStates9, batch_.render_count(), batch_.render_values(),
                    render_piece) &&
        send_pieces(d3d9proto::av_d3d9_SetSamplerStates9, batch_.sampler_count(), batch_.sampler_values(),
                    stage_piece) &&
        send_pieces(d3d9proto::av_d3d9_SetTextureStageStates9, batch_.stage_count(), batch_.stage_values(),
                    stage_piece);
    batch_.taken();
    return sent;
}

bool D3d9Device::send_pieces(StatesCommand send, uint32_t count, const uint32_t *values, uint32_t piece)
{
    AV_SCOPE("D3d9Device::send_pieces");
    icd::Pieces pieces(count, piece);
    while (pieces.next())
        if (!send(wire(), static_cast<uint32_t>(pieces.count()), values + pieces.offset())) return false;
    return true;
}

bool D3d9Device::recorded(const D3d9StateEntry &entry)
{
    AV_SCOPE("D3d9Device::recorded");
    if (recording_ == nullptr) {
        static_cast<void>(shadow_.set(entry));
        return false;
    }
    if (recording_->record(entry) != Status::ok) log_.write("avumd: a Direct3D 9 state block has no room");
    return true;
}

HRESULT D3d9Device::set_render_state(const D3DDDIARG_RENDERSTATE *args)
{
    AV_SCOPE("D3d9Device::set_render_state");
    const uint32_t value = args->Value;
    if (recorded(D3d9StateEntry(Kind::render, static_cast<uint32_t>(args->State), 0u, &value, 1u))) return S_OK;
    batch_.render_state(static_cast<uint32_t>(args->State), value);
    return S_OK;
}

HRESULT D3d9Device::set_texture_stage_state(const D3DDDIARG_TEXTURESTAGESTATE *args)
{
    AV_SCOPE("D3d9Device::set_texture_stage_state");
    const uint32_t value = args->Value;
    if (recorded(D3d9StateEntry(Kind::stage, args->Stage, static_cast<uint32_t>(args->State), &value, 1u)))
        return S_OK;
    batch_.stage_state(args->Stage, static_cast<uint32_t>(args->State), value);
    return S_OK;
}

HRESULT D3d9Device::set_texture(UINT stage, HANDLE texture)
{
    AV_SCOPE("D3d9Device::set_texture");
    if (recorded(D3d9StateEntry(Kind::texture, stage, texture, 0u, 0u))) return S_OK;
    const D3d9Resource *const bound = resource(texture);
    const uint32_t            id = bound != nullptr && bound->kind != D3d9Resource::Kind::system ? bound->id : 0u;
    return d3d9proto::av_d3d9_SetTexture9(wire(), stage, id) ? S_OK : lost(__func__);
}

HRESULT D3d9Device::set_pixel_shader(HANDLE shader)
{
    AV_SCOPE("D3d9Device::set_pixel_shader");
    if (recorded(D3d9StateEntry(Kind::pixel_shader, 0u, shader, 0u, 0u))) return S_OK;
    return d3d9proto::av_d3d9_SetPixelShader9(wire(), id_of(shader)) ? S_OK : lost(__func__);
}

HRESULT D3d9Device::set_vertex_shader_func(HANDLE shader)
{
    AV_SCOPE("D3d9Device::set_vertex_shader_func");
    if (shader == nullptr) null_shaders_++;
    if (recorded(D3d9StateEntry(Kind::vertex_shader, 0u, shader, 0u, 0u))) return S_OK;
    return d3d9proto::av_d3d9_SetVertexShader9(wire(), id_of(shader)) ? S_OK : lost(__func__);
}

HRESULT D3d9Device::set_vertex_shader_decl(HANDLE declaration)
{
    AV_SCOPE("D3d9Device::set_vertex_shader_decl");
    if (recorded(D3d9StateEntry(Kind::declaration, 0u, declaration, 0u, 0u))) return S_OK;
    return d3d9proto::av_d3d9_SetVertexDeclaration9(wire(), id_of(declaration)) ? S_OK : lost(__func__);
}

/* Constants are the shadow's and a state block's register by register, and
 * cross in one command whatever their count. */
HRESULT D3d9Device::set_pixel_shader_const(const D3DDDIARG_SETPIXELSHADERCONST *args, const FLOAT *values)
{
    AV_SCOPE("D3d9Device::set_pixel_shader_const");
    bool kept = false;
    for (UINT index = 0u; index < args->Count; index++)
        kept = recorded(D3d9StateEntry(Kind::constant_f, stage_pixel, args->Register + index,
                                       reinterpret_cast<const uint32_t *>(values + index * components), components));
    if (kept) return S_OK;
    return d3d9proto::av_d3d9_SetConstantsF9(wire(), stage_pixel, args->Register, args->Count * components, values)
               ? S_OK
               : lost(__func__);
}

HRESULT D3d9Device::set_vertex_shader_const(const D3DDDIARG_SETVERTEXSHADERCONST *args, const void *values)
{
    AV_SCOPE("D3d9Device::set_vertex_shader_const");
    const auto *const floats = static_cast<const FLOAT *>(values);
    bool              kept = false;
    for (UINT index = 0u; index < args->Count; index++)
        kept = recorded(D3d9StateEntry(Kind::constant_f, stage_vertex, args->Register + index,
                                       reinterpret_cast<const uint32_t *>(floats + index * components), components));
    if (kept) return S_OK;
    return d3d9proto::av_d3d9_SetConstantsF9(wire(), stage_vertex, args->Register, args->Count * components, floats)
               ? S_OK
               : lost(__func__);
}

HRESULT D3d9Device::set_vertex_shader_const_i(const D3DDDIARG_SETVERTEXSHADERCONSTI *args, const INT *values)
{
    AV_SCOPE("D3d9Device::set_vertex_shader_const_i");
    bool kept = false;
    for (UINT index = 0u; index < args->Count; index++)
        kept = recorded(D3d9StateEntry(Kind::constant_i, stage_vertex, args->Register + index,
                                       reinterpret_cast<const uint32_t *>(values + index * components), components));
    if (kept) return S_OK;
    return d3d9proto::av_d3d9_SetConstantsI9(wire(), stage_vertex, args->Register, args->Count * components,
                                             reinterpret_cast<const int32_t *>(values))
               ? S_OK
               : lost(__func__);
}

HRESULT D3d9Device::set_pixel_shader_const_i(const D3DDDIARG_SETPIXELSHADERCONSTI *args, const INT *values)
{
    AV_SCOPE("D3d9Device::set_pixel_shader_const_i");
    bool kept = false;
    for (UINT index = 0u; index < args->Count; index++)
        kept = recorded(D3d9StateEntry(Kind::constant_i, stage_pixel, args->Register + index,
                                       reinterpret_cast<const uint32_t *>(values + index * components), components));
    if (kept) return S_OK;
    return d3d9proto::av_d3d9_SetConstantsI9(wire(), stage_pixel, args->Register, args->Count * components,
                                             reinterpret_cast<const int32_t *>(values))
               ? S_OK
               : lost(__func__);
}

HRESULT D3d9Device::set_vertex_shader_const_b(const D3DDDIARG_SETVERTEXSHADERCONSTB *args, const BOOL *values)
{
    AV_SCOPE("D3d9Device::set_vertex_shader_const_b");
    bool kept = false;
    for (UINT index = 0u; index < args->Count; index++)
        kept = recorded(D3d9StateEntry(Kind::constant_b, stage_vertex, args->Register + index,
                                       reinterpret_cast<const uint32_t *>(values + index), 1u));
    if (kept) return S_OK;
    return d3d9proto::av_d3d9_SetConstantsB9(wire(), stage_vertex, args->Register, args->Count,
                                             reinterpret_cast<const uint32_t *>(values))
               ? S_OK
               : lost(__func__);
}

HRESULT D3d9Device::set_pixel_shader_const_b(const D3DDDIARG_SETPIXELSHADERCONSTB *args, const BOOL *values)
{
    AV_SCOPE("D3d9Device::set_pixel_shader_const_b");
    bool kept = false;
    for (UINT index = 0u; index < args->Count; index++)
        kept = recorded(D3d9StateEntry(Kind::constant_b, stage_pixel, args->Register + index,
                                       reinterpret_cast<const uint32_t *>(values + index), 1u));
    if (kept) return S_OK;
    return d3d9proto::av_d3d9_SetConstantsB9(wire(), stage_pixel, args->Register, args->Count,
                                             reinterpret_cast<const uint32_t *>(values))
               ? S_OK
               : lost(__func__);
}

HRESULT D3d9Device::set_stream_source(const D3DDDIARG_SETSTREAMSOURCE *args)
{
    AV_SCOPE("D3d9Device::set_stream_source");
    if (args->Stream >= streams) return E_INVALIDARG;
    if (recorded(D3d9StateEntry(Kind::stream, args->Stream, args->hVertexBuffer, args->Offset, args->Stride)))
        return S_OK;
    streams_[args->Stream] = Stream{resource(args->hVertexBuffer), nullptr, args->Offset, args->Stride};
    return S_OK;
}

/* A user pointer is read at the draw: the runtime keeps it valid until then. */
HRESULT D3d9Device::set_stream_source_um(const D3DDDIARG_SETSTREAMSOURCEUM *args, const void *data)
{
    AV_SCOPE("D3d9Device::set_stream_source_um");
    if (args->Stream >= streams) return E_INVALIDARG;
    streams_[args->Stream] = Stream{nullptr, data, 0u, args->Stride};
    return S_OK;
}

HRESULT D3d9Device::set_indices(const D3DDDIARG_SETINDICES *args)
{
    AV_SCOPE("D3d9Device::set_indices");
    if (recorded(D3d9StateEntry(Kind::indices, 0u, args->hIndexBuffer, args->Stride, 0u))) return S_OK;
    indices_ = resource(args->hIndexBuffer);
    index_stride_ = args->Stride;
    user_indices_ = nullptr;
    return S_OK;
}

HRESULT D3d9Device::set_indices_um(UINT index_bytes, const void *data)
{
    AV_SCOPE("D3d9Device::set_indices_um");
    user_indices_ = data;
    user_index_bytes_ = index_bytes;
    return S_OK;
}

HRESULT D3d9Device::set_stream_source_freq(const D3DDDIARG_SETSTREAMSOURCEFREQ *args)
{
    AV_SCOPE("D3d9Device::set_stream_source_freq");
    return d3d9proto::av_d3d9_SetStreamSourceFreq9(wire(), args->Stream, args->Divider) ? S_OK : lost(__func__);
}

/* The DDI sets the viewport's rectangle and its depth range apart; the API
 * sets them together. */
HRESULT D3d9Device::set_viewport(const D3DDDIARG_VIEWPORTINFO *args)
{
    AV_SCOPE("D3d9Device::set_viewport");
    const uint32_t words[4] = {args->X, args->Y, args->Width, args->Height};
    if (recorded(D3d9StateEntry(Kind::viewport, 0u, 0u, words, 4u))) return S_OK;
    viewport_ = *args;
    trace("viewport", args->X, args->Y, args->Width, args->Height);
    return send_viewport();
}

HRESULT D3d9Device::set_z_range(const D3DDDIARG_ZRANGE *args)
{
    AV_SCOPE("D3d9Device::set_z_range");
    if (recorded(D3d9StateEntry(Kind::z_range, 0u, 0u, reinterpret_cast<const uint32_t *>(args), 2u))) return S_OK;
    z_range_ = *args;
    return send_viewport();
}

HRESULT D3d9Device::send_viewport()
{
    AV_SCOPE("D3d9Device::send_viewport");
    const d3d9proto::Viewport viewport = {viewport_.X, viewport_.Y, viewport_.Width, viewport_.Height, z_range_.MinZ,
                                          z_range_.MaxZ};
    return d3d9proto::av_d3d9_SetViewport9(wire(), viewport) ? S_OK : lost(__func__);
}

HRESULT D3d9Device::set_scissor_rect(const RECT *rect)
{
    AV_SCOPE("D3d9Device::set_scissor_rect");
    if (recorded(D3d9StateEntry(Kind::scissor, 0u, 0u, reinterpret_cast<const uint32_t *>(rect), 4u))) return S_OK;
    scissor_ = *rect;
    scissor_set_ = true;
    trace("scissor", static_cast<uint32_t>(rect->left), static_cast<uint32_t>(rect->top),
          static_cast<uint32_t>(rect->right), static_cast<uint32_t>(rect->bottom));
    return send_scissor();
}

HRESULT D3d9Device::send_scissor()
{
    AV_SCOPE("D3d9Device::send_scissor");
    if (!scissor_set_) return S_OK;
    const d3d9proto::Rect rect = {scissor_.left, scissor_.top, scissor_.right, scissor_.bottom};
    return d3d9proto::av_d3d9_SetScissorRect9(wire(), rect) ? S_OK : lost(__func__);
}

HRESULT D3d9Device::set_clip_plane(const D3DDDIARG_SETCLIPPLANE *args)
{
    AV_SCOPE("D3d9Device::set_clip_plane");
    if (recorded(D3d9StateEntry(Kind::clip_plane, args->Index, 0u, reinterpret_cast<const uint32_t *>(args->Plane),
                                components)))
        return S_OK;
    return d3d9proto::av_d3d9_SetClipPlane9(wire(), args->Index, components, args->Plane) ? S_OK : lost(__func__);
}

/* The API's SetRenderTarget resets the viewport and scissor to the target;
 * the DDI's does not, so the device's own are put back after it. */
HRESULT D3d9Device::set_render_target(const D3DDDIARG_SETRENDERTARGET *args)
{
    AV_SCOPE("D3d9Device::set_render_target");
    if (args->RenderTargetIndex >= render_targets) return E_INVALIDARG;
    uint32_t                  id = 0u, subresource = 0u;
    const D3d9Resource *const target = resource(args->hRenderTarget);
    if (target != nullptr && !surface_of(*target, args->SubResourceIndex, id, subresource)) return E_INVALIDARG;
    trace("target", args->RenderTargetIndex, id, target != nullptr ? target->subresources[args->SubResourceIndex].width : 0u,
          target != nullptr ? target->subresources[args->SubResourceIndex].height : 0u);
    if (!send_states() || !d3d9proto::av_d3d9_SetRenderTarget9(wire(), args->RenderTargetIndex, id, subresource))
        return lost(__func__);
    const HRESULT viewport = send_viewport();
    return FAILED(viewport) ? viewport : send_scissor();
}

HRESULT D3d9Device::set_depth_stencil(const D3DDDIARG_SETDEPTHSTENCIL *args)
{
    AV_SCOPE("D3d9Device::set_depth_stencil");
    uint32_t                  id = 0u, subresource = 0u;
    const D3d9Resource *const target = resource(args->hZBuffer);
    if (target != nullptr && !surface_of(*target, 0u, id, subresource)) return E_INVALIDARG;
    trace("depth", id, target != nullptr ? target->subresources[0].width : 0u,
          target != nullptr ? target->subresources[0].height : 0u, 0u);
    return d3d9proto::av_d3d9_SetDepthStencil9(wire(), id, subresource) ? S_OK : lost(__func__);
}

/* ---------------------------------------------------------------- shaders */

HRESULT D3d9Device::create_vertex_shader_func(D3DDDIARG_CREATEVERTEXSHADERFUNC *args, const UINT *code)
{
    AV_SCOPE("D3d9Device::create_vertex_shader_func");
    const uint32_t id = next_id();
    if (!d3d9proto::av_d3d9_CreateVertexShader9(wire(), id, reinterpret_cast<const uint8_t *>(code), args->Size))
        return lost(__func__);
    args->ShaderHandle = handle_of(id);
    return S_OK;
}

HRESULT D3d9Device::delete_vertex_shader_func(HANDLE shader)
{
    AV_SCOPE("D3d9Device::delete_vertex_shader_func");
    return d3d9proto::av_d3d9_Destroy9(wire(), id_of(shader)) ? S_OK : lost(__func__);
}

HRESULT D3d9Device::create_pixel_shader(D3DDDIARG_CREATEPIXELSHADER *args, const UINT *code)
{
    AV_SCOPE("D3d9Device::create_pixel_shader");
    const uint32_t id = next_id();
    if (!d3d9proto::av_d3d9_CreatePixelShader9(wire(), id, reinterpret_cast<const uint8_t *>(code), args->CodeSize))
        return lost(__func__);
    args->ShaderHandle = handle_of(id);
    return S_OK;
}

HRESULT D3d9Device::delete_pixel_shader(HANDLE shader)
{
    AV_SCOPE("D3d9Device::delete_pixel_shader");
    return d3d9proto::av_d3d9_Destroy9(wire(), id_of(shader)) ? S_OK : lost(__func__);
}

/* The runtime may or may not end the list with D3DDECL_END (stream 0xff). */
HRESULT D3d9Device::create_vertex_shader_decl(D3DDDIARG_CREATEVERTEXSHADERDECL *args,
                                              const D3DDDIVERTEXELEMENT *elements)
{
    AV_SCOPE("D3d9Device::create_vertex_shader_decl");
    constexpr uint32_t most = 64u;
    constexpr WORD     end_stream = 0xffu;
    d3d9proto::VertexElement list[most] = {};
    uint32_t                 count = 0u;
    for (UINT index = 0u; index < args->NumVertexElements && elements[index].Stream != end_stream; index++) {
        if (count == most) return E_INVALIDARG;
        const D3DDDIVERTEXELEMENT &element = elements[index];
        list[count++] = d3d9proto::VertexElement{element.Stream, element.Offset, element.Type, element.Method,
                                                 element.Usage,  element.UsageIndex};
    }
    const uint32_t id = next_id();
    if (count != 0u && !d3d9proto::av_d3d9_CreateVertexDeclaration9(wire(), id, count, list)) return lost(__func__);
    args->ShaderHandle = handle_of(id);
    return S_OK;
}

HRESULT D3d9Device::delete_vertex_shader_decl(HANDLE declaration)
{
    AV_SCOPE("D3d9Device::delete_vertex_shader_decl");
    return d3d9proto::av_d3d9_Destroy9(wire(), id_of(declaration)) ? S_OK : lost(__func__);
}

/* ---------------------------------------------------------------- queries */

HRESULT D3d9Device::create_query(D3DDDIARG_CREATEQUERY *args)
{
    AV_SCOPE("D3d9Device::create_query");
    D3d9Query *made = nullptr;
    if (queries_.create(made) != Status::ok) return E_OUTOFMEMORY;
    *made = D3d9Query{next_id(), args->QueryType, query_bytes(args->QueryType)};
    if (!d3d9proto::av_d3d9_CreateQuery9(wire(), made->id, static_cast<uint32_t>(args->QueryType))) {
        static_cast<void>(queries_.destroy(made));
        return lost(__func__);
    }
    args->hQuery = made;
    return S_OK;
}

HRESULT D3d9Device::destroy_query(const HANDLE handle)
{
    AV_SCOPE("D3d9Device::destroy_query");
    D3d9Query *const query = queries_.find(handle);
    if (query == nullptr) return E_INVALIDARG;
    const bool sent = d3d9proto::av_d3d9_Destroy9(wire(), query->id);
    static_cast<void>(queries_.destroy(query));
    return sent ? S_OK : lost(__func__);
}

HRESULT D3d9Device::issue_query(const D3DDDIARG_ISSUEQUERY *args)
{
    AV_SCOPE("D3d9Device::issue_query");
    const D3d9Query *const query = queries_.find(args->hQuery);
    if (query == nullptr) return E_INVALIDARG;
    const uint32_t flags = args->Flags.Begin != 0u ? issue_begin : issue_end;
    if (!send_states() || !d3d9proto::av_d3d9_IssueQuery9(wire(), query->id, flags)) return lost(__func__);
    return S_OK;
}

/* S_FALSE until the host has the answer; never blocking. */
HRESULT D3d9Device::get_query_data(const D3DDDIARG_GETQUERYDATA *args)
{
    AV_SCOPE("D3d9Device::get_query_data");
    const D3d9Query *const query = queries_.find(args->hQuery);
    if (query == nullptr) return E_INVALIDARG;
    uint8_t  answer[sizeof(D3DDDIDEVINFO_VCACHE)] = {};
    uint32_t hresult = 1u;
    uint64_t answered = 0u;
    const uint64_t started = microseconds();
    if (!d3d9proto::av_d3d9_GetQueryData9(wire(), query->id, get_data_flush, &hresult, answer, sizeof(answer),
                                          &answered))
        return lost(__func__);
    trace_wait("wait-query", started, static_cast<uint32_t>(query->type));
    if (hresult != S_OK) return hresult == S_FALSE ? S_FALSE : static_cast<HRESULT>(hresult);
    if (args->pData != nullptr && answered >= query->data_bytes) memcpy(args->pData, answer, query->data_bytes);
    return S_OK;
}

/* ---------------------------------------------------------------- state blocks */

HRESULT D3d9Device::apply(const D3d9StateEntry &entry)
{
    AV_SCOPE("D3d9Device::apply");
    const uint32_t *const value = entry.value();
    switch (entry.kind()) {
    case Kind::render: {
        const D3DDDIARG_RENDERSTATE args = {static_cast<D3DDDIRENDERSTATETYPE>(entry.a()), value[0]};
        return set_render_state(&args);
    }
    case Kind::stage: {
        const D3DDDIARG_TEXTURESTAGESTATE args = {entry.a(), static_cast<D3DDDITEXTURESTAGESTATETYPE>(entry.b()),
                                                  value[0]};
        return set_texture_stage_state(&args);
    }
    case Kind::texture: return set_texture(entry.a(), entry.handle());
    case Kind::vertex_shader: return set_vertex_shader_func(entry.handle());
    case Kind::pixel_shader: return set_pixel_shader(entry.handle());
    case Kind::declaration: return set_vertex_shader_decl(entry.handle());
    case Kind::constant_f:
    case Kind::constant_i:
    case Kind::constant_b: return apply_constant(entry);
    case Kind::stream: {
        const D3DDDIARG_SETSTREAMSOURCE args = {entry.a(), entry.handle(), value[2], value[3]};
        return set_stream_source(&args);
    }
    case Kind::indices: {
        const D3DDDIARG_SETINDICES args = {entry.handle(), value[2]};
        return set_indices(&args);
    }
    case Kind::viewport: {
        const D3DDDIARG_VIEWPORTINFO args = {value[0], value[1], value[2], value[3]};
        return set_viewport(&args);
    }
    case Kind::z_range: return set_z_range(reinterpret_cast<const D3DDDIARG_ZRANGE *>(value));
    case Kind::scissor: return set_scissor_rect(reinterpret_cast<const RECT *>(value));
    case Kind::clip_plane: {
        D3DDDIARG_SETCLIPPLANE args = {entry.a(), {}};
        memcpy(args.Plane, value, sizeof(args.Plane));
        return set_clip_plane(&args);
    }
    case Kind::none: break;
    }
    return S_OK;
}

HRESULT D3d9Device::apply_constant(const D3d9StateEntry &entry)
{
    AV_SCOPE("D3d9Device::apply_constant");
    const uint32_t *const value = entry.value();
    const bool            vertex = entry.a() == stage_vertex;
    if (entry.kind() == Kind::constant_f) {
        const D3DDDIARG_SETVERTEXSHADERCONST vertex_args = {entry.b(), 1u};
        const D3DDDIARG_SETPIXELSHADERCONST  pixel_args = {entry.b(), 1u};
        return vertex ? set_vertex_shader_const(&vertex_args, value)
                      : set_pixel_shader_const(&pixel_args, reinterpret_cast<const FLOAT *>(value));
    }
    if (entry.kind() == Kind::constant_i) {
        const D3DDDIARG_SETVERTEXSHADERCONSTI vertex_args = {entry.b(), 1u};
        const D3DDDIARG_SETPIXELSHADERCONSTI  pixel_args = {entry.b(), 1u};
        return vertex ? set_vertex_shader_const_i(&vertex_args, reinterpret_cast<const INT *>(value))
                      : set_pixel_shader_const_i(&pixel_args, reinterpret_cast<const INT *>(value));
    }
    const D3DDDIARG_SETVERTEXSHADERCONSTB vertex_args = {entry.b(), 1u};
    const D3DDDIARG_SETPIXELSHADERCONSTB  pixel_args = {entry.b(), 1u};
    return vertex ? set_vertex_shader_const_b(&vertex_args, reinterpret_cast<const BOOL *>(value))
                  : set_pixel_shader_const_b(&pixel_args, reinterpret_cast<const BOOL *>(value));
}

/* BEGIN starts a block the calls up to END record into, handed back at END;
 * EXECUTE applies a block, CAPTURE refreshes it from the device as it is now,
 * DELETE lets it go (reverse-engineering.md §4.4). */
HRESULT D3d9Device::state_set(D3DDDIARG_STATESET *args)
{
    AV_SCOPE("D3d9Device::state_set");
    d3d::D3d9StateBlock *const named =
        args->Operation == stateset_begin ? nullptr : blocks_.find(static_cast<void *>(args->hStateSet));
    trace("stateset", args->Operation, static_cast<uint32_t>(args->StateBlockType), named != nullptr ? named->count() : 0u,
          recording_ != nullptr ? recording_->count() : 0u);
    switch (args->Operation) {
    case stateset_begin: {
        if (recording_ != nullptr) return E_INVALIDARG;
        d3d::D3d9StateBlock *made = nullptr;
        if (blocks_.create(made, memory_) != Status::ok) return E_OUTOFMEMORY;
        recording_ = made;
        return S_OK;
    }
    case stateset_end:
        if (recording_ == nullptr) return E_INVALIDARG;
        args->hStateSet = recording_;
        recording_ = nullptr;
        return S_OK;
    case stateset_delete:
        if (named == nullptr) return E_INVALIDARG;
        static_cast<void>(blocks_.destroy(named));
        return S_OK;
    case stateset_execute:
        if (named == nullptr) return E_INVALIDARG;
        for (uint32_t index = 0u; index < named->count(); index++) {
            const HRESULT applied = apply(named->at(index));
            if (FAILED(applied)) return applied;
        }
        return S_OK;
    case stateset_capture:
        if (named == nullptr) return E_INVALIDARG;
        named->capture(shadow_);
        return S_OK;
    default: not_implemented("a Direct3D 9 state block operation it has no use for"); return S_OK;
    }
}

/* ---------------------------------------------------------------- the rest */

HRESULT D3d9Device::update_w_info(const D3DDDIARG_WINFO *)
{
    AV_SCOPE("D3d9Device::update_w_info");
    return S_OK;
}

HRESULT D3d9Device::validate_device(D3DDDIARG_VALIDATETEXTURESTAGESTATE *args)
{
    AV_SCOPE("D3d9Device::validate_device");
    args->NumPasses = 1u;
    return S_OK;
}

HRESULT D3d9Device::get_info(UINT info, void *data, UINT bytes)
{
    AV_SCOPE("D3d9Device::get_info");
    if (info != D3DDDIDEVINFOID_VCACHE || data == nullptr || bytes < sizeof(D3DDDIDEVINFO_VCACHE)) return E_FAIL;
    D3DDDIDEVINFO_VCACHE answer = {};
    answer.Pattern = vcache_pattern;
    memcpy(data, &answer, sizeof(answer));
    return S_OK;
}

/* With hardware T&L and shaders at model 2 or later, the runtime turns fixed
 * function into shaders and consumes these itself (reverse-engineering.md
 * §4.5); one that still arrives changes nothing the shaders read. */
HRESULT D3d9Device::multiply_transform(const D3DDDIARG_MULTIPLYTRANSFORM *)
{
    AV_SCOPE("D3d9Device::multiply_transform");
    return S_OK;
}

HRESULT D3d9Device::set_transform(const D3DDDIARG_SETTRANSFORM *)
{
    AV_SCOPE("D3d9Device::set_transform");
    return S_OK;
}

HRESULT D3d9Device::set_material(const D3DDDIARG_SETMATERIAL *)
{
    AV_SCOPE("D3d9Device::set_material");
    return S_OK;
}

HRESULT D3d9Device::set_light(const D3DDDIARG_SETLIGHT *, const D3DDDI_LIGHT *)
{
    AV_SCOPE("D3d9Device::set_light");
    return S_OK;
}

HRESULT D3d9Device::create_light(const D3DDDIARG_CREATELIGHT *)
{
    AV_SCOPE("D3d9Device::create_light");
    return S_OK;
}

HRESULT D3d9Device::destroy_light(const D3DDDIARG_DESTROYLIGHT *)
{
    AV_SCOPE("D3d9Device::destroy_light");
    return S_OK;
}

HRESULT D3d9Device::set_priority(const D3DDDIARG_SETPRIORITY *)
{
    AV_SCOPE("D3d9Device::set_priority");
    return S_OK;
}

HRESULT D3d9Device::update_palette(const D3DDDIARG_UPDATEPALETTE *, const PALETTEENTRY *)
{
    AV_SCOPE("D3d9Device::update_palette");
    return S_OK;
}

HRESULT D3d9Device::set_palette(const D3DDDIARG_SETPALETTE *)
{
    AV_SCOPE("D3d9Device::set_palette");
    return S_OK;
}

HRESULT D3d9Device::set_convolution_kernel_mono(const D3DDDIARG_SETCONVOLUTIONKERNELMONO *)
{
    AV_SCOPE("D3d9Device::set_convolution_kernel_mono");
    return S_OK;
}

HRESULT D3d9Device::compose_rects(const D3DDDIARG_COMPOSERECTS *)
{
    AV_SCOPE("D3d9Device::compose_rects");
    not_implemented("ComposeRects, which no cap offers, was called");
    return S_OK;
}

HRESULT D3d9Device::query_resource_residency(const D3DDDIARG_QUERYRESOURCERESIDENCY *)
{
    AV_SCOPE("D3d9Device::query_resource_residency");
    return S_OK;
}

/* No patches are offered (D3DDEVCAPS_RTPATCHES, NPATCHES), so none is drawn. */
HRESULT D3d9Device::draw_rect_patch(const D3DDDIARG_DRAWRECTPATCH *, const D3DDDIRECTPATCH_INFO *, const FLOAT *)
{
    AV_SCOPE("D3d9Device::draw_rect_patch");
    return S_OK;
}

HRESULT D3d9Device::draw_tri_patch(const D3DDDIARG_DRAWTRIPATCH *, const D3DDDITRIPATCH_INFO *, const FLOAT *)
{
    AV_SCOPE("D3d9Device::draw_tri_patch");
    return S_OK;
}

/* The video, extension and overlay devices are asked for only where caps
 * offer them, and none are offered. */
HRESULT D3d9Device::create_decode_device(D3DDDIARG_CREATEDECODEDEVICE *)
{
    AV_SCOPE("D3d9Device::create_decode_device");
    return E_NOTIMPL;
}

HRESULT D3d9Device::destroy_decode_device(HANDLE)
{
    AV_SCOPE("D3d9Device::destroy_decode_device");
    return E_NOTIMPL;
}

HRESULT D3d9Device::set_decode_render_target(const D3DDDIARG_SETDECODERENDERTARGET *)
{
    AV_SCOPE("D3d9Device::set_decode_render_target");
    return E_NOTIMPL;
}

HRESULT D3d9Device::decode_begin_frame(D3DDDIARG_DECODEBEGINFRAME *)
{
    AV_SCOPE("D3d9Device::decode_begin_frame");
    return E_NOTIMPL;
}

HRESULT D3d9Device::decode_end_frame(D3DDDIARG_DECODEENDFRAME *)
{
    AV_SCOPE("D3d9Device::decode_end_frame");
    return E_NOTIMPL;
}

HRESULT D3d9Device::decode_execute(const D3DDDIARG_DECODEEXECUTE *)
{
    AV_SCOPE("D3d9Device::decode_execute");
    return E_NOTIMPL;
}

HRESULT D3d9Device::decode_extension_execute(const D3DDDIARG_DECODEEXTENSIONEXECUTE *)
{
    AV_SCOPE("D3d9Device::decode_extension_execute");
    return E_NOTIMPL;
}

HRESULT D3d9Device::create_video_process_device(D3DDDIARG_CREATEVIDEOPROCESSDEVICE *)
{
    AV_SCOPE("D3d9Device::create_video_process_device");
    return E_NOTIMPL;
}

HRESULT D3d9Device::destroy_video_process_device(HANDLE)
{
    AV_SCOPE("D3d9Device::destroy_video_process_device");
    return E_NOTIMPL;
}

HRESULT D3d9Device::video_process_begin_frame(HANDLE)
{
    AV_SCOPE("D3d9Device::video_process_begin_frame");
    return E_NOTIMPL;
}

HRESULT D3d9Device::video_process_end_frame(D3DDDIARG_VIDEOPROCESSENDFRAME *)
{
    AV_SCOPE("D3d9Device::video_process_end_frame");
    return E_NOTIMPL;
}

HRESULT D3d9Device::set_video_process_render_target(const D3DDDIARG_SETVIDEOPROCESSRENDERTARGET *)
{
    AV_SCOPE("D3d9Device::set_video_process_render_target");
    return E_NOTIMPL;
}

HRESULT D3d9Device::video_process_blt(const D3DDDIARG_VIDEOPROCESSBLT *)
{
    AV_SCOPE("D3d9Device::video_process_blt");
    return E_NOTIMPL;
}

HRESULT D3d9Device::create_extension_device(D3DDDIARG_CREATEEXTENSIONDEVICE *)
{
    AV_SCOPE("D3d9Device::create_extension_device");
    return E_NOTIMPL;
}

HRESULT D3d9Device::destroy_extension_device(HANDLE)
{
    AV_SCOPE("D3d9Device::destroy_extension_device");
    return E_NOTIMPL;
}

HRESULT D3d9Device::extension_execute(const D3DDDIARG_EXTENSIONEXECUTE *)
{
    AV_SCOPE("D3d9Device::extension_execute");
    return E_NOTIMPL;
}

HRESULT D3d9Device::create_overlay(D3DDDIARG_CREATEOVERLAY *)
{
    AV_SCOPE("D3d9Device::create_overlay");
    return E_NOTIMPL;
}

HRESULT D3d9Device::update_overlay(const D3DDDIARG_UPDATEOVERLAY *)
{
    AV_SCOPE("D3d9Device::update_overlay");
    return E_NOTIMPL;
}

HRESULT D3d9Device::flip_overlay(const D3DDDIARG_FLIPOVERLAY *)
{
    AV_SCOPE("D3d9Device::flip_overlay");
    return E_NOTIMPL;
}

HRESULT D3d9Device::get_overlay_color_controls(D3DDDIARG_GETOVERLAYCOLORCONTROLS *)
{
    AV_SCOPE("D3d9Device::get_overlay_color_controls");
    return E_NOTIMPL;
}

HRESULT D3d9Device::set_overlay_color_controls(const D3DDDIARG_SETOVERLAYCOLORCONTROLS *)
{
    AV_SCOPE("D3d9Device::set_overlay_color_controls");
    return E_NOTIMPL;
}

HRESULT D3d9Device::destroy_overlay(const D3DDDIARG_DESTROYOVERLAY *)
{
    AV_SCOPE("D3d9Device::destroy_overlay");
    return E_NOTIMPL;
}

HRESULT D3d9Device::resolve_shared_resource(const D3DDDIARG_RESOLVESHAREDRESOURCE *)
{
    AV_SCOPE("D3d9Device::resolve_shared_resource");
    return S_OK;
}

HRESULT D3d9Device::discard(const D3DDDIARG_DISCARD *)
{
    AV_SCOPE("D3d9Device::discard");
    return S_OK;
}

HRESULT D3d9Device::offer_resources(const D3DDDIARG_OFFERRESOURCES *)
{
    AV_SCOPE("D3d9Device::offer_resources");
    return S_OK;
}

HRESULT D3d9Device::reclaim_resources(const D3DDDIARG_RECLAIMRESOURCES *)
{
    AV_SCOPE("D3d9Device::reclaim_resources");
    return S_OK;
}

HRESULT D3d9Device::check_direct_flip_support(D3DDDIARG_CHECKDIRECTFLIPSUPPORT *args)
{
    AV_SCOPE("D3d9Device::check_direct_flip_support");
    args->Supported = FALSE;
    return S_OK;
}

HRESULT D3d9Device::check_multi_plane_overlay_support(D3DDDIARG_CHECKMULTIPLANEOVERLAYSUPPORT *args)
{
    AV_SCOPE("D3d9Device::check_multi_plane_overlay_support");
    args->Supported = FALSE;
    return S_OK;
}

HRESULT D3d9Device::present_multi_plane_overlay(const D3DDDIARG_PRESENTMULTIPLANEOVERLAY *)
{
    AV_SCOPE("D3d9Device::present_multi_plane_overlay");
    return E_NOTIMPL;
}

HRESULT D3d9Device::check_present_duration_support(D3DDDIARG_CHECKPRESENTDURATIONSUPPORT *args)
{
    AV_SCOPE("D3d9Device::check_present_duration_support");
    /* No duration but the refresh's own: none closer either side. */
    args->ClosestSmallerDuration = 0u;
    args->ClosestLargerDuration = 0u;
    return S_OK;
}

}  // namespace av::d3d9
