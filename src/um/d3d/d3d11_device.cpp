#include "um/d3d/d3d11_device.hpp"

#include "core/platform/block_source.hpp"
#include "core/support/text_buffer.hpp"
#include "core/trace/function_scope.hpp"
#include "icd/common/log_sink.hpp"
#include "proto/client_blob_arena.hpp"
#include "proto/context/sub_api.hpp"
#include "proto/d3d/dxgi_format_layout.hpp"
#include "proto/generated/d3d/d3d_dispatch_client.h"
#include "um/d3d/d3d11_host_caps.hpp"
#include "um/d3d/d3d11_session.hpp"

namespace av::d3d11 {

namespace {

/* D3D_FEATURE_LEVEL by the DDI's pipeline level, which CreateDevice's flags
 * carry. */
constexpr uint32_t feature_levels[] = {0xa000u, 0xa100u, 0xb000u, 0xb100u, 0x9100u,
                                       0x9200u, 0x9300u, 0xc000u, 0xc100u};

/* Past this the host's answer crosses through the blob arena rather than the
 * reply slot. */
constexpr size_t inline_limit = 32u * 1024u;

}  // namespace

D3d11Device::D3d11Device(D3d11Session &session, BlockSource &memory, icd::LogSink &log, const D3d11HostCaps &caps)
    : session_(session), memory_(memory), log_(log), caps_(caps), runtime_{}, core_layer_{}, kernel_(nullptr),
      core_(nullptr), dxgi_(nullptr), channel_(), present_context_(nullptr), feature_level_(0u), shared_keys_(0u)
{
    AV_SCOPE("D3d11Device::D3d11Device");
}

D3d11Device::~D3d11Device()
{
    AV_SCOPE("D3d11Device::~D3d11Device");
    if (channel_.constructed()) {
        static_cast<void>(channel_.object().close());
        static_cast<void>(channel_.object().flush());
        channel_.destroy();
        session_.leave();
    }
    if (present_context_ != nullptr && kernel_ != nullptr) {
        D3DDDICB_DESTROYCONTEXT destroy = {};
        destroy.hContext = present_context_;
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

void D3d11Device::upload(uint32_t resource, uint32_t subresource, const D3D10_DDI_BOX *box, const void *data,
                         size_t bytes, uint32_t row_pitch, uint32_t depth_pitch, uint32_t copy_flags)
{
    AV_SCOPE("D3d11Device::upload");
    if (data == nullptr || bytes == 0u) return;
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
    static_cast<void>(d3dproto::rvk_d3d_Flush11(wire()));
    if (wire().flush() != Status::ok) set_error(D3DDDIERR_DEVICEREMOVED);
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

/* ---------------------------------------------------------------- what the caps refuse */

VOID D3d11Device::command_list_execute(D3D11DDI_HCOMMANDLIST)
{
    AV_SCOPE("D3d11Device::command_list_execute");
    set_error(E_NOTIMPL);
}

VOID D3d11Device::check_deferred_context_handle_sizes(UINT *count, D3D11DDI_HANDLESIZE *)
{
    AV_SCOPE("D3d11Device::check_deferred_context_handle_sizes");
    if (count != nullptr) *count = 0u;
}

SIZE_T D3d11Device::calc_deferred_context_handle_size(D3D11DDI_HANDLETYPE, void *)
{
    AV_SCOPE("D3d11Device::calc_deferred_context_handle_size");
    return 0u;
}

SIZE_T D3d11Device::calc_private_deferred_context_size(const D3D11DDIARG_CALCPRIVATEDEFERREDCONTEXTSIZE *)
{
    AV_SCOPE("D3d11Device::calc_private_deferred_context_size");
    return 0u;
}

VOID D3d11Device::create_deferred_context(const D3D11DDIARG_CREATEDEFERREDCONTEXT *)
{
    AV_SCOPE("D3d11Device::create_deferred_context");
    set_error(E_NOTIMPL);
}

VOID D3d11Device::abandon_command_list()
{
    AV_SCOPE("D3d11Device::abandon_command_list");
}

SIZE_T D3d11Device::calc_private_command_list_size(const D3D11DDIARG_CREATECOMMANDLIST *)
{
    AV_SCOPE("D3d11Device::calc_private_command_list_size");
    return 0u;
}

VOID D3d11Device::create_command_list(const D3D11DDIARG_CREATECOMMANDLIST *, D3D11DDI_HCOMMANDLIST,
                                      D3D11DDI_HRTCOMMANDLIST)
{
    AV_SCOPE("D3d11Device::create_command_list");
    set_error(E_NOTIMPL);
}

VOID D3d11Device::destroy_command_list(D3D11DDI_HCOMMANDLIST)
{
    AV_SCOPE("D3d11Device::destroy_command_list");
}

VOID D3d11Device::recycle_command_list(D3D11DDI_HCOMMANDLIST)
{
    AV_SCOPE("D3d11Device::recycle_command_list");
}

HRESULT D3d11Device::recycle_create_command_list(const D3D11DDIARG_CREATECOMMANDLIST *, D3D11DDI_HCOMMANDLIST,
                                                 D3D11DDI_HRTCOMMANDLIST)
{
    AV_SCOPE("D3d11Device::recycle_create_command_list");
    return E_NOTIMPL;
}

HRESULT D3d11Device::recycle_create_deferred_context(const D3D11DDIARG_CREATEDEFERREDCONTEXT *)
{
    AV_SCOPE("D3d11Device::recycle_create_deferred_context");
    return E_NOTIMPL;
}

VOID D3d11Device::recycle_destroy_command_list(D3D11DDI_HCOMMANDLIST)
{
    AV_SCOPE("D3d11Device::recycle_destroy_command_list");
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
