#include "um/d3d/d3d11_device.hpp"
#include "proto/resources/mip_extent.hpp"
#include "proto/resources/subresource_layout.hpp"

#include "core/platform/block_source.hpp"
#include "core/log/log_line.hpp"
#include "core/platform/thread_lock.hpp"
#include "core/trace/function_scope.hpp"
#include "proto/client/transfer_room.hpp"
#include "core/log/keyed_log.hpp"
#include "icd/d3d/device_table.hpp"
#include "icd/d3d/texture_upload.hpp"
#include "proto/client/client_blob_arena.hpp"
#include "proto/context/sub_api.hpp"
#include "proto/d3d/dxgi_format_layout.hpp"
#include "proto/generated/d3d/d3d_dispatch_client.h"
#include "um/d3d/d3d11_dxgi_thunks.hpp"
#include "um/d3d/d3d11_host_caps.hpp"
#include "um/d3d/d3d_session.hpp"
#include "icd/d3d/device_block.hpp"

extern "C" VOID APIENTRY avumd_d3d11_DestroyDeferredContext(D3D10DDI_HDEVICE device);

namespace av::d3d11 {

namespace {
constexpr uint32_t feature_levels[] = {0xa000u, 0xa100u, 0xb000u, 0xb100u, 0x9100u,
                                       0x9200u, 0x9300u, 0xc000u, 0xc100u};

constexpr uint32_t copy_discard = 0x2u;

}

D3d11Device::D3d11Device(d3d::D3dSession &session, BlockSource &memory, KeyedLog &log, const D3d11HostCaps &caps,
                         d3d::D3dCallTrace &trace, const d3d::SharedRowsRequest &rows)
    : session_(session), memory_(memory), shadows_(memory), maps_(memory), lines_(log), refused_(lines_), loss_(lines_), caps_(caps), runtime_{}, core_layer_{}, kernel_(), lender_(session), clock_(), allocations_(kernel_, lines_, clock_),
      core_(nullptr), dxgi_(nullptr), channel_(), slices_(), parent_(nullptr), recording_lock_(), caller_(), call_mutex_(), calls_(call_mutex_, caller_), recording_(),
      recorded_(), context_(nullptr), present_order_(), waits_(), feature_level_(0u),
      guest_surfaces_(memory, Pool::paged), gdi_watched_(memory, Pool::paged), taking_gdi_writes_(false), resolved_(nullptr), resolved_returned_ns_(0u), resolved_thread_(0u), flush_candidate_(nullptr), flush_gap_ns_(0u), guest_lock_refused_(), redirection_unlent_(), pages_read_refused_(), pages_read_late_(), present_refused_(), present_flags_(~0u), follow_refused_(), token_refused_(), frame_(1u),
      trace_(trace), rows_(rows), context_number_(trace.number_context())
{
    AV_SCOPE("D3d11Device::D3d11Device");
}

D3d11Device::~D3d11Device()
{
    AV_SCOPE("D3d11Device::~D3d11Device");
    if (parent_ != nullptr) {
        if (channel_.constructed()) channel_.destroy();
        if (recording_.constructed()) recording_.destroy();
        if (recorded_.constructed()) recorded_.destroy();
        return;
    }
    const bool joined = channel_.constructed();
    if (joined) {
        static_cast<void>(channel_.object().close());
        static_cast<void>(channel_.object().flush());
        channel_.destroy();
    }
    if (waits_.constructed()) waits_.destroy();
    if (present_order_.constructed()) present_order_.destroy();
    if (joined) session_.reclaim_retired();
    if (joined) session_.leave();
    kernel_.destroy_context(context_);
}

HRESULT D3d11Device::initialize(uint32_t adapter, const D3D10DDIARG_CREATEDEVICE &create)
{
    AV_SCOPE("D3d11Device::initialize");
    runtime_ = create.hRTDevice;
    core_layer_ = create.hRTCoreLayer;
    kernel_.bind(create.pKTCallbacks, runtime_.handle);
    lender_.bind(create.pKTCallbacks, runtime_.handle);
    core_ = create.p11UMCallbacks;
    dxgi_ = create.DXGIBaseDDI.pDXGIBaseCallbacks;
    const HRESULT context = allocations_.make_context(context_);
    if (FAILED(context)) return context;
    const uint32_t level = D3D11DDI_EXTRACT_3DPIPELINELEVEL_FROM_FLAGS(create.Flags);
    feature_level_ = level < sizeof(feature_levels) / sizeof(feature_levels[0]) ? feature_levels[level] : 0u;
    const HRESULT joined = session_.join(adapter);
    if (FAILED(joined)) return joined;
    static_cast<void>(present_order_.construct(kernel_.callbacks(), kernel_.runtime(), context_, session_, lines_));
    static_cast<void>(waits_.construct(kernel_.callbacks(), kernel_.runtime(), session_.transport(), session_.lock(), lines_, context_));
    static_cast<void>(channel_.construct(session_.stream(), session_.lock(), session_.next_id()));
    uint32_t hresult = 1u, made = 0u;
    if (!channel_.object().open(feature_level_, hresult, made) || hresult != 0u) {
        lines_.write("the host made no Direct3D 11 device");
        channel_.destroy();
        waits_.destroy();
        present_order_.destroy();
        session_.leave();
        return E_FAIL;
    }
    session_.query_results().bind(channel_.object());
    slices_.own(session_.slices());
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
    return static_cast<const d3d::RecordStamp *>(object_block)->id();
}

uint32_t D3d11Device::next_id()
{
    AV_SCOPE("D3d11Device::next_id");
    return session_.next_id();
}

void D3d11Device::set_error(HRESULT result)
{
    AV_SCOPE("D3d11Device::set_error");
    LogLine(lines_)
        .text("device error ")
        .hex(static_cast<uint32_t>(result), 8u)
        .write();
    if (core_ != nullptr) core_->pfnSetErrorCb(core_layer_, result);
}

HRESULT D3d11Device::lost(const char *entry)
{
    AV_SCOPE("D3d11Device::lost");
    loss_.lose(session_.fault_code());
    static_cast<void>(loss_.refuses(entry));
    if (core_ != nullptr) core_->pfnSetErrorCb(core_layer_, D3DDDIERR_DEVICEREMOVED);
    return loss_.guilty() ? DXGI_ERROR_DEVICE_RESET : DXGI_ERROR_DEVICE_REMOVED;
}

void D3d11Device::not_implemented(const char *call)
{
    AV_SCOPE("D3d11Device::not_implemented");
    refused_.unserved(call);
    set_error(E_NOTIMPL);
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
    kernel_.bind(&parent.kernel_.callbacks(), runtime_.handle);
    lender_.bind(&parent.kernel_.callbacks(), runtime_.handle);
    feature_level_ = parent.feature_level_;
    static_cast<void>(recorded_.construct(memory_, recording_bytes, most_recording_bytes));
    if (recorded_.object().initialize() != Status::ok) return E_OUTOFMEMORY;
    const uint32_t device = parent.channel_.object().device();
    d3d::CommandRecording &recorded = recorded_.object();
    static_cast<void>(recording_.construct(session_.transport()));
    recording_.object().record(recorded);
    recording_.object().selected(device);
    static_cast<void>(channel_.construct(recording_.object(), recording_lock_, device));
    return S_OK;
}

void D3d11Device::lock_calls()
{
    AV_SCOPE("D3d11Device::lock_calls");
    calls_.acquire();
    note_call_after_resolve();
    if (gdi_watched_.size() != 0u && !taking_gdi_writes_) take_gdi_writes();
}

void D3d11Device::unlock_calls()
{
    AV_SCOPE("D3d11Device::unlock_calls");
    flush_candidate_ = nullptr;
    calls_.release();
}

void D3d11Device::retire()
{
    AV_SCOPE("D3d11Device::retire");
    if (parent_ == nullptr) return;
    ThreadLock &lock = session_.lock();
    lock.acquire();
    static_cast<void>(session_.contexts().destroy(this));
    lock.release();
}

bool D3d11Device::removed_on_host()
{
    AV_SCOPE("D3d11Device::removed_on_host");
    uint32_t reason = 0u;
    return !d3dproto::av_d3d_Finish11(wire(), &reason) || reason != 0u;
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
    const SubresourceLayout subresources(resource.mips, resource.layers, 1u);
    const MipExtent extent(resource.width, resource.height, resource.depth, subresources.mip_of(subresource));
    width = static_cast<uint32_t>(extent.width());
    height = extent.height();
    depth = extent.depth();
    if (resource.dimension == D3D10DDIRESOURCE_BUFFER) height = depth = 1u;
}

uint32_t D3d11Device::row_bytes(const ResourceRecord &resource, uint32_t width) const
{
    AV_SCOPE("D3d11Device::row_bytes");
    if (resource.dimension == D3D10DDIRESOURCE_BUFFER) return width;
    return DxgiFormatLayout(resource.format).copied_row_bytes(width);
}

uint32_t D3d11Device::rows(const ResourceRecord &resource, uint32_t height) const
{
    AV_SCOPE("D3d11Device::rows");
    if (resource.dimension == D3D10DDIRESOURCE_BUFFER) return 1u;
    return DxgiFormatLayout(resource.format).rows(height);
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
    return resource.dimension == D3D10DDIRESOURCE_BUFFER ? 1u
        : SubresourceLayout(resource.mips, resource.layers, 1u).count();
}

size_t D3d11Device::subresource_offset(const ResourceRecord &resource, uint32_t subresource) const
{
    AV_SCOPE("D3d11Device::subresource_offset");
    size_t offset = 0u;
    for (uint32_t earlier = 0u; earlier < subresource; earlier++) offset += subresource_bytes(resource, earlier);
    return offset;
}

D3d11Device::UpdateWriter::UpdateWriter(D3d11Device &device, uint32_t resource, uint32_t subresource,
                                        uint32_t flags)
    : device_(device), resource_(resource), subresource_(subresource), flags_(flags)
{
    AV_SCOPE("D3d11Device::UpdateWriter::UpdateWriter");
}

bool D3d11Device::UpdateWriter::piece(const d3dproto::Box &box, const uint8_t *data, uint64_t bytes,
                                      uint32_t row_pitch, uint32_t slice_pitch, bool first)
{
    AV_SCOPE("D3d11Device::UpdateWriter::piece");
    return device_.upload_piece_of(resource_, subresource_, box, data, static_cast<size_t>(bytes), row_pitch,
                                   slice_pitch, first ? flags_ : flags_ & ~copy_discard);
}

void D3d11Device::upload(const ResourceRecord &resource, uint32_t subresource, const D3D10_DDI_BOX *box,
                         const void *data, uint32_t row_pitch, uint32_t depth_pitch, uint32_t copy_flags)
{
    AV_SCOPE("D3d11Device::upload");
    if (data == nullptr) return;
    uint32_t width = 0u, height = 0u, depth = 0u;
    extent(resource, subresource, width, height, depth);
    const d3dproto::Box region =
        box == nullptr ? d3dproto::Box{0u, 0u, 0u, width, height, depth}
                       : d3dproto::Box{static_cast<uint32_t>(box->left),  static_cast<uint32_t>(box->top),
                                       static_cast<uint32_t>(box->front), static_cast<uint32_t>(box->right),
                                       static_cast<uint32_t>(box->bottom), static_cast<uint32_t>(box->back)};
    const DxgiFormatLayout format(resource.format);
    const bool             bytes = resource.dimension == D3D10DDIRESOURCE_BUFFER || format.element_bytes() == 0u;
    const TexelLayout      layout = bytes ? TexelLayout(1u, 1u, 1u) : TexelLayout(format);
    const d3d::TextureUpload upload(layout, region, row_pitch, depth_pitch, piece_bytes());
    if (upload.bytes() == 0u) return;
    UpdateWriter writer(*this, resource.stamp.id(), subresource, copy_flags);
    if (!upload.send(writer, static_cast<const uint8_t *>(data)))
        lines_.once("an upload whose rows are closer together than a row of them");
}

size_t D3d11Device::piece_bytes() const
{
    AV_SCOPE("D3d11Device::piece_bytes");
    return parent_ != nullptr ? d3d::D3dCommandStream::recorded_data : TransferRoom::lent_bytes;
}

bool D3d11Device::upload_piece_of(uint32_t resource, uint32_t subresource, const d3dproto::Box &box,
                                  const void *data, size_t bytes, uint32_t row_pitch, uint32_t depth_pitch,
                                  uint32_t copy_flags)
{
    AV_SCOPE("D3d11Device::upload_piece_of");
    if (!wire().make_room_for_blobs(bytes)) lines_.once("an upload larger than the arena");
    if (d3dproto::av_d3d_UpdateSubresource(wire(), resource, subresource, 1u, box, row_pitch, depth_pitch, copy_flags,
                                            data, bytes))
        return true;
    set_error(E_OUTOFMEMORY);
    return false;
}

bool D3d11Device::read_back(const ResourceRecord &resource, uint32_t subresource, uint8_t *out, size_t capacity)
{
    AV_SCOPE("D3d11Device::read_back");
    if (!wire().make_room_for_blobs(capacity)) return false;
    uint32_t hresult = 1u, row_pitch = 0u, depth_pitch = 0u;
    uint64_t read = 0u;
    if (!d3dproto::av_d3d_ReadSubresource(wire(), resource.stamp.id(), subresource, &hresult, &row_pitch, &depth_pitch, out,
                                           capacity, &read))
        return false;
    return hresult == 0u && read <= capacity;
}

void D3d11Device::destroy_object(uint32_t id)
{
    AV_SCOPE("D3d11Device::destroy_object");
    if (id != 0u) static_cast<void>(d3dproto::av_d3d_DestroyObject(wire(), id));
}

BOOL D3d11Device::flush(UINT, UINT)
{
    AV_SCOPE("D3d11Device::flush");
    learn_from_flush();
    if (wire().application_flush() != Status::ok) {
        lost(__func__);
        return TRUE;
    }
    if (FAILED(follow_host())) lost(__func__);
    session_.reclaim_retired();
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

VOID D3d11Device::check_direct_flip_support(D3D10DDI_HRESOURCE primary, D3D10DDI_HRESOURCE candidate, UINT,
                                            BOOL *supported)
{
    AV_SCOPE("D3d11Device::check_direct_flip_support");
    if (supported == nullptr) return;
    *supported = FALSE;
    if (primary.pDrvPrivate == nullptr || candidate.pDrvPrivate == nullptr) return;
    const ResourceRecord &shown = record(primary);
    const ResourceRecord &flipped = record(candidate);
    const bool held = shown.allocation != 0u && flipped.allocation != 0u && shown.shared_key != 0u &&
                      flipped.shared_key != 0u;
    *supported = held && shown.width == flipped.width && shown.height == flipped.height && shown.format == flipped.format
                     ? TRUE
                     : FALSE;
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

VOID D3d11Device::acquire_resource(D3D10DDI_HRESOURCE, HANDLE token)
{
    AV_SCOPE("D3d11Device::acquire_resource");
    if (token == nullptr || context_ == nullptr) return;
    const HRESULT queued = kernel_.acquire(token, context_);
    if (SUCCEEDED(queued) || !token_refused_.first()) return;
    LogLine(lines_).text("the kernel refused a buffer's sync token ").hex(static_cast<uint32_t>(queued), 8u).write();
}

VOID D3d11Device::release_resource(D3D10DDI_HRESOURCE, HANDLE token)
{
    AV_SCOPE("D3d11Device::release_resource");
    if (token == nullptr || context_ == nullptr) return;
    if (wire().application_flush() != Status::ok || FAILED(follow_host())) {
        lost(__func__);
        return;
    }
    const HRESULT queued = kernel_.release(token, context_);
    if (SUCCEEDED(queued) || !token_refused_.first()) return;
    LogLine(lines_).text("the kernel refused a buffer's sync token ").hex(static_cast<uint32_t>(queued), 8u).write();
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
    if (!list.stamp.holds(CommandListRecord::tag)) {
        set_error(E_INVALIDARG);
        return;
    }
    const Status ran = wire().execute(list.commands, list.length);
    if (ran == Status::ok) return;
    LogLine(lines_)
        .text("a command list of ")
        .number(list.length)
        .text(" bytes could not be run, status ")
        .number(static_cast<uint32_t>(ran))
        .write();
    lost(__func__);
}

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
    const Status pooled = session_.contexts().create(context, session_, memory_, lines_, caps_, trace_, rows_);
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
    d3d::DeviceBlock<D3d11Device>(args->hDrvContext.pDrvPrivate).bind(*context);
    fill_device_funcs(*args->pWDDM2_6ContextFuncs);
    args->pWDDM2_6ContextFuncs->pfnDestroyDevice = &avumd_d3d11_DestroyDeferredContext;
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

VOID D3d11Device::create_command_list(const D3D11DDIARG_CREATECOMMANDLIST *args, D3D11DDI_HCOMMANDLIST handle,
                                      D3D11DDI_HRTCOMMANDLIST)
{
    AV_SCOPE("D3d11Device::create_command_list");
    auto &list = *static_cast<CommandListRecord *>(handle.pDrvPrivate);
    list = CommandListRecord{d3d::RecordStamp(), nullptr, 0u};
    if (args == nullptr) {
        set_error(E_INVALIDARG);
        return;
    }
    D3d11Device   &context = d3d::DeviceBlock<D3d11Device>(args->hDeferredContext.pDrvPrivate).device();
    const size_t   bytes = context.wire().recorded_bytes();
    uint8_t *const commands = bytes == 0u ? nullptr : static_cast<uint8_t *>(memory_.allocate(bytes, Pool::paged));
    if (bytes != 0u && commands == nullptr) {
        set_error(E_OUTOFMEMORY);
        return;
    }
    size_t       taken = 0u;
    const Status took = context.wire().take_recorded(commands, bytes, taken);
    if (took != Status::ok) {
        if (commands != nullptr) memory_.release(commands);
        if (took != Status::no_space) {
            lost(__func__);
            return;
        }
        lines_.write("a command list outgrew its deferred context's recording block");
        set_error(E_OUTOFMEMORY);
        return;
    }
    list = CommandListRecord{d3d::RecordStamp(CommandListRecord::tag, 0u), commands, taken};
}

VOID D3d11Device::destroy_command_list(D3D11DDI_HCOMMANDLIST handle)
{
    AV_SCOPE("D3d11Device::destroy_command_list");
    auto &list = *static_cast<CommandListRecord *>(handle.pDrvPrivate);
    if (!list.stamp.holds(CommandListRecord::tag)) return;
    if (list.commands != nullptr) memory_.release(list.commands);
    list = CommandListRecord{d3d::RecordStamp(), nullptr, 0u};
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
    return static_cast<const CommandListRecord *>(handle.pDrvPrivate)->stamp.holds(CommandListRecord::tag) ? S_OK
                                                                                                         : E_OUTOFMEMORY;
}

HRESULT D3d11Device::recycle_create_deferred_context(const D3D11DDIARG_CREATEDEFERREDCONTEXT *args)
{
    AV_SCOPE("D3d11Device::recycle_create_deferred_context");
    if (args == nullptr || args->hDrvContext.pDrvPrivate == nullptr || args->pWDDM2_6ContextFuncs == nullptr)
        return E_INVALIDARG;
    D3d11Device *const held = &d3d::DeviceBlock<D3d11Device>(args->hDrvContext.pDrvPrivate).device();
    session_.lock().acquire();
    D3d11Device *const live = session_.contexts().find(held);
    session_.lock().release();
    if (live != nullptr && live->parent_ == this) {
        size_t dropped = 0u;
        static_cast<void>(live->wire().take_recorded(nullptr, 0u, dropped));
        live->core_layer_ = args->hRTCoreLayer;
        live->core_ = args->p11UMCallbacks;
        fill_device_funcs(*args->pWDDM2_6ContextFuncs);
        args->pWDDM2_6ContextFuncs->pfnDestroyDevice = &avumd_d3d11_DestroyDeferredContext;
        return S_OK;
    }
    create_deferred_context(args);
    return d3d::DeviceBlock<D3d11Device>(args->hDrvContext.pDrvPrivate).device().parent_ == this ? S_OK : E_OUTOFMEMORY;
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
    not_implemented("pfnUpdateTileMappings");
}

VOID D3d11Device::copy_tile_mappings(D3D10DDI_HRESOURCE, const D3DWDDM1_3DDI_TILED_RESOURCE_COORDINATE *,
                                     D3D10DDI_HRESOURCE, const D3DWDDM1_3DDI_TILED_RESOURCE_COORDINATE *,
                                     const D3DWDDM1_3DDI_TILE_REGION_SIZE *, UINT)
{
    AV_SCOPE("D3d11Device::copy_tile_mappings");
    not_implemented("pfnCopyTileMappings");
}

VOID D3d11Device::copy_tiles(D3D10DDI_HRESOURCE, const D3DWDDM1_3DDI_TILED_RESOURCE_COORDINATE *,
                             const D3DWDDM1_3DDI_TILE_REGION_SIZE *, D3D10DDI_HRESOURCE, UINT64, UINT)
{
    AV_SCOPE("D3d11Device::copy_tiles");
    not_implemented("pfnCopyTiles");
}

VOID D3d11Device::update_tiles(D3D10DDI_HRESOURCE, const D3DWDDM1_3DDI_TILED_RESOURCE_COORDINATE *,
                               const D3DWDDM1_3DDI_TILE_REGION_SIZE *, const void *, UINT)
{
    AV_SCOPE("D3d11Device::update_tiles");
    not_implemented("pfnUpdateTiles");
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
    not_implemented("pfnResizeTilePool");
}

VOID D3d11Device::get_resource_layout(D3D10DDI_HRESOURCE handle, UINT count, D3DKMT_HANDLE *allocation,
                                      D3DWDDM2_0DDI_TEXTURE_LAYOUT *layout, UINT *swizzle_transition,
                                      D3DWDDM2_0DDI_SUBRESOURCE_LAYOUT *subresources)
{
    AV_SCOPE("D3d11Device::get_resource_layout");
    const ResourceRecord &resource = record(handle);
    if (allocation != nullptr) *allocation = resource.allocation;
    if (layout != nullptr) *layout = D3DWDDM2_0DDI_TL_ROW_MAJOR;
    const bool lent = resource.lent != nullptr;
    if (swizzle_transition != nullptr) *swizzle_transition = no_swizzle_transition;
    if (subresources == nullptr) return;
    const uint32_t known = subresource_count(resource);
    for (UINT index = 0u; index < count; index++) {
        D3DWDDM2_0DDI_SUBRESOURCE_LAYOUT &out = subresources[index];
        out = {};
        if (index >= known) continue;
        uint32_t width = 0u;
        uint32_t height = 0u;
        uint32_t depth = 0u;
        extent(resource, index, width, height, depth);
        out.BaseOffset = subresource_offset(resource, index);
        out.RowPitch = lent ? resource.allocation_pitch : row_bytes(resource, width);
        out.DepthPitch = out.RowPitch * rows(resource, height);
    }
}

}
