#include "um/d3d/d3d11_device.hpp"

#include "core/platform/block_source.hpp"
#include "core/support/text_buffer.hpp"
#include "core/trace/function_scope.hpp"
#include "icd/common/log_sink.hpp"
#include "icd/common/thread_lock.hpp"
#include "icd/d3d/device_table.hpp"
#include "proto/client_blob_arena.hpp"
#include "proto/context/sub_api.hpp"
#include "proto/d3d/dxgi_format_layout.hpp"
#include "proto/generated/d3d/d3d_dispatch_client.h"
#include "um/d3d/d3d11_dxgi_thunks.hpp"
#include "um/d3d/d3d11_host_caps.hpp"
#include "um/d3d/d3d_session.hpp"
#include "um/d3d/device_handle.hpp"

extern "C" VOID APIENTRY rvkumd_d3d11_DestroyDeferredContext(D3D10DDI_HDEVICE device);

namespace av::d3d11 {

namespace {

/* D3D_FEATURE_LEVEL by the DDI's pipeline level, which CreateDevice's flags
 * carry. */
constexpr uint32_t feature_levels[] = {0xa000u, 0xa100u, 0xb000u, 0xb100u, 0x9100u,
                                       0x9200u, 0x9300u, 0xc000u, 0xc100u};

/* Past this the host's answer crosses through the blob arena rather than the
 * reply slot. */
constexpr size_t inline_limit = 32u * 1024u;
/* The most one UpdateSubresource carries: half the session's blob arena, so
 * a piece always fits once the arena is emptied. */
constexpr size_t upload_piece = 16u << 20u;
/* D3D11_COPY_DISCARD: only the first piece of an upload may discard. */
constexpr uint32_t copy_discard = 0x2u;

}  // namespace

D3d11Device::D3d11Device(d3d::D3dSession &session, BlockSource &memory, icd::LogSink &log, const D3d11HostCaps &caps)
    : session_(session), memory_(memory), log_(log), caps_(caps), runtime_{}, core_layer_{}, kernel_(nullptr),
      core_(nullptr), dxgi_(nullptr), channel_(), parent_(nullptr), recording_lock_(), calls_(), calls_owner_(0u), calls_depth_(0u), recording_(),
      recording_block_(nullptr), context_(nullptr), paging_(), follower_(), feature_level_(0u),
      guest_surfaces_{}, guest_surface_count_(0u), guest_lock_refused_(false), present_refused_(false), follow_refused_(false), frame_(1u)
{
    AV_SCOPE("D3d11Device::D3d11Device");
}

D3d11Device::~D3d11Device()
{
    AV_SCOPE("D3d11Device::~D3d11Device");
    if (parent_ != nullptr) {
        if (channel_.constructed()) channel_.destroy();
        if (recording_.constructed()) recording_.destroy();
        if (recording_block_ != nullptr) memory_.release(recording_block_);
        return;
    }
    if (channel_.constructed()) {
        static_cast<void>(channel_.object().close());
        static_cast<void>(channel_.object().flush());
        channel_.destroy();
        session_.leave();
    }
    if (paging_.constructed()) paging_.destroy();
    follower_.destroy();
    if (context_ != nullptr && kernel_ != nullptr) {
        D3DDDICB_DESTROYCONTEXT destroy = {};
        destroy.hContext = context_;
        kernel_->pfnDestroyContextCb(runtime_.handle, &destroy);
    }
}

HRESULT D3d11Device::initialize(uint32_t adapter, const D3D10DDIARG_CREATEDEVICE &create)
{
    AV_SCOPE("D3d11Device::initialize");
    runtime_ = create.hRTDevice;
    core_layer_ = create.hRTCoreLayer;
    kernel_ = create.pKTCallbacks;
    core_ = create.p11UMCallbacks;
    dxgi_ = create.DXGIBaseDDI.pDXGIBaseCallbacks;
    static_cast<void>(paging_.construct(*kernel_, runtime_.handle));
    static_cast<void>(follower_.construct(*kernel_, runtime_.handle, session_.transport()));
    const HRESULT context = make_context();
    if (FAILED(context)) return context;
    const uint32_t level = D3D11DDI_EXTRACT_3DPIPELINELEVEL_FROM_FLAGS(create.Flags);
    feature_level_ = level < sizeof(feature_levels) / sizeof(feature_levels[0]) ? feature_levels[level] : 0u;
    const HRESULT joined = session_.join(adapter);
    if (FAILED(joined)) return joined;
    static_cast<void>(channel_.construct(session_.stream(), session_.lock(), session_.next_id()));
    uint32_t hresult = 1u, made = 0u;
    if (!channel_.object().open(feature_level_, hresult, made) || hresult != 0u) {
        log_.write("rvkumd: the host made no Direct3D 11 device");
        channel_.destroy();
        session_.leave();
        return E_FAIL;
    }
    session_.query_results().bind(channel_.object());
    return S_OK;
}

ResourceRecord &D3d11Device::record(D3D10DDI_HRESOURCE handle) const
{
    AV_SCOPE("D3d11Device::record");
    return *static_cast<ResourceRecord *>(handle.pDrvPrivate);
}

ResourceRecord &D3d11Device::record(DXGI_DDI_HRESOURCE handle) const
{
    AV_SCOPE("D3d11Device::record");
    return *reinterpret_cast<ResourceRecord *>(handle);
}

uint32_t D3d11Device::id_of(const void *object_block) const
{
    AV_SCOPE("D3d11Device::id_of");
    if (object_block == nullptr) return 0u;
    /* Every record starts with a magic and the id: resource, object and query
     * alike. */
    return static_cast<const uint32_t *>(object_block)[1];
}

uint32_t D3d11Device::next_id()
{
    AV_SCOPE("D3d11Device::next_id");
    return session_.next_id();
}

void D3d11Device::set_error(HRESULT result)
{
    AV_SCOPE("D3d11Device::set_error");
    char line[96] = {};
    TextBuffer text(line, sizeof(line));
    text.append("rvkumd: device error 0x");
    text.append_hex(static_cast<uint32_t>(result), 8u);
    log_.write(text.text());
    if (core_ != nullptr) core_->pfnSetErrorCb(core_layer_, result);
}

bool D3d11Device::context_local(void *handle, const void *immediate, size_t bytes) const
{
    AV_SCOPE("D3d11Device::context_local");
    if (parent_ == nullptr) return false;
    if (handle != nullptr && immediate != nullptr) __builtin_memcpy(handle, immediate, bytes);
    return true;
}

void D3d11Device::still_drawing()
{
    AV_SCOPE("D3d11Device::still_drawing");
    if (core_ != nullptr) core_->pfnSetErrorCb(core_layer_, DXGI_DDI_ERR_WASSTILLDRAWING);
}

HRESULT D3d11Device::initialize_deferred(D3d11Device &parent, const D3D11DDIARG_CREATEDEFERREDCONTEXT &create)
{
    AV_SCOPE("D3d11Device::initialize_deferred");
    if (!parent.channel_.constructed()) return E_FAIL;
    parent_ = &parent;
    runtime_ = parent.runtime_;
    core_layer_ = create.hRTCoreLayer;
    core_ = create.p11UMCallbacks;
    kernel_ = parent.kernel_;
    feature_level_ = parent.feature_level_;
    recording_block_ = static_cast<uint8_t *>(memory_.allocate(recording_bytes, Pool::paged));
    if (recording_block_ == nullptr) return E_OUTOFMEMORY;
    const uint32_t device = parent.channel_.object().device();
    static_cast<void>(recording_.construct(session_.transport(), recording_block_, recording_bytes));
    recording_.object().record();
    /* Recorded for the parent's device, which execution selects first. */
    recording_.object().selected(device);
    static_cast<void>(channel_.construct(recording_.object(), recording_lock_, device));
    return S_OK;
}

void D3d11Device::lock_calls()
{
    AV_SCOPE("D3d11Device::lock_calls");
    const uint32_t self = GetCurrentThreadId();
    if (__atomic_load_n(&calls_owner_, __ATOMIC_ACQUIRE) == self) {
        calls_depth_++;
        return;
    }
    calls_.acquire();
    __atomic_store_n(&calls_owner_, self, __ATOMIC_RELEASE);
    calls_depth_ = 1u;
}

void D3d11Device::unlock_calls()
{
    AV_SCOPE("D3d11Device::unlock_calls");
    if (--calls_depth_ != 0u) return;
    __atomic_store_n(&calls_owner_, 0u, __ATOMIC_RELEASE);
    calls_.release();
}

void D3d11Device::retire()
{
    AV_SCOPE("D3d11Device::retire");
    if (parent_ == nullptr) return;
    icd::ThreadLock &lock = session_.lock();
    lock.acquire();
    static_cast<void>(session_.contexts().destroy(this));
    lock.release();
}

bool D3d11Device::removed_on_host()
{
    AV_SCOPE("D3d11Device::removed_on_host");
    uint32_t reason = 0u;
    return !d3dproto::rvk_d3d_Finish11(wire(), &reason) || reason != 0u;
}

d3d::D3dDeviceChannel &D3d11Device::wire()
{
    AV_SCOPE("D3d11Device::wire");
    return channel_.object();
}

void D3d11Device::extent(const ResourceRecord &resource, uint32_t subresource, uint32_t &width, uint32_t &height,
                         uint32_t &depth) const
{
    AV_SCOPE("D3d11Device::extent");
    const uint32_t mip = resource.mips == 0u ? 0u : subresource % resource.mips;
    width = resource.width >> mip == 0u ? 1u : resource.width >> mip;
    height = resource.height >> mip == 0u ? 1u : resource.height >> mip;
    depth = resource.depth >> mip == 0u ? 1u : resource.depth >> mip;
    if (resource.dimension == D3D10DDIRESOURCE_BUFFER) height = depth = 1u;
}

uint32_t D3d11Device::row_bytes(const ResourceRecord &resource, uint32_t width) const
{
    AV_SCOPE("D3d11Device::row_bytes");
    if (resource.dimension == D3D10DDIRESOURCE_BUFFER) return width;
    const DxgiFormatLayout layout(resource.format);
    return layout.element_bytes() == 0u ? width : layout.row_bytes(width);
}

uint32_t D3d11Device::rows(const ResourceRecord &resource, uint32_t height) const
{
    AV_SCOPE("D3d11Device::rows");
    if (resource.dimension == D3D10DDIRESOURCE_BUFFER) return 1u;
    const DxgiFormatLayout layout(resource.format);
    return layout.element_bytes() == 0u ? height : layout.rows(height);
}

size_t D3d11Device::subresource_bytes(const ResourceRecord &resource, uint32_t subresource) const
{
    AV_SCOPE("D3d11Device::subresource_bytes");
    uint32_t width = 0u, height = 0u, depth = 0u;
    extent(resource, subresource, width, height, depth);
    return static_cast<size_t>(row_bytes(resource, width)) * rows(resource, height) * depth;
}

uint32_t D3d11Device::subresource_count(const ResourceRecord &resource) const
{
    AV_SCOPE("D3d11Device::subresource_count");
    return resource.dimension == D3D10DDIRESOURCE_BUFFER ? 1u : resource.mips * resource.layers;
}

size_t D3d11Device::subresource_offset(const ResourceRecord &resource, uint32_t subresource) const
{
    AV_SCOPE("D3d11Device::subresource_offset");
    size_t offset = 0u;
    for (uint32_t earlier = 0u; earlier < subresource; earlier++) offset += subresource_bytes(resource, earlier);
    return offset;
}

size_t D3d11Device::upload_bytes(const ResourceRecord &resource, uint32_t subresource, const D3D10_DDI_BOX *box,
                                 uint32_t row_pitch, uint32_t depth_pitch) const
{
    AV_SCOPE("D3d11Device::upload_bytes");
    uint32_t width = 0u, height = 0u, depth = 0u;
    extent(resource, subresource, width, height, depth);
    if (box != nullptr) {
        width = static_cast<uint32_t>(box->right - box->left);
        height = static_cast<uint32_t>(box->bottom - box->top);
        depth = static_cast<uint32_t>(box->back - box->front);
    }
    if (width == 0u || height == 0u || depth == 0u) return 0u;
    const uint32_t line = row_bytes(resource, width);
    const uint32_t lines = rows(resource, height);
    return static_cast<size_t>(depth - 1u) * depth_pitch + static_cast<size_t>(lines - 1u) * row_pitch + line;
}

void D3d11Device::upload(const ResourceRecord &resource, uint32_t subresource, const D3D10_DDI_BOX *box,
                         const void *data, size_t bytes, uint32_t row_pitch, uint32_t depth_pitch, uint32_t copy_flags)
{
    AV_SCOPE("D3d11Device::upload");
    if (data == nullptr || bytes == 0u) return;
    if (bytes <= upload_piece) {
        upload_piece_of(resource.id, subresource, box, data, bytes, row_pitch, depth_pitch, copy_flags);
        return;
    }
    uint32_t width = 0u, height = 0u, depth = 0u;
    extent(resource, subresource, width, height, depth);
    const D3D10_DDI_BOX whole = {0, 0, 0, static_cast<LONG>(width), static_cast<LONG>(height), static_cast<LONG>(depth)};
    const D3D10_DDI_BOX region = box != nullptr ? *box : whole;
    if (resource.dimension == D3D10DDIRESOURCE_BUFFER) {
        upload_buffer_pieces(resource.id, region, static_cast<const uint8_t *>(data), copy_flags);
        return;
    }
    upload_row_pieces(resource, subresource, region, static_cast<const uint8_t *>(data), row_pitch, depth_pitch,
                      copy_flags);
}

/* A buffer's box is in bytes, so a piece is a run of them. */
void D3d11Device::upload_buffer_pieces(uint32_t resource, const D3D10_DDI_BOX &region, const uint8_t *data,
                                       uint32_t copy_flags)
{
    AV_SCOPE("D3d11Device::upload_buffer_pieces");
    const uint32_t length = static_cast<uint32_t>(region.right - region.left);
    for (uint32_t done = 0u; done < length; done += static_cast<uint32_t>(upload_piece)) {
        const uint32_t piece = length - done < upload_piece ? length - done : static_cast<uint32_t>(upload_piece);
        D3D10_DDI_BOX part = region;
        part.left = region.left + static_cast<LONG>(done);
        part.right = part.left + static_cast<LONG>(piece);
        upload_piece_of(resource, 0u, &part, data + done, piece, 0u, 0u, done == 0u ? copy_flags : copy_flags & ~copy_discard);
    }
}

/* A texture goes a depth slice at a time and, within one, as many rows (block
 * rows, for a compressed format) as fit a piece. */
void D3d11Device::upload_row_pieces(const ResourceRecord &resource, uint32_t subresource, const D3D10_DDI_BOX &region,
                                    const uint8_t *data, uint32_t row_pitch, uint32_t depth_pitch, uint32_t copy_flags)
{
    AV_SCOPE("D3d11Device::upload_row_pieces");
    const uint32_t height = static_cast<uint32_t>(region.bottom - region.top);
    const uint32_t lines = rows(resource, height);
    const uint32_t line = row_bytes(resource, static_cast<uint32_t>(region.right - region.left));
    if (lines == 0u || row_pitch == 0u) return;
    /* Texel rows per stored row: 4 for a block-compressed format, 1 otherwise. */
    const uint32_t texels_per_line = (height + lines - 1u) / lines;
    const uint32_t lines_per_piece = row_pitch >= upload_piece ? 1u : static_cast<uint32_t>(upload_piece / row_pitch);
    bool first = true;
    for (LONG slice = region.front; slice < region.back; slice++) {
        const uint8_t *const plane = data + static_cast<size_t>(slice - region.front) * depth_pitch;
        for (uint32_t done = 0u; done < lines; done += lines_per_piece) {
            const uint32_t count = lines - done < lines_per_piece ? lines - done : lines_per_piece;
            D3D10_DDI_BOX part = region;
            part.front = slice;
            part.back = slice + 1;
            part.top = region.top + static_cast<LONG>(done * texels_per_line);
            const LONG bottom = region.top + static_cast<LONG>((done + count) * texels_per_line);
            part.bottom = bottom < region.bottom ? bottom : region.bottom;
            const size_t bytes = static_cast<size_t>(count - 1u) * row_pitch + line;
            upload_piece_of(resource.id, subresource, &part, plane + static_cast<size_t>(done) * row_pitch, bytes,
                            row_pitch, depth_pitch, first ? copy_flags : copy_flags & ~copy_discard);
            first = false;
        }
    }
}

void D3d11Device::upload_piece_of(uint32_t resource, uint32_t subresource, const D3D10_DDI_BOX *box, const void *data,
                                  size_t bytes, uint32_t row_pitch, uint32_t depth_pitch, uint32_t copy_flags)
{
    AV_SCOPE("D3d11Device::upload_piece_of");
    /* Large data leases the arena; an arena short of room is emptied first,
     * which is one round trip rather than a refused upload. */
    if (bytes > inline_limit && !wire().make_room_for(bytes)) log_.write("rvkumd: an upload larger than the arena");
    const d3dproto::Box region =
        box == nullptr ? d3dproto::Box{} : d3dproto::Box{static_cast<uint32_t>(box->left), static_cast<uint32_t>(box->top), static_cast<uint32_t>(box->front),
                                     static_cast<uint32_t>(box->right), static_cast<uint32_t>(box->bottom),
                                     static_cast<uint32_t>(box->back)};
    if (!d3dproto::rvk_d3d_UpdateSubresource(wire(), resource, subresource, box != nullptr ? 1u : 0u, region, row_pitch,
                                             depth_pitch, copy_flags, data, bytes))
        set_error(E_OUTOFMEMORY);
}

bool D3d11Device::read_back(const ResourceRecord &resource, uint32_t subresource, uint8_t *out, size_t capacity)
{
    AV_SCOPE("D3d11Device::read_back");
    if (capacity > inline_limit && !wire().make_room_for(capacity)) return false;
    uint32_t hresult = 1u, row_pitch = 0u, depth_pitch = 0u;
    uint64_t read = 0u;
    if (!d3dproto::rvk_d3d_ReadSubresource(wire(), resource.id, subresource, &hresult, &row_pitch, &depth_pitch, out,
                                           capacity, &read))
        return false;
    return hresult == 0u && read <= capacity;
}

void D3d11Device::destroy_object(uint32_t id)
{
    AV_SCOPE("D3d11Device::destroy_object");
    if (id != 0u) static_cast<void>(d3dproto::rvk_d3d_DestroyObject(wire(), id));
}

/* ---------------------------------------------------------------- device-wide slots */

BOOL D3d11Device::flush(UINT, UINT)
{
    AV_SCOPE("D3d11Device::flush");
    if (wire().application_flush() != Status::ok) {
        set_error(D3DDDIERR_DEVICEREMOVED);
        return TRUE;
    }
    /* The runtime signals a fence it was asked to after flushing, on the
     * kernel context: the signal must wait for what the flush sent. */
    if (FAILED(follow_host())) set_error(D3DDDIERR_DEVICEREMOVED);
    return TRUE;
}

VOID D3d11Device::relocate_device_funcs(struct D3DWDDM2_6DDI_DEVICEFUNCS *)
{
    AV_SCOPE("D3d11Device::relocate_device_funcs");
}

VOID D3d11Device::check_format_support(DXGI_FORMAT format, UINT *support)
{
    AV_SCOPE("D3d11Device::check_format_support");
    if (support != nullptr) *support = caps_.format_support(format);
}

VOID D3d11Device::check_multisample_quality_levels(DXGI_FORMAT format, UINT samples, UINT, UINT *levels)
{
    AV_SCOPE("D3d11Device::check_multisample_quality_levels");
    if (levels != nullptr) *levels = caps_.quality_levels(format, samples);
}

VOID D3d11Device::check_counter_info(D3D10DDI_COUNTER_INFO *info)
{
    AV_SCOPE("D3d11Device::check_counter_info");
    if (info == nullptr) return;
    info->LastDeviceDependentCounter = static_cast<D3D10DDI_QUERY>(0);
    info->NumSimultaneousCounters = 0u;
    info->NumDetectableParallelUnits = 0u;
}

VOID D3d11Device::check_counter(D3D10DDI_QUERY, D3D10DDI_COUNTER_TYPE *, UINT *, LPSTR, UINT *, LPSTR, UINT *, LPSTR,
                                UINT *)
{
    AV_SCOPE("D3d11Device::check_counter");
    set_error(E_INVALIDARG);
}

VOID D3d11Device::set_text_filter_size(UINT, UINT)
{
    AV_SCOPE("D3d11Device::set_text_filter_size");
}

VOID D3d11Device::check_direct_flip_support(D3D10DDI_HRESOURCE, D3D10DDI_HRESOURCE, UINT, BOOL *supported)
{
    AV_SCOPE("D3d11Device::check_direct_flip_support");
    if (supported != nullptr) *supported = FALSE;
}

VOID D3d11Device::set_marker()
{
    AV_SCOPE("D3d11Device::set_marker");
}

VOID D3d11Device::set_marker_mode(D3DWDDM1_3DDI_MARKER_TYPE, UINT)
{
    AV_SCOPE("D3d11Device::set_marker_mode");
}

VOID D3d11Device::set_hardware_protection(D3D10DDI_HRESOURCE, BOOL)
{
    AV_SCOPE("D3d11Device::set_hardware_protection");
}

VOID D3d11Device::set_hardware_protection_state(BOOL)
{
    AV_SCOPE("D3d11Device::set_hardware_protection_state");
}

VOID D3d11Device::acquire_resource(D3D10DDI_HRESOURCE, HANDLE)
{
    AV_SCOPE("D3d11Device::acquire_resource");
}

VOID D3d11Device::release_resource(D3D10DDI_HRESOURCE, HANDLE)
{
    AV_SCOPE("D3d11Device::release_resource");
}

HRESULT D3d11Device::retrieve_shader_comment(D3D10DDI_HSHADER, WCHAR *, SIZE_T *)
{
    AV_SCOPE("D3d11Device::retrieve_shader_comment");
    return E_NOTIMPL;
}

VOID D3d11Device::assign_debug_binary(D3D10DDI_HSHADER, UINT, const void *)
{
    AV_SCOPE("D3d11Device::assign_debug_binary");
}

SIZE_T D3d11Device::calc_private_shader_cache_session_size()
{
    AV_SCOPE("D3d11Device::calc_private_shader_cache_session_size");
    return sizeof(uint32_t);
}

VOID D3d11Device::create_shader_cache_session(D3DWDDM2_2DDI_HCACHESESSION, D3DWDDM2_2DDI_HRTCACHESESSION)
{
    AV_SCOPE("D3d11Device::create_shader_cache_session");
}

VOID D3d11Device::destroy_shader_cache_session(D3DWDDM2_2DDI_HCACHESESSION)
{
    AV_SCOPE("D3d11Device::destroy_shader_cache_session");
}

VOID D3d11Device::set_shader_cache_session(D3DWDDM2_2DDI_HCACHESESSION)
{
    AV_SCOPE("D3d11Device::set_shader_cache_session");
}

VOID D3d11Device::query_scanout_caps(D3D10DDI_HRESOURCE, UINT, D3DDDI_VIDEO_PRESENT_SOURCE_ID, UINT,
                                     D3DWDDM2_6DDI_SCANOUT_FLAGS *flags)
{
    AV_SCOPE("D3d11Device::query_scanout_caps");
    if (flags != nullptr) *flags = static_cast<D3DWDDM2_6DDI_SCANOUT_FLAGS>(0);
}

VOID D3d11Device::prepare_scanout_transformation(D3D10DDI_HRESOURCE, UINT, D3DDDI_VIDEO_PRESENT_SOURCE_ID, UINT,
                                                 D3D10_DDI_RECT *)
{
    AV_SCOPE("D3d11Device::prepare_scanout_transformation");
}

VOID D3d11Device::command_list_execute(D3D11DDI_HCOMMANDLIST handle)
{
    AV_SCOPE("D3d11Device::command_list_execute");
    const auto &list = *static_cast<const CommandListRecord *>(handle.pDrvPrivate);
    if (list.magic != CommandListRecord::tag) {
        set_error(E_INVALIDARG);
        return;
    }
    if (wire().execute(list.commands, list.length) != Status::ok) set_error(D3DDDIERR_DEVICEREMOVED);
}

/* A context-local handle is a copy of the immediate context's block
 * (context_local), so each type needs what its block does. */
SIZE_T D3d11Device::context_handle_bytes(uint32_t type) const
{
    AV_SCOPE("D3d11Device::context_handle_bytes");
    switch (type) {
    case D3D10DDI_HT_RESOURCE: return sizeof(ResourceRecord);
    case D3D10DDI_HT_QUERY: return sizeof(QueryRecord);
    case D3D11DDI_HT_COMMANDLIST: return sizeof(CommandListRecord);
    default: return sizeof(ObjectRecord);
    }
}

VOID D3d11Device::check_deferred_context_handle_sizes(UINT *count, D3D11DDI_HANDLESIZE *sizes)
{
    AV_SCOPE("D3d11Device::check_deferred_context_handle_sizes");
    constexpr uint32_t types = static_cast<uint32_t>(D3DWDDM2_2DDI_HT_CACHESESSION) + 1u;
    if (count == nullptr) return;
    if (sizes != nullptr) {
        const uint32_t filled = *count < types ? *count : types;
        for (uint32_t type = 0u; type < filled; type++)
            sizes[type] = D3D11DDI_HANDLESIZE{static_cast<D3D11DDI_HANDLETYPE>(type), context_handle_bytes(type)};
    }
    *count = types;
}

SIZE_T D3d11Device::calc_deferred_context_handle_size(D3D11DDI_HANDLETYPE type, void *)
{
    AV_SCOPE("D3d11Device::calc_deferred_context_handle_size");
    return context_handle_bytes(static_cast<uint32_t>(type));
}

SIZE_T D3d11Device::calc_private_deferred_context_size(const D3D11DDIARG_CALCPRIVATEDEFERREDCONTEXTSIZE *)
{
    AV_SCOPE("D3d11Device::calc_private_deferred_context_size");
    return d3d::DeviceTable::private_bytes;
}

VOID D3d11Device::create_deferred_context(const D3D11DDIARG_CREATEDEFERREDCONTEXT *args)
{
    AV_SCOPE("D3d11Device::create_deferred_context");
    if (args == nullptr || args->hDrvContext.pDrvPrivate == nullptr || args->pWDDM2_6ContextFuncs == nullptr) {
        set_error(E_INVALIDARG);
        return;
    }
    D3d11Device *context = nullptr;
    session_.lock().acquire();
    const Status pooled = session_.contexts().create(context, session_, memory_, log_, caps_);
    session_.lock().release();
    if (pooled != Status::ok) {
        set_error(E_OUTOFMEMORY);
        return;
    }
    const HRESULT made = context->initialize_deferred(*this, *args);
    if (FAILED(made)) {
        session_.lock().acquire();
        static_cast<void>(session_.contexts().destroy(context));
        session_.lock().release();
        set_error(made);
        return;
    }
    DeviceHandle(args->hDrvContext.pDrvPrivate).bind(*context);
    fill_device_funcs(*args->pWDDM2_6ContextFuncs);
    args->pWDDM2_6ContextFuncs->pfnDestroyDevice = &rvkumd_d3d11_DestroyDeferredContext;
}

VOID D3d11Device::abandon_command_list()
{
    AV_SCOPE("D3d11Device::abandon_command_list");
    size_t dropped = 0u;
    static_cast<void>(wire().take_recorded(nullptr, 0u, dropped));
}

SIZE_T D3d11Device::calc_private_command_list_size(const D3D11DDIARG_CREATECOMMANDLIST *)
{
    AV_SCOPE("D3d11Device::calc_private_command_list_size");
    return sizeof(CommandListRecord);
}

/* The deferred context's recording becomes the list's own copy, and the
 * context starts recording afresh. */
VOID D3d11Device::create_command_list(const D3D11DDIARG_CREATECOMMANDLIST *args, D3D11DDI_HCOMMANDLIST handle,
                                      D3D11DDI_HRTCOMMANDLIST)
{
    AV_SCOPE("D3d11Device::create_command_list");
    auto &list = *static_cast<CommandListRecord *>(handle.pDrvPrivate);
    list = CommandListRecord{0u, 0u, nullptr, 0u};
    if (args == nullptr) {
        set_error(E_INVALIDARG);
        return;
    }
    D3d11Device   &context = DeviceHandle(args->hDeferredContext.pDrvPrivate).device();
    const size_t   bytes = context.wire().recorded_bytes();
    uint8_t *const commands = bytes == 0u ? nullptr : static_cast<uint8_t *>(memory_.allocate(bytes, Pool::paged));
    if (bytes != 0u && commands == nullptr) {
        set_error(E_OUTOFMEMORY);
        return;
    }
    size_t taken = 0u;
    if (context.wire().take_recorded(commands, bytes, taken) != Status::ok) {
        if (commands != nullptr) memory_.release(commands);
        set_error(D3DDDIERR_DEVICEREMOVED);
        return;
    }
    list = CommandListRecord{CommandListRecord::tag, 0u, commands, taken};
}

VOID D3d11Device::destroy_command_list(D3D11DDI_HCOMMANDLIST handle)
{
    AV_SCOPE("D3d11Device::destroy_command_list");
    auto &list = *static_cast<CommandListRecord *>(handle.pDrvPrivate);
    if (list.magic != CommandListRecord::tag) return;
    if (list.commands != nullptr) memory_.release(list.commands);
    list = CommandListRecord{0u, 0u, nullptr, 0u};
}

VOID D3d11Device::recycle_command_list(D3D11DDI_HCOMMANDLIST)
{
    AV_SCOPE("D3d11Device::recycle_command_list");
}

HRESULT D3d11Device::recycle_create_command_list(const D3D11DDIARG_CREATECOMMANDLIST *args, D3D11DDI_HCOMMANDLIST handle,
                                                 D3D11DDI_HRTCOMMANDLIST runtime)
{
    AV_SCOPE("D3d11Device::recycle_create_command_list");
    create_command_list(args, handle, runtime);
    return static_cast<const CommandListRecord *>(handle.pDrvPrivate)->magic == CommandListRecord::tag ? S_OK
                                                                                                         : E_OUTOFMEMORY;
}

/* The runtime reuses a deferred context's block for a fresh recording: the
 * context already in it starts over, and a block with none gets one. */
HRESULT D3d11Device::recycle_create_deferred_context(const D3D11DDIARG_CREATEDEFERREDCONTEXT *args)
{
    AV_SCOPE("D3d11Device::recycle_create_deferred_context");
    if (args == nullptr || args->hDrvContext.pDrvPrivate == nullptr || args->pWDDM2_6ContextFuncs == nullptr)
        return E_INVALIDARG;
    D3d11Device *const held = &DeviceHandle(args->hDrvContext.pDrvPrivate).device();
    session_.lock().acquire();
    D3d11Device *const live = session_.contexts().find(held);
    session_.lock().release();
    if (live != nullptr && live->parent_ == this) {
        size_t dropped = 0u;
        static_cast<void>(live->wire().take_recorded(nullptr, 0u, dropped));
        live->core_layer_ = args->hRTCoreLayer;
        live->core_ = args->p11UMCallbacks;
        fill_device_funcs(*args->pWDDM2_6ContextFuncs);
        args->pWDDM2_6ContextFuncs->pfnDestroyDevice = &rvkumd_d3d11_DestroyDeferredContext;
        return S_OK;
    }
    create_deferred_context(args);
    return DeviceHandle(args->hDrvContext.pDrvPrivate).device().parent_ == this ? S_OK : E_OUTOFMEMORY;
}

VOID D3d11Device::recycle_destroy_command_list(D3D11DDI_HCOMMANDLIST handle)
{
    AV_SCOPE("D3d11Device::recycle_destroy_command_list");
    destroy_command_list(handle);
}

VOID D3d11Device::update_tile_mappings(D3D10DDI_HRESOURCE, UINT, const D3DWDDM1_3DDI_TILED_RESOURCE_COORDINATE *,
                                       const D3DWDDM1_3DDI_TILE_REGION_SIZE *, D3D10DDI_HRESOURCE, UINT, const UINT *,
                                       const UINT *, const UINT *, UINT)
{
    AV_SCOPE("D3d11Device::update_tile_mappings");
    set_error(E_NOTIMPL);
}

VOID D3d11Device::copy_tile_mappings(D3D10DDI_HRESOURCE, const D3DWDDM1_3DDI_TILED_RESOURCE_COORDINATE *,
                                     D3D10DDI_HRESOURCE, const D3DWDDM1_3DDI_TILED_RESOURCE_COORDINATE *,
                                     const D3DWDDM1_3DDI_TILE_REGION_SIZE *, UINT)
{
    AV_SCOPE("D3d11Device::copy_tile_mappings");
    set_error(E_NOTIMPL);
}

VOID D3d11Device::copy_tiles(D3D10DDI_HRESOURCE, const D3DWDDM1_3DDI_TILED_RESOURCE_COORDINATE *,
                             const D3DWDDM1_3DDI_TILE_REGION_SIZE *, D3D10DDI_HRESOURCE, UINT64, UINT)
{
    AV_SCOPE("D3d11Device::copy_tiles");
    set_error(E_NOTIMPL);
}

VOID D3d11Device::update_tiles(D3D10DDI_HRESOURCE, const D3DWDDM1_3DDI_TILED_RESOURCE_COORDINATE *,
                               const D3DWDDM1_3DDI_TILE_REGION_SIZE *, const void *, UINT)
{
    AV_SCOPE("D3d11Device::update_tiles");
    set_error(E_NOTIMPL);
}

VOID D3d11Device::tiled_resource_barrier(D3D11DDI_HANDLETYPE, void *, D3D11DDI_HANDLETYPE, void *)
{
    AV_SCOPE("D3d11Device::tiled_resource_barrier");
}

VOID D3d11Device::get_mip_packing(D3D10DDI_HRESOURCE, UINT *packed, UINT *tiles)
{
    AV_SCOPE("D3d11Device::get_mip_packing");
    if (packed != nullptr) *packed = 0u;
    if (tiles != nullptr) *tiles = 0u;
}

VOID D3d11Device::resize_tile_pool(D3D10DDI_HRESOURCE, UINT64)
{
    AV_SCOPE("D3d11Device::resize_tile_pool");
    set_error(E_NOTIMPL);
}

VOID D3d11Device::get_resource_layout(D3D10DDI_HRESOURCE, UINT, D3DKMT_HANDLE *, D3DWDDM2_0DDI_TEXTURE_LAYOUT *,
                                      UINT *, D3DWDDM2_0DDI_SUBRESOURCE_LAYOUT *)
{
    AV_SCOPE("D3d11Device::get_resource_layout");
    set_error(E_NOTIMPL);
}

}  // namespace av::d3d11
