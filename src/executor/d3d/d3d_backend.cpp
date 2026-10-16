#include "executor/d3d/d3d_backend.hpp"
#include "executor/sharing/shared_image_shape.hpp"

#include "executor/sharing/host_shares.hpp"
#include "executor/sharing/session_shares.hpp"

#include <stdio.h>

#include "core/log/log_line.hpp"
#include "core/trace/function_scope.hpp"
#include "executor/owned_fd.hpp"
#include "executor/source_memory.hpp"
#include "device/abi/av_executor.h"
#include "executor/d3d/dxbc_container.hpp"
#include "proto/d3d/dxgi_format_layout.hpp"
#include "proto/d3d/resource_misc.hpp"
#include "proto/resources/pitched_extent.hpp"
#include "executor/import_table.hpp"
#include "executor/poll_budget.hpp"
#include "executor/d3d/query_result_publisher.hpp"
#include "executor/shim/d3d11_host_device.hpp"
#include "executor/shim/d3d_headers.hpp"
#include "executor/shim/surface_identity.hpp"
#include "executor/d3d/shared_surfaces.hpp"
#include "executor/d3d/frame_stager.hpp"
#include "executor/d3d/staged_frame.hpp"
#include "proto/resources/surface_point.hpp"
#include "proto/generated/d3d/d3d_command_names.h"
#include "proto/generated/d3d/d3d_protocol_ids.h"
#include "proto/wire/wire_arena.hpp"

namespace av {

namespace {

constexpr uint32_t stage_vertex = 0u;
constexpr uint32_t stage_hull = 1u;
constexpr uint32_t stage_domain = 2u;
constexpr uint32_t stage_geometry = 3u;
constexpr uint32_t stage_pixel = 4u;
constexpr uint32_t stage_compute = 5u;

constexpr uint32_t dimension_buffer = 1u;
constexpr uint32_t dimension_texture1d = 2u;
constexpr uint32_t dimension_texture2d = 3u;
constexpr uint32_t dimension_texture3d = 4u;

/* The misc flags the host's resources may carry: sharing is the executor's
 * own table, and a keyed mutex or GDI surface has no meaning on a device no
 * other process opens. */
constexpr uint32_t kept_misc_flags = D3D11_RESOURCE_MISC_GENERATE_MIPS | D3D11_RESOURCE_MISC_TEXTURECUBE |
                                     D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS |
                                     D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS |
                                     D3D11_RESOURCE_MISC_BUFFER_STRUCTURED | D3D11_RESOURCE_MISC_RESOURCE_CLAMP;
constexpr uint32_t known_bind_flags = D3D11_BIND_VERTEX_BUFFER | D3D11_BIND_INDEX_BUFFER |
                                      D3D11_BIND_CONSTANT_BUFFER | D3D11_BIND_SHADER_RESOURCE |
                                      D3D11_BIND_STREAM_OUTPUT | D3D11_BIND_RENDER_TARGET |
                                      D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_UNORDERED_ACCESS;
constexpr uint32_t max_dimension = 16384u;
constexpr uint32_t max_buffer_bytes = 1u << 30u;
constexpr uint32_t no_stream = 0xffffffffu;
constexpr size_t   container_slack = 64u * 1024u;

/* A Direct3D 11 present's frame, staged through its GPU's device: a 2D
 * texture of four-byte texels, or nothing. */
class PresentedTexture final : public FrameStager {
public:
    /* `texture` null for a resource that is no 2D texture. */
    PresentedTexture(D3d11HostDevice &device, ID3D11Texture2D *texture, uint32_t subresource, uint32_t width,
                     uint32_t height, uint32_t format);

    StagedFrame *stage() override;

private:
    D3d11HostDevice &device_;
    ID3D11Texture2D *texture_;
    uint32_t         subresource_;
    uint32_t         width_;
    uint32_t         height_;
    uint32_t         format_;
};

PresentedTexture::PresentedTexture(D3d11HostDevice &device, ID3D11Texture2D *texture, uint32_t subresource,
                                   uint32_t width, uint32_t height, uint32_t format)
    : device_(device), texture_(texture), subresource_(subresource), width_(width), height_(height), format_(format)
{
    AV_SCOPE("PresentedTexture::PresentedTexture");
}

StagedFrame *PresentedTexture::stage()
{
    AV_SCOPE("PresentedTexture::stage");
    if (texture_ == nullptr || !DxgiFormatLayout(format_).word_texels()) return nullptr;
    return device_.stage_frame(texture_, subresource_, width_, height_, format_);
}

}  // namespace

D3dObjectKind D3dBackend::shader_kind(uint32_t stage) const
{
    AV_SCOPE("D3dBackend::shader_kind");
    switch (stage) {
    case stage_vertex: return D3dObjectKind::vertex_shader;
    case stage_hull: return D3dObjectKind::hull_shader;
    case stage_domain: return D3dObjectKind::domain_shader;
    case stage_geometry: return D3dObjectKind::geometry_shader;
    case stage_pixel: return D3dObjectKind::pixel_shader;
    case stage_compute: return D3dObjectKind::compute_shader;
    default: return D3dObjectKind::none;
    }
}

D3dBackend::D3dBackend()
    : d3dproto::ServerDispatch(guest_), device11_(nullptr), surfaces_(nullptr), shares_(nullptr), wake_(), fence_wake_(), clock_(), admitted_(false), held_(false), heap_(),
      blocks_(heap_), remade_(blocks_, Pool::paged), states_(blocks_), sink_(stderr),
      lines_(sink_, "virtio-gpu-av: d3d11: "), refusal_(lines_), arena_(nullptr), objects_(blocks_), guest_(),
      region_(guest_), imports_(nullptr), d3d12_(guest_, objects_, wake_, fence_wake_, blocks_),
      d3d9_(guest_, objects_, blocks_),
      published_(blocks_, guest_), progress11_(fence_wake_), constants_(blocks_), slice_storage_(), slice_memory_(nullptr),
      slice_storage_refused_(false),
      missed_key_(0u), mark_refused_(false), show_refused_(false), lost_(false)
{
    AV_SCOPE("D3dBackend::D3dBackend");
}

bool D3dBackend::initialize(SessionShares &shares, ImportTable &imports, D3d11HostDevice *device11,
                            D3d12HostDevice *device12, SharedSurfaces *surfaces, D3d9HostDevice *device9)
{
    AV_SCOPE("D3dBackend::initialize");
    if (device11 == nullptr || !device11->admit(wake_)) return false;
    admitted_ = true;
    device11_ = device11;
    surfaces_ = surfaces;
    shares_ = &shares;
    imports_ = &imports;
    d3d12_.initialize(imports, device12, device11, surfaces, &shares);
    d3d9_.initialize(device9, surfaces, device11, &shares);
    lost_ = false;
    return true;
}

void D3dBackend::shut_down()
{
    AV_SCOPE("D3dBackend::shut_down");
    /* Direct3D 12's and 9's first: their objects are in the session's table
     * too, and 12 drains its queues through its own before letting them go. */
    d3d12_.shut_down();
    d3d9_.shut_down();
    if (device11_ != nullptr) {
        /* forget_state takes the device's lock itself. */
        for (ID3DDeviceContextState *state = states_.remove_any(); state != nullptr; state = states_.remove_any()) {
            device11_->forget_state(state);
            device11_->lock(nullptr);
            state->Release();
            device11_->unlock();
        }
        device11_->lock(nullptr);
        published_.unbind();
        constants_.close();
        /* While the table still names the dynamic buffers: each is discarded
         * off the store before the store goes, then the table lets them go. */
        release_slice_storage();
        objects_.release_all();
        progress11_.release();
        device11_->unlock();
    }
    static_cast<void>(states_.select(0u));
    if (admitted_) device11_->dismiss(wake_);
    admitted_ = false;
    wake_.clear();
    fence_wake_.unbind();
    device11_ = nullptr;
    shares_ = nullptr;
    slice_storage_refused_ = false;
    imports_ = nullptr;
}

bool D3dBackend::knows(uint32_t command_id) const
{
    AV_SCOPE("D3dBackend::knows");
    return d3dproto::knows_command(command_id) || d3d12_.knows(command_id) || d3d9_.knows(command_id);
}

bool D3dBackend::run(uint32_t command_id, WireReader &payload, WireWriter &reply, WireArena &arena)
{
    AV_SCOPE("D3dBackend::run");
    if (device11_ == nullptr) return false;
    blocks_.forget();
    /* Direct3D 12 has no immediate context to share: its commands take no
     * Direct3D 11 lock, and a batch mixing both holds it only across the 11
     * ones - a 12 command can take it itself (sharing a surface does), and
     * the lock is not recursive. */
    if (d3d12_.knows(command_id)) {
        end_batch();
        return d3d12_.run(command_id, payload, reply, arena);
    }
    /* Direct3D 9's devices are their own, as 12's queues are. */
    if (d3d9_.knows(command_id)) {
        end_batch();
        return d3d9_.run(command_id, payload, reply, arena);
    }
    if (!held_) device11_->lock(states_.selected());
    else device11_->select(states_.selected());
    held_ = true;
    arena_ = &arena;
    const bool ran = d3dproto::dispatch_command(*this, command_id, payload, reply, arena);
    arena_ = nullptr;
    if (ran) return true;
    return states_.refuse(refusal_, d3dproto::command_name(command_id),
                          blocks_.refusal(static_cast<uint32_t>(DXGI_ERROR_DRIVER_INTERNAL_ERROR)));
}

/* A batch that ended queries is flushed, so DXVK starts the work their
 * results wait on rather than when the guest next asks. */
/* A batch is marked and flushed, so the ring completes it only once the GPU
 * has run it (D3d11BatchProgress); a batch that ended queries is flushed even
 * when it cannot be marked, so DXVK starts the work their results wait on
 * rather than when the guest next asks. */
void D3dBackend::end_batch()
{
    AV_SCOPE("D3dBackend::end_batch");
    if (!held_) return;
    if (progress11_.fence() == nullptr) progress11_.adopt(device11_->make_fence(progress11_.marked()));
    if (device11_->signal(progress11_.fence(), progress11_.next())) {
        progress11_.signalled();
        published_.flushed();
    } else {
        if (!mark_refused_)
            LogLine(lines_).text("no fence marks the session's batches: they complete as recorded").write();
        mark_refused_ = true;
        if (published_.needs_flush()) {
            device11_->context()->Flush();
            published_.flushed();
        }
    }
    static_cast<void>(published_.publish(device11_->context()));
    held_ = false;
    device11_->unlock();
}

bool D3dBackend::poll()
{
    AV_SCOPE("D3dBackend::poll");
    const bool unwatched = progress11_.polled();
    if (device11_ == nullptr || held_ || !published_.pending()) return unwatched;
    /* GetData reads no pipeline state, so no device's state is selected. */
    device11_->lock(nullptr);
    const bool pending = published_.publish(device11_->context());
    device11_->unlock();
    /* Either still waiting keeps the worker polling: queries all published
     * say nothing of a batch no wake watches. */
    return pending || unwatched;
}

void D3dBackend::bind_signal(void (*signal)(void *context), void *context)
{
    AV_SCOPE("D3dBackend::bind_signal");
    wake_.bind(signal, context);
    fence_wake_.bind(signal, context);
}

/* A session is one runtime's (avumd makes one per runtime a process uses), so
 * a session whose Direct3D 12 queues ever marked work answers with theirs, and
 * any other with its Direct3D 11 batches': the two count apart, and the ring
 * compares one answer with the next. */
void D3dBackend::gpu_progress(uint64_t &marked, uint64_t &reached) const
{
    AV_SCOPE("D3dBackend::gpu_progress");
    d3d12_.gpu_progress(marked, reached);
    if (marked == 0u) {
        marked = progress11_.marked();
        reached = progress11_.reached();
    }
    /* A lost device finishes nothing more: what it was given is not waited
     * for, or a ring holding its batches would never let them go. */
    if (lost()) reached = marked;
}

bool D3dBackend::lost() const
{
    AV_SCOPE("D3dBackend::lost");
    return lost_ || (device11_ != nullptr && device11_->lost()) || d3d12_.lost() || d3d9_.lost();
}

bool D3dBackend::lose_device()
{
    AV_SCOPE("D3dBackend::lose_device");
    return device11_ != nullptr && device11_->lose();
}

bool D3dBackend::adopt(const SourceMemory &memory, uint64_t &handle)
{
    AV_SCOPE("D3dBackend::adopt");
    if (!region_.held()) return region_.adopt(memory, handle);
    handle = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&memory));
    return true;
}

void D3dBackend::abandon(const SourceMemory &memory, uint64_t)
{
    AV_SCOPE("D3dBackend::abandon");
    if (&memory == slice_memory_ && device11_ != nullptr) {
        device11_->lock(nullptr);
        release_slice_storage();
        device11_->unlock();
    }
    if (!region_.is(memory)) return;
    published_.unbind();
    region_.abandon(memory);
}

/* A texture the guest shared is made over memory other APIs can open: as rows
 * when the guest asked, for an API that reads it so (feature 053) - a window
 * those APIs draw presented through it, an OpenCL image over it - and
 * otherwise in the GPU's own tiling, which Direct3D draws and samples at full
 * speed (ResourceMisc). Null for anything else, which DXVK makes as it makes
 * any texture. */
ID3D11Texture2D *D3dBackend::shareable(const D3D11_TEXTURE2D_DESC &texture, uint32_t guest_misc)
{
    AV_SCOPE("D3dBackend::shareable");
    const ResourceMisc misc(guest_misc);
    if (!misc.shared()) return nullptr;
    D3D11_TEXTURE2D_DESC1 described = {};
    described.Width = texture.Width;
    described.Height = texture.Height;
    described.MipLevels = texture.MipLevels;
    described.ArraySize = texture.ArraySize;
    described.Format = texture.Format;
    described.SampleDesc = texture.SampleDesc;
    described.Usage = texture.Usage;
    described.BindFlags = texture.BindFlags;
    described.MiscFlags = texture.MiscFlags;
    return misc.rows() ? device11_->make_rows(described) : device11_->make_tiled(described);
}

/* A shared texture's memory, as the dma-buf OpenGL and Vulkan import. The
 * Direct3D 11 lock a batch holds is held across this command too; one that
 * arrives on its own takes it. */
bool D3dBackend::export_object(uint32_t kind, uint64_t wire_handle, uint32_t &handle_kind, OwnedFd &descriptor)
{
    AV_SCOPE("D3dBackend::export_object");
    handle_kind = 0u;
    if (device11_ == nullptr || kind != AV_SHARED_MEMORY || wire_handle == 0u || wire_handle > UINT32_MAX)
        return false;
    ID3D11Resource *const resource = objects_.find_as<ID3D11Resource>(static_cast<uint32_t>(wire_handle), D3dObjectKind::resource);
    ID3D11Texture2D *texture = nullptr;
    if (resource == nullptr ||
        FAILED(resource->QueryInterface(__uuidof(ID3D11Texture2D), reinterpret_cast<void **>(&texture))))
        return false;
    const bool locked = !held_;
    if (locked) device11_->lock(nullptr);
    const bool exported = device11_->export_memory(texture, descriptor);
    if (locked) device11_->unlock();
    texture->Release();
    if (exported) handle_kind = AV_SHARED_HANDLE_DMA_BUF;
    return exported;
}

bool D3dBackend::import_object(uint32_t, uint32_t, int, uint64_t, const SharedImageShape &, uint64_t &wire_handle,
                               uint64_t &detail)
{
    AV_SCOPE("D3dBackend::import_object");
    wire_handle = 0u;
    detail = 0u;
    return false;
}

void D3dBackend::release_object(uint32_t, uint64_t, uint64_t)
{
    AV_SCOPE("D3dBackend::release_object");
}

bool D3dBackend::all_of(const uint32_t *ids, uint32_t count, D3dObjectKind kind) const
{
    AV_SCOPE("D3dBackend::all_of");
    for (uint32_t index = 0u; index < count; index++)
        if (ids[index] != 0u && objects_.find(ids[index], kind) == nullptr) return false;
    return true;
}

bool D3dBackend::ready() const
{
    AV_SCOPE("D3dBackend::ready");
    return states_.ready();
}

bool D3dBackend::has_device() const
{
    AV_SCOPE("D3dBackend::has_device");
    return states_.count() != 0u;
}

bool D3dBackend::extent_of(ID3D11Resource *resource, uint32_t subresource, uint32_t &width, uint32_t &height,
                           uint32_t &depth, uint32_t &format) const
{
    AV_SCOPE("D3dBackend::extent_of");
    D3D11_RESOURCE_DIMENSION dimension = D3D11_RESOURCE_DIMENSION_UNKNOWN;
    resource->GetType(&dimension);
    uint32_t mips = 1u, layers = 1u;
    width = height = depth = 1u;
    format = 0u;
    if (dimension == D3D11_RESOURCE_DIMENSION_BUFFER) {
        D3D11_BUFFER_DESC buffer = {};
        static_cast<ID3D11Buffer *>(resource)->GetDesc(&buffer);
        width = buffer.ByteWidth;
    } else if (dimension == D3D11_RESOURCE_DIMENSION_TEXTURE1D) {
        D3D11_TEXTURE1D_DESC texture = {};
        static_cast<ID3D11Texture1D *>(resource)->GetDesc(&texture);
        width = texture.Width;
        mips = texture.MipLevels;
        layers = texture.ArraySize;
        format = texture.Format;
    } else if (dimension == D3D11_RESOURCE_DIMENSION_TEXTURE2D) {
        D3D11_TEXTURE2D_DESC texture = {};
        static_cast<ID3D11Texture2D *>(resource)->GetDesc(&texture);
        width = texture.Width;
        height = texture.Height;
        mips = texture.MipLevels;
        layers = texture.ArraySize;
        format = texture.Format;
    } else if (dimension == D3D11_RESOURCE_DIMENSION_TEXTURE3D) {
        D3D11_TEXTURE3D_DESC texture = {};
        static_cast<ID3D11Texture3D *>(resource)->GetDesc(&texture);
        width = texture.Width;
        height = texture.Height;
        depth = texture.Depth;
        mips = texture.MipLevels;
        format = texture.Format;
    } else {
        return false;
    }
    if (subresource >= mips * layers) return false;
    const uint32_t mip = subresource % mips;
    width = width >> mip == 0u ? 1u : width >> mip;
    height = height >> mip == 0u ? 1u : height >> mip;
    depth = depth >> mip == 0u ? 1u : depth >> mip;
    return true;
}

ID3D11Resource *D3dBackend::staging_for(ID3D11Resource *resource, uint32_t, uint32_t width, uint32_t height,
                                        uint32_t depth, uint32_t format)
{
    AV_SCOPE("D3dBackend::staging_for");
    D3D11_RESOURCE_DIMENSION dimension = D3D11_RESOURCE_DIMENSION_UNKNOWN;
    resource->GetType(&dimension);
    ID3D11Device5 *const device = device11_->device();
    if (dimension == D3D11_RESOURCE_DIMENSION_BUFFER) {
        const D3D11_BUFFER_DESC buffer = {width, D3D11_USAGE_STAGING, 0u, D3D11_CPU_ACCESS_READ, 0u, 0u};
        ID3D11Buffer *made = nullptr;
        return SUCCEEDED(device->CreateBuffer(&buffer, nullptr, &made)) ? made : nullptr;
    }
    if (dimension == D3D11_RESOURCE_DIMENSION_TEXTURE1D) {
        const D3D11_TEXTURE1D_DESC texture = {width, 1u, 1u, static_cast<DXGI_FORMAT>(format), D3D11_USAGE_STAGING,
                                              0u,    D3D11_CPU_ACCESS_READ, 0u};
        ID3D11Texture1D *made = nullptr;
        return SUCCEEDED(device->CreateTexture1D(&texture, nullptr, &made)) ? made : nullptr;
    }
    if (dimension == D3D11_RESOURCE_DIMENSION_TEXTURE2D) {
        const D3D11_TEXTURE2D_DESC texture = {width, height, 1u, 1u, static_cast<DXGI_FORMAT>(format), {1u, 0u},
                                              D3D11_USAGE_STAGING, 0u, D3D11_CPU_ACCESS_READ, 0u};
        ID3D11Texture2D *made = nullptr;
        return SUCCEEDED(device->CreateTexture2D(&texture, nullptr, &made)) ? made : nullptr;
    }
    const D3D11_TEXTURE3D_DESC texture = {width, height, depth, 1u, static_cast<DXGI_FORMAT>(format),
                                          D3D11_USAGE_STAGING, 0u, D3D11_CPU_ACCESS_READ, 0u};
    ID3D11Texture3D *made = nullptr;
    return SUCCEEDED(device->CreateTexture3D(&texture, nullptr, &made)) ? made : nullptr;
}

/* ---------------------------------------------------------------- device */

bool D3dBackend::on_CreateDevice11(uint32_t device, uint32_t feature_level, uint32_t, uint32_t &hresult,
                                   uint32_t &level)
{
    AV_SCOPE("D3dBackend::on_CreateDevice11");
    hresult = static_cast<uint32_t>(E_FAIL);
    level = 0u;
    if (device == 0u || states_.find(device) != nullptr) return false;
    /* make_state takes the device's lock itself, and run holds it. */
    device11_->unlock();
    uint32_t made = 0u;
    ID3DDeviceContextState *const state = device11_->make_state(made);
    const Status kept = state == nullptr ? Status::invalid_state : states_.add(device, state);
    if (kept == Status::ok) static_cast<void>(states_.select(device));
    if (kept != Status::ok && state != nullptr) state->Release();
    device11_->lock(states_.selected());
    if (kept == Status::no_memory) hresult = static_cast<uint32_t>(E_OUTOFMEMORY);
    if (kept != Status::ok) return true;
    hresult = static_cast<uint32_t>(S_OK);
    level = feature_level != 0u && feature_level < made ? feature_level : made;
    return true;
}

/* run holds the device's lock with the state that was selected when the
 * command began; taking it again swaps the new one in. An id the host has no
 * device for selects nothing, and what follows until the next selection is
 * dropped: refusing the selection would remove whichever device spoke before
 * it, which did nothing wrong. */
bool D3dBackend::on_SelectDevice11(uint32_t device)
{
    AV_SCOPE("D3dBackend::on_SelectDevice11");
    ID3DDeviceContextState *const selected = states_.select(device);
    device11_->unlock();
    device11_->lock(selected);
    if (selected != nullptr) constants_.selected(device11_->context(), device);
    return true;
}

bool D3dBackend::on_DestroyDevice11(uint32_t device)
{
    AV_SCOPE("D3dBackend::on_DestroyDevice11");
    ID3DDeviceContextState *const state = states_.remove(device);
    if (state == nullptr) return false;
    constants_.forget_device(device);
    /* forget_state takes the device's lock itself, and run holds it. */
    device11_->unlock();
    device11_->forget_state(state);
    device11_->lock(states_.selected());
    state->Release();
    return true;
}

bool D3dBackend::on_RotateResources11(uint32_t count, const uint32_t *resources)
{
    AV_SCOPE("D3dBackend::on_RotateResources11");
    if (!has_device() || count > max_rotation) return false;
    if (count < 2u) return true;
    IUnknown *textures[max_rotation] = {};
    for (uint32_t index = 0u; index < count; index++) {
        textures[index] = objects_.find(resources[index], D3dObjectKind::resource);
        if (textures[index] == nullptr) return false;
        for (uint32_t other = 0u; other < index; other++)
            if (textures[other] == textures[index]) return false;
    }
    for (uint32_t index = 0u; index < count; index++)
        static_cast<void>(objects_.exchange(resources[index], D3dObjectKind::resource, textures[(index + 1u) % count]));
    return remake_views(textures, count) == Status::ok;
}

/* Every view of a rotated texture is made again, with its own description, on
 * the texture its resource's id now names: the next one in the rotation. A
 * remade view's place in remade_ is had before it replaces the old one, so
 * a view either is remade and rebound or stays as it was. */
Status D3dBackend::remake_views(IUnknown *const *textures, uint32_t count)
{
    AV_SCOPE("D3dBackend::remake_views");
    Status remade = Status::ok;
    for (uint32_t view = 0u; view < objects_.views() && remade == Status::ok; view++) {
        const uint32_t slot = objects_.view_slot(view);
        uint32_t      id = 0u;
        D3dObjectKind kind = D3dObjectKind::none;
        /* The table lists only slots holding a view, so `object` is one. */
        IUnknown *const object = objects_.at(slot, id, kind);
        IUnknown *viewed = objects_.viewed(slot);
        if (viewed == nullptr) {
            ID3D11Resource *resource = nullptr;
            static_cast<ID3D11View *>(object)->GetResource(&resource);
            viewed = resource;
            if (resource != nullptr) resource->Release();
            objects_.note_viewed(slot, viewed);
        }
        uint32_t found = count;
        for (uint32_t index = 0u; index < count; index++)
            if (viewed == textures[index]) found = index;
        if (found == count) continue;
        IUnknown *const made = remake_view(kind, object, static_cast<ID3D11Resource *>(textures[(found + 1u) % count]));
        if (made == nullptr) continue;
        remade = remade_.reserve(remade_.size() + 1u);
        if (remade != Status::ok) {
            made->Release();
            continue;
        }
        IUnknown *const old = objects_.exchange(id, kind, made);
        static_cast<void>(remade_.append(Remade{old, made}));
    }
    rebind_remade();
    return remade;
}

/* What was bound of a remade view is bound as its replacement, here on the
 * host, so the guest need not have the runtime re-send every slot of every
 * stage after each present. */
void D3dBackend::rebind_remade()
{
    AV_SCOPE("D3dBackend::rebind_remade");
    constexpr UINT srv_slots = D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT;
    constexpr UINT uav_slots = D3D11_1_UAV_SLOT_COUNT;
    ID3D11DeviceContext *const context = device11_->context();
    ID3D11ShaderResourceView *srvs[srv_slots] = {};
    using GetSrv = void (STDMETHODCALLTYPE ID3D11DeviceContext::*)(UINT, UINT, ID3D11ShaderResourceView **);
    using SetSrv = void (STDMETHODCALLTYPE ID3D11DeviceContext::*)(UINT, UINT, ID3D11ShaderResourceView *const *);
    const GetSrv gets[] = {&ID3D11DeviceContext::VSGetShaderResources, &ID3D11DeviceContext::HSGetShaderResources,
                           &ID3D11DeviceContext::DSGetShaderResources, &ID3D11DeviceContext::GSGetShaderResources,
                           &ID3D11DeviceContext::PSGetShaderResources, &ID3D11DeviceContext::CSGetShaderResources};
    const SetSrv sets[] = {&ID3D11DeviceContext::VSSetShaderResources, &ID3D11DeviceContext::HSSetShaderResources,
                           &ID3D11DeviceContext::DSSetShaderResources, &ID3D11DeviceContext::GSSetShaderResources,
                           &ID3D11DeviceContext::PSSetShaderResources, &ID3D11DeviceContext::CSSetShaderResources};
    for (uint32_t stage = 0u; stage < 6u; stage++) {
        (context->*gets[stage])(0u, srv_slots, srvs);
        if (swap_remade(reinterpret_cast<IUnknown **>(srvs), srv_slots)) (context->*sets[stage])(0u, srv_slots, srvs);
        release_all(reinterpret_cast<IUnknown **>(srvs), srv_slots);
    }
    ID3D11UnorderedAccessView *uavs[uav_slots] = {};
    context->CSGetUnorderedAccessViews(0u, uav_slots, uavs);
    if (swap_remade(reinterpret_cast<IUnknown **>(uavs), uav_slots)) context->CSSetUnorderedAccessViews(0u, uav_slots, uavs, nullptr);
    release_all(reinterpret_cast<IUnknown **>(uavs), uav_slots);
    ID3D11RenderTargetView *rtvs[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT] = {};
    ID3D11DepthStencilView *dsv = nullptr;
    context->OMGetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, rtvs, &dsv);
    const bool targets = swap_remade(reinterpret_cast<IUnknown **>(rtvs), D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT);
    const bool depth = swap_remade(reinterpret_cast<IUnknown **>(&dsv), 1u);
    if (targets || depth) context->OMSetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, rtvs, dsv);
    release_all(reinterpret_cast<IUnknown **>(rtvs), D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT);
    release_all(reinterpret_cast<IUnknown **>(&dsv), 1u);
    for (const Remade &one : remade_) one.old_->Release();
    remade_.truncate(0u);
}

/* Each bound remade view in `bound` becomes its replacement, the reference
 * the Get gave moving with it. */
bool D3dBackend::swap_remade(IUnknown **bound, uint32_t count) const
{
    AV_SCOPE("D3dBackend::swap_remade");
    bool swapped = false;
    for (uint32_t slot = 0u; slot < count; slot++) {
        if (bound[slot] == nullptr) continue;
        for (const Remade &one : remade_) {
            if (bound[slot] != one.old_) continue;
            bound[slot]->Release();
            bound[slot] = one.made_;
            bound[slot]->AddRef();
            swapped = true;
            break;
        }
    }
    return swapped;
}

void D3dBackend::release_all(IUnknown **held, uint32_t count) const
{
    AV_SCOPE("D3dBackend::release_all");
    for (uint32_t slot = 0u; slot < count; slot++)
        if (held[slot] != nullptr) held[slot]->Release();
}

IUnknown *D3dBackend::remake_view(D3dObjectKind kind, IUnknown *view, ID3D11Resource *texture)
{
    AV_SCOPE("D3dBackend::remake_view");
    ID3D11Device5 *const device = device11_->device();
    if (kind == D3dObjectKind::shader_resource_view) {
        D3D11_SHADER_RESOURCE_VIEW_DESC desc = {};
        static_cast<ID3D11ShaderResourceView *>(view)->GetDesc(&desc);
        ID3D11ShaderResourceView *made = nullptr;
        return SUCCEEDED(device->CreateShaderResourceView(texture, &desc, &made)) ? made : nullptr;
    }
    if (kind == D3dObjectKind::render_target_view) {
        D3D11_RENDER_TARGET_VIEW_DESC desc = {};
        static_cast<ID3D11RenderTargetView *>(view)->GetDesc(&desc);
        ID3D11RenderTargetView *made = nullptr;
        return SUCCEEDED(device->CreateRenderTargetView(texture, &desc, &made)) ? made : nullptr;
    }
    if (kind == D3dObjectKind::depth_stencil_view) {
        D3D11_DEPTH_STENCIL_VIEW_DESC desc = {};
        static_cast<ID3D11DepthStencilView *>(view)->GetDesc(&desc);
        ID3D11DepthStencilView *made = nullptr;
        return SUCCEEDED(device->CreateDepthStencilView(texture, &desc, &made)) ? made : nullptr;
    }
    D3D11_UNORDERED_ACCESS_VIEW_DESC desc = {};
    static_cast<ID3D11UnorderedAccessView *>(view)->GetDesc(&desc);
    ID3D11UnorderedAccessView *made = nullptr;
    return SUCCEEDED(device->CreateUnorderedAccessView(texture, &desc, &made)) ? made : nullptr;
}

bool D3dBackend::on_QueryCaps11(d3dproto::Caps11 &caps, uint8_t *formats, uint64_t formats_capacity,
                                uint64_t &formats_size)
{
    AV_SCOPE("D3dBackend::on_QueryCaps11");
    caps = device11_->capabilities().caps();
    formats_size = device11_->capabilities().formats().give(formats, formats_capacity);
    return true;
}

bool D3dBackend::on_ClearState11()
{
    AV_SCOPE("D3dBackend::on_ClearState11");
    if (!has_device()) return false;
    device11_->context()->ClearState();
    constants_.cleared(states_.selected_id());
    return true;
}

bool D3dBackend::on_Flush11()
{
    AV_SCOPE("D3dBackend::on_Flush11");
    if (!has_device()) return false;
    device11_->context()->Flush();
    published_.flushed();
    return true;
}

bool D3dBackend::on_Finish11(uint32_t &hresult)
{
    AV_SCOPE("D3dBackend::on_Finish11");
    if (!has_device()) return false;
    /* Every session records into the one immediate context, so what the guest
     * needs of a finish - its commands ahead of anything another session
     * sends next, and the arena read - holds once they have run here. Waiting
     * for the GPU as well, under the device lock, stalled every session and
     * the scanout behind it: 7 to 50 ms a present. */
    device11_->context()->Flush();
    const HRESULT removed = device11_->device()->GetDeviceRemovedReason();
    lost_ = FAILED(removed);
    const uint32_t own = states_.removed_reason(states_.selected_id());
    hresult = own != 0u ? own : static_cast<uint32_t>(removed);
    return true;
}

bool D3dBackend::on_DestroyObject(uint32_t id)
{
    AV_SCOPE("D3dBackend::on_DestroyObject");
    D3dObjectKind kind = D3dObjectKind::none;
    IUnknown *const found = objects_.find_any(id, kind);
    /* Direct3D 12's and 9's share the session's table; they are not
     * DestroyObject's. */
    if (found == nullptr || kind >= D3dObjectKind::heap12) return false;
    published_.forget(id);
    constants_.forget(id);
    objects_.remove(id);
    return true;
}

/* ---------------------------------------------------------------- creation */

bool D3dBackend::on_CreateResource(uint32_t id, const d3dproto::ResourceDesc &desc, uint64_t shared_key)
{
    AV_SCOPE("D3dBackend::on_CreateResource");
    if (!has_device() || !DxgiFormatLayout(desc.format).known() || (desc.bind_flags & ~known_bind_flags) != 0u) return false;
    if (desc.width == 0u || desc.mip_levels > 16u) return false;
    const uint32_t misc = desc.misc_flags & kept_misc_flags;
    ID3D11Device5 *const device = device11_->device();
    IUnknown *made = nullptr;
    HRESULT result = E_INVALIDARG;
    if (desc.dimension == dimension_buffer) {
        if (desc.width > max_buffer_bytes) return false;
        /* A buffer the guest rewrites with Map(DISCARD) stays one DXVK can
         * rename: as a default buffer every rewrite is a copy on the GPU's
         * timeline, with the barriers that go with it. */
        constexpr uint32_t ddi_usage_dynamic = 2u;
        constexpr uint32_t not_dynamic = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_STREAM_OUTPUT;
        const bool dynamic = desc.usage == ddi_usage_dynamic && (desc.bind_flags & not_dynamic) == 0u &&
                             (misc & D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS) == 0u;
        const D3D11_BUFFER_DESC buffer = {desc.width, dynamic ? D3D11_USAGE_DYNAMIC : D3D11_USAGE_DEFAULT,
                                          desc.bind_flags, dynamic ? static_cast<UINT>(D3D11_CPU_ACCESS_WRITE) : 0u,
                                          misc, desc.structure_stride};
        ID3D11Buffer *out = nullptr;
        result = device->CreateBuffer(&buffer, nullptr, &out);
        made = out;
    } else if (desc.dimension == dimension_texture1d) {
        if (desc.width > max_dimension || desc.depth_or_array > 2048u) return false;
        const D3D11_TEXTURE1D_DESC texture = {desc.width, desc.mip_levels, desc.depth_or_array,
                                              static_cast<DXGI_FORMAT>(desc.format), D3D11_USAGE_DEFAULT,
                                              desc.bind_flags, 0u, misc};
        ID3D11Texture1D *out = nullptr;
        result = device->CreateTexture1D(&texture, nullptr, &out);
        made = out;
    } else if (desc.dimension == dimension_texture2d) {
        if (desc.width > max_dimension || desc.height > max_dimension || desc.depth_or_array > 2048u) return false;
        const D3D11_TEXTURE2D_DESC texture = {desc.width,
                                              desc.height,
                                              desc.mip_levels,
                                              desc.depth_or_array,
                                              static_cast<DXGI_FORMAT>(desc.format),
                                              {desc.sample_count == 0u ? 1u : desc.sample_count, desc.sample_quality},
                                              D3D11_USAGE_DEFAULT,
                                              desc.bind_flags,
                                              0u,
                                              misc};
        ID3D11Texture2D *out = shareable(texture, desc.misc_flags);
        result = out != nullptr ? S_OK : device->CreateTexture2D(&texture, nullptr, &out);
        made = out;
    } else if (desc.dimension == dimension_texture3d) {
        if (desc.width > 2048u || desc.height > 2048u || desc.depth_or_array > 2048u) return false;
        const D3D11_TEXTURE3D_DESC texture = {desc.width,         desc.height, desc.depth_or_array,
                                              desc.mip_levels,    static_cast<DXGI_FORMAT>(desc.format),
                                              D3D11_USAGE_DEFAULT, desc.bind_flags, 0u, misc};
        ID3D11Texture3D *out = nullptr;
        result = device->CreateTexture3D(&texture, nullptr, &out);
        made = out;
    } else {
        return false;
    }
    /* A resource the host will not make is the guest's to learn about at its
     * next Finish, as a real driver's out-of-memory would be; the id then names
     * nothing and every command naming it is refused. */
    if (FAILED(result) || made == nullptr) return true;
    if (objects_.keep(id, D3dObjectKind::resource, made) != Status::ok) return false;
    /* The surface a present of it publishes on (SharedSurfaces). */
    ID3D11DeviceChild *child = nullptr;
    if (shared_key != 0u &&
        SUCCEEDED(made->QueryInterface(__uuidof(ID3D11DeviceChild), reinterpret_cast<void **>(&child)))) {
        static_cast<void>(SurfaceIdentity(shared_key).mark(child));
        child->Release();
    }
    /* A key refused is one no other process can open: the host log is where
     * the refusal is seen, since the creator is not told. */
    const Status published =
        shared_key != 0u ? shares_->publish_resource(shared_key, made, ShareOrigin::d3d11) : Status::ok;
    if (published != Status::ok)
        LogLine(lines_)
            .text("shared key ")
            .hex(shared_key, 16u)
            .text(" not shared, status ")
            .number(static_cast<uint32_t>(published))
            .write();
    if (published == Status::ok) describe_shared_image(shared_key, made);
    return true;
}

/* A 2D texture made over exportable memory is described as the image it is,
 * so an API that opens its memory - OpenCL - views it as that image with no
 * copy (SharedImageShape). */
void D3dBackend::describe_shared_image(uint64_t shared_key, IUnknown *made)
{
    AV_SCOPE("D3dBackend::describe_shared_image");
    ID3D11Texture2D *texture = nullptr;
    if (FAILED(made->QueryInterface(__uuidof(ID3D11Texture2D), reinterpret_cast<void **>(&texture)))) return;
    D3D11_TEXTURE2D_DESC described = {};
    texture->GetDesc(&described);
    uint32_t pitch = 0u;
    if (described.MipLevels == 1u && described.ArraySize == 1u && described.SampleDesc.Count == 1u &&
        device11_->exportable_pitch(texture, pitch))
        static_cast<void>(shares_->describe_image(
            shared_key, SharedImageShape(described.Width, described.Height, static_cast<uint32_t>(described.Format), pitch)));
    texture->Release();
}

bool D3dBackend::on_OpenResource(uint32_t id, uint64_t shared_key, uint32_t &found, d3dproto::ResourceDesc &desc)
{
    AV_SCOPE("D3dBackend::on_OpenResource");
    found = 0u;
    desc = d3dproto::ResourceDesc{};
    if (!has_device()) return false;
    /* A flip-model buffer is read as its latest present's copy (show). */
    IUnknown *const shared = device11_->open_shared(shares_->domain(), shared_key);
    if (shared == nullptr) {
        if (shared_key != missed_key_)
            LogLine(lines_)
                .text("no resource shared under key ")
                .hex(shared_key, 16u)
                .text("; ")
                .number(shares_->bound() ? shares_->shares()->live() : 0u)
                .text(" shared")
                .write();
        missed_key_ = shared_key;
        return true;
    }
    ID3D11Resource *resource = nullptr;
    const HRESULT cast = shared->QueryInterface(__uuidof(ID3D11Resource), reinterpret_cast<void **>(&resource));
    shared->Release();
    if (FAILED(cast)) return true;
    D3D11_RESOURCE_DIMENSION dimension = D3D11_RESOURCE_DIMENSION_UNKNOWN;
    resource->GetType(&dimension);
    desc.dimension = static_cast<uint32_t>(dimension);
    if (dimension == D3D11_RESOURCE_DIMENSION_TEXTURE2D) {
        D3D11_TEXTURE2D_DESC texture = {};
        static_cast<ID3D11Texture2D *>(resource)->GetDesc(&texture);
        desc.width = texture.Width;
        desc.height = texture.Height;
        desc.depth_or_array = texture.ArraySize;
        desc.mip_levels = texture.MipLevels;
        desc.format = static_cast<uint32_t>(texture.Format);
        desc.sample_count = texture.SampleDesc.Count;
        desc.sample_quality = texture.SampleDesc.Quality;
        desc.bind_flags = texture.BindFlags;
        desc.misc_flags = texture.MiscFlags;
    } else if (dimension == D3D11_RESOURCE_DIMENSION_BUFFER) {
        D3D11_BUFFER_DESC buffer = {};
        static_cast<ID3D11Buffer *>(resource)->GetDesc(&buffer);
        desc.width = buffer.ByteWidth;
        desc.bind_flags = buffer.BindFlags;
        desc.misc_flags = buffer.MiscFlags;
        desc.structure_stride = buffer.StructureByteStride;
    }
    if (objects_.keep(id, D3dObjectKind::resource, resource) != Status::ok) return false;
    found = 1u;
    return true;
}

bool D3dBackend::on_CreateShaderResourceView(uint32_t id, const d3dproto::ViewDesc &desc)
{
    AV_SCOPE("D3dBackend::on_CreateShaderResourceView");
    ID3D11Resource *const resource = objects_.find_as<ID3D11Resource>(desc.resource, D3dObjectKind::resource);
    if (resource == nullptr || !DxgiFormatLayout(desc.format).known()) return false;
    D3D11_SHADER_RESOURCE_VIEW_DESC view = {};
    view.Format = static_cast<DXGI_FORMAT>(desc.format);
    view.ViewDimension = static_cast<D3D11_SRV_DIMENSION>(desc.dimension);
    static_assert(sizeof(view.Buffer) <= sizeof(desc.args));
    /* Every SRV dimension's union arm is up to four UINTs, BufferEx's flags
     * the fourth's third; they are laid out as the API lays them. */
    __builtin_memcpy(&view.Buffer, desc.args, sizeof(uint32_t) * 4u);
    if (view.ViewDimension == D3D11_SRV_DIMENSION_BUFFEREX) view.BufferEx.Flags = desc.flags;
    if (desc.dimension > D3D11_SRV_DIMENSION_BUFFEREX) return false;
    ID3D11ShaderResourceView *made = nullptr;
    if (FAILED(device11_->device()->CreateShaderResourceView(resource, &view, &made))) return true;
    return objects_.keep(id, D3dObjectKind::shader_resource_view, made) == Status::ok;
}

bool D3dBackend::on_CreateRenderTargetView(uint32_t id, const d3dproto::ViewDesc &desc)
{
    AV_SCOPE("D3dBackend::on_CreateRenderTargetView");
    ID3D11Resource *const resource = objects_.find_as<ID3D11Resource>(desc.resource, D3dObjectKind::resource);
    if (resource == nullptr || !DxgiFormatLayout(desc.format).known() || desc.dimension > D3D11_RTV_DIMENSION_TEXTURE3D)
        return false;
    D3D11_RENDER_TARGET_VIEW_DESC view = {};
    view.Format = static_cast<DXGI_FORMAT>(desc.format);
    view.ViewDimension = static_cast<D3D11_RTV_DIMENSION>(desc.dimension);
    __builtin_memcpy(&view.Buffer, desc.args, sizeof(uint32_t) * 3u);
    ID3D11RenderTargetView *made = nullptr;
    if (FAILED(device11_->device()->CreateRenderTargetView(resource, &view, &made))) return true;
    return objects_.keep(id, D3dObjectKind::render_target_view, made) == Status::ok;
}

bool D3dBackend::on_CreateDepthStencilView(uint32_t id, const d3dproto::ViewDesc &desc)
{
    AV_SCOPE("D3dBackend::on_CreateDepthStencilView");
    ID3D11Resource *const resource = objects_.find_as<ID3D11Resource>(desc.resource, D3dObjectKind::resource);
    if (resource == nullptr || !DxgiFormatLayout(desc.format).known() || desc.dimension > D3D11_DSV_DIMENSION_TEXTURE2DMSARRAY)
        return false;
    D3D11_DEPTH_STENCIL_VIEW_DESC view = {};
    view.Format = static_cast<DXGI_FORMAT>(desc.format);
    view.ViewDimension = static_cast<D3D11_DSV_DIMENSION>(desc.dimension);
    view.Flags = desc.flags & (D3D11_DSV_READ_ONLY_DEPTH | D3D11_DSV_READ_ONLY_STENCIL);
    __builtin_memcpy(&view.Texture1D, desc.args, sizeof(uint32_t) * 3u);
    ID3D11DepthStencilView *made = nullptr;
    if (FAILED(device11_->device()->CreateDepthStencilView(resource, &view, &made))) return true;
    return objects_.keep(id, D3dObjectKind::depth_stencil_view, made) == Status::ok;
}

bool D3dBackend::on_CreateUnorderedAccessView(uint32_t id, const d3dproto::ViewDesc &desc)
{
    AV_SCOPE("D3dBackend::on_CreateUnorderedAccessView");
    ID3D11Resource *const resource = objects_.find_as<ID3D11Resource>(desc.resource, D3dObjectKind::resource);
    if (resource == nullptr || !DxgiFormatLayout(desc.format).known() || desc.dimension > D3D11_UAV_DIMENSION_TEXTURE3D)
        return false;
    D3D11_UNORDERED_ACCESS_VIEW_DESC view = {};
    view.Format = static_cast<DXGI_FORMAT>(desc.format);
    view.ViewDimension = static_cast<D3D11_UAV_DIMENSION>(desc.dimension);
    __builtin_memcpy(&view.Buffer, desc.args, sizeof(uint32_t) * 3u);
    if (view.ViewDimension == D3D11_UAV_DIMENSION_BUFFER) view.Buffer.Flags = desc.flags;
    ID3D11UnorderedAccessView *made = nullptr;
    if (FAILED(device11_->device()->CreateUnorderedAccessView(resource, &view, &made))) return true;
    return objects_.keep(id, D3dObjectKind::unordered_access_view, made) == Status::ok;
}

bool D3dBackend::on_CreateShader(uint32_t id, uint32_t stage, uint32_t input_count,
                                 const d3dproto::SignatureEntry *inputs, uint32_t output_count,
                                 const d3dproto::SignatureEntry *outputs, uint32_t patch_count,
                                 const d3dproto::SignatureEntry *patch_constants, uint32_t stream_out_count,
                                 const d3dproto::StreamOutEntry *stream_out, uint32_t stride_count,
                                 const uint32_t *strides, uint32_t rasterized_stream, const uint8_t *code,
                                 uint64_t code_size)
{
    AV_SCOPE("D3dBackend::on_CreateShader");
    const D3dObjectKind kind = shader_kind(stage);
    if (!has_device() || kind == D3dObjectKind::none || code == nullptr) return false;
    /* The container is the code and three signatures: as big as the code plus
     * what the signatures' tables and names can take. */
    const size_t capacity = static_cast<size_t>(code_size) + container_slack;
    uint8_t *const storage = arena_ == nullptr ? nullptr : arena_->allocate<uint8_t>(capacity);
    if (storage == nullptr) return false;
    DxbcContainer container(storage, capacity);
    const bool pixel = stage == stage_pixel;
    if (!container.add_signature(DxbcContainer::Role::input, inputs, input_count, false) ||
        !container.add_signature(DxbcContainer::Role::output, outputs, output_count, pixel) ||
        (patch_count != 0u &&
         !container.add_signature(DxbcContainer::Role::patch_constant, patch_constants, patch_count, false)) ||
        !container.add_code(code, static_cast<size_t>(code_size)))
        return false;
    const size_t size = container.finish();
    if (size == 0u) return false;
    ID3D11Device5 *const device = device11_->device();
    HRESULT result = E_INVALIDARG;
    IUnknown *made = nullptr;
    switch (stage) {
    case stage_vertex: {
        ID3D11VertexShader *out = nullptr;
        result = device->CreateVertexShader(storage, size, nullptr, &out);
        made = out;
        break;
    }
    case stage_hull: {
        ID3D11HullShader *out = nullptr;
        result = device->CreateHullShader(storage, size, nullptr, &out);
        made = out;
        break;
    }
    case stage_domain: {
        ID3D11DomainShader *out = nullptr;
        result = device->CreateDomainShader(storage, size, nullptr, &out);
        made = out;
        break;
    }
    case stage_geometry: {
        ID3D11GeometryShader *out = nullptr;
        if (stream_out_count == 0u) {
            result = device->CreateGeometryShader(storage, size, nullptr, &out);
        } else {
            D3D11_SO_DECLARATION_ENTRY entries[128] = {};
            for (uint32_t index = 0u; index < stream_out_count; index++) {
                const d3dproto::StreamOutEntry &entry = stream_out[index];
                entries[index].Stream = entry.stream;
                entries[index].OutputSlot = static_cast<BYTE>(entry.output_slot);
                const uint32_t mask = entry.mask & 0xfu;
                entries[index].StartComponent = mask == 0u ? 0u : static_cast<BYTE>(__builtin_ctz(mask));
                entries[index].ComponentCount = static_cast<BYTE>(__builtin_popcount(mask));
                if (entry.register_index == no_stream) continue;
                d3dproto::SignatureEntry named = {};
                for (uint32_t output = 0u; output < output_count; output++)
                    if (outputs[output].register_index == entry.register_index &&
                        outputs[output].stream == entry.stream)
                        named = outputs[output];
                uint32_t semantic = 0u;
                entries[index].SemanticName = container.semantic_of(named, false, semantic);
                entries[index].SemanticIndex = semantic;
            }
            result = device->CreateGeometryShaderWithStreamOutput(
                storage, size, entries, stream_out_count, strides, stride_count,
                rasterized_stream == no_stream ? D3D11_SO_NO_RASTERIZED_STREAM : rasterized_stream, nullptr, &out);
        }
        made = out;
        break;
    }
    case stage_pixel: {
        ID3D11PixelShader *out = nullptr;
        result = device->CreatePixelShader(storage, size, nullptr, &out);
        made = out;
        break;
    }
    default: {
        ID3D11ComputeShader *out = nullptr;
        result = device->CreateComputeShader(storage, size, nullptr, &out);
        made = out;
        break;
    }
    }
    if (FAILED(result) || made == nullptr) return true;
    return objects_.keep(id, kind, made) == Status::ok;
}

bool D3dBackend::on_CreateInputLayout(uint32_t id, uint32_t element_count, const d3dproto::InputElement *elements)
{
    AV_SCOPE("D3dBackend::on_CreateInputLayout");
    if (!has_device()) return false;
    D3D11_INPUT_ELEMENT_DESC descs[32] = {};
    uint32_t registers[32] = {};
    uint32_t distinct = 0u;
    for (uint32_t index = 0u; index < element_count; index++) {
        const d3dproto::InputElement &element = elements[index];
        if (!DxgiFormatLayout(element.format).known() || element.input_slot >= 32u || element.input_register >= 32u)
            return false;
        descs[index].SemanticName = "AV";
        descs[index].SemanticIndex = element.input_register * 4u;
        descs[index].Format = static_cast<DXGI_FORMAT>(element.format);
        descs[index].InputSlot = element.input_slot;
        descs[index].AlignedByteOffset = element.aligned_byte_offset;
        descs[index].InputSlotClass = static_cast<D3D11_INPUT_CLASSIFICATION>(element.classification);
        descs[index].InstanceDataStepRate = element.step_rate;
        bool seen = false;
        for (uint32_t earlier = 0u; earlier < distinct; earlier++) seen = seen || registers[earlier] == element.input_register;
        if (!seen) registers[distinct++] = element.input_register;
    }
    uint8_t storage[4096];
    DxbcContainer container(storage, sizeof(storage));
    if (!container.add_layout_signature(registers, distinct)) return false;
    const size_t size = container.finish();
    if (size == 0u) return false;
    ID3D11InputLayout *made = nullptr;
    if (FAILED(device11_->device()->CreateInputLayout(descs, element_count, storage, size, &made))) return true;
    return objects_.keep(id, D3dObjectKind::input_layout, made) == Status::ok;
}

bool D3dBackend::on_CreateBlendState(uint32_t id, const d3dproto::BlendDesc &desc)
{
    AV_SCOPE("D3dBackend::on_CreateBlendState");
    if (!has_device()) return false;
    D3D11_BLEND_DESC1 blend = {};
    blend.AlphaToCoverageEnable = desc.alpha_to_coverage != 0u;
    blend.IndependentBlendEnable = desc.independent_blend != 0u;
    for (uint32_t target = 0u; target < 8u; target++) {
        const d3dproto::RenderTargetBlend &in = desc.targets[target];
        D3D11_RENDER_TARGET_BLEND_DESC1 &out = blend.RenderTarget[target];
        out.BlendEnable = in.blend_enable != 0u;
        out.LogicOpEnable = in.logic_op_enable != 0u;
        out.SrcBlend = static_cast<D3D11_BLEND>(in.src_blend);
        out.DestBlend = static_cast<D3D11_BLEND>(in.dest_blend);
        out.BlendOp = static_cast<D3D11_BLEND_OP>(in.blend_op);
        out.SrcBlendAlpha = static_cast<D3D11_BLEND>(in.src_blend_alpha);
        out.DestBlendAlpha = static_cast<D3D11_BLEND>(in.dest_blend_alpha);
        out.BlendOpAlpha = static_cast<D3D11_BLEND_OP>(in.blend_op_alpha);
        out.LogicOp = static_cast<D3D11_LOGIC_OP>(in.logic_op);
        out.RenderTargetWriteMask = static_cast<UINT8>(in.write_mask);
    }
    ID3D11BlendState1 *made = nullptr;
    if (FAILED(device11_->device()->CreateBlendState1(&blend, &made))) return true;
    return objects_.keep(id, D3dObjectKind::blend_state, made) == Status::ok;
}

bool D3dBackend::on_CreateDepthStencilState(uint32_t id, const d3dproto::DepthStencilDesc &desc)
{
    AV_SCOPE("D3dBackend::on_CreateDepthStencilState");
    if (!has_device()) return false;
    D3D11_DEPTH_STENCIL_DESC state = {};
    state.DepthEnable = desc.depth_enable != 0u;
    state.DepthWriteMask = static_cast<D3D11_DEPTH_WRITE_MASK>(desc.depth_write_mask);
    state.DepthFunc = static_cast<D3D11_COMPARISON_FUNC>(desc.depth_func);
    state.StencilEnable = desc.stencil_enable != 0u;
    state.StencilReadMask = static_cast<UINT8>(desc.stencil_read_mask);
    state.StencilWriteMask = static_cast<UINT8>(desc.stencil_write_mask);
    const d3dproto::StencilOp *const faces[2] = {&desc.front, &desc.back};
    D3D11_DEPTH_STENCILOP_DESC *const outs[2] = {&state.FrontFace, &state.BackFace};
    for (uint32_t face = 0u; face < 2u; face++) {
        outs[face]->StencilFailOp = static_cast<D3D11_STENCIL_OP>(faces[face]->fail);
        outs[face]->StencilDepthFailOp = static_cast<D3D11_STENCIL_OP>(faces[face]->depth_fail);
        outs[face]->StencilPassOp = static_cast<D3D11_STENCIL_OP>(faces[face]->pass);
        outs[face]->StencilFunc = static_cast<D3D11_COMPARISON_FUNC>(faces[face]->func);
    }
    ID3D11DepthStencilState *made = nullptr;
    if (FAILED(device11_->device()->CreateDepthStencilState(&state, &made))) return true;
    return objects_.keep(id, D3dObjectKind::depth_stencil_state, made) == Status::ok;
}

bool D3dBackend::on_CreateRasterizerState(uint32_t id, const d3dproto::RasterizerDesc &desc)
{
    AV_SCOPE("D3dBackend::on_CreateRasterizerState");
    if (!has_device()) return false;
    D3D11_RASTERIZER_DESC2 state = {};
    state.FillMode = static_cast<D3D11_FILL_MODE>(desc.fill_mode);
    state.CullMode = static_cast<D3D11_CULL_MODE>(desc.cull_mode);
    state.FrontCounterClockwise = desc.front_counter_clockwise != 0u;
    state.DepthBias = desc.depth_bias;
    state.DepthBiasClamp = desc.depth_bias_clamp;
    state.SlopeScaledDepthBias = desc.slope_scaled_depth_bias;
    state.DepthClipEnable = desc.depth_clip != 0u;
    state.ScissorEnable = desc.scissor != 0u;
    state.MultisampleEnable = desc.multisample != 0u;
    state.AntialiasedLineEnable = desc.antialiased_line != 0u;
    state.ForcedSampleCount = desc.forced_sample_count;
    state.ConservativeRaster = static_cast<D3D11_CONSERVATIVE_RASTERIZATION_MODE>(desc.conservative);
    ID3D11RasterizerState2 *made = nullptr;
    if (FAILED(device11_->device()->CreateRasterizerState2(&state, &made))) return true;
    return objects_.keep(id, D3dObjectKind::rasterizer_state, made) == Status::ok;
}

bool D3dBackend::on_CreateSampler(uint32_t id, const d3dproto::SamplerDesc &desc)
{
    AV_SCOPE("D3dBackend::on_CreateSampler");
    if (!has_device()) return false;
    D3D11_SAMPLER_DESC sampler = {};
    sampler.Filter = static_cast<D3D11_FILTER>(desc.filter);
    sampler.AddressU = static_cast<D3D11_TEXTURE_ADDRESS_MODE>(desc.address_u);
    sampler.AddressV = static_cast<D3D11_TEXTURE_ADDRESS_MODE>(desc.address_v);
    sampler.AddressW = static_cast<D3D11_TEXTURE_ADDRESS_MODE>(desc.address_w);
    sampler.MipLODBias = desc.mip_lod_bias;
    sampler.MaxAnisotropy = desc.max_anisotropy;
    sampler.ComparisonFunc = static_cast<D3D11_COMPARISON_FUNC>(desc.comparison);
    for (uint32_t index = 0u; index < 4u; index++) sampler.BorderColor[index] = desc.border_color[index];
    sampler.MinLOD = desc.min_lod;
    sampler.MaxLOD = desc.max_lod;
    ID3D11SamplerState *made = nullptr;
    if (FAILED(device11_->device()->CreateSamplerState(&sampler, &made))) return true;
    return objects_.keep(id, D3dObjectKind::sampler, made) == Status::ok;
}

bool D3dBackend::on_CreateQuery(uint32_t id, uint32_t type, uint32_t misc_flags)
{
    AV_SCOPE("D3dBackend::on_CreateQuery");
    if (!has_device() || type > D3D11_QUERY_SO_OVERFLOW_PREDICATE_STREAM3) return false;
    const D3D11_QUERY_DESC query = {static_cast<D3D11_QUERY>(type), misc_flags & D3D11_QUERY_MISC_PREDICATEHINT};
    ID3D11Query *made = nullptr;
    if (FAILED(device11_->device()->CreateQuery(&query, &made))) return true;
    return objects_.keep(id, D3dObjectKind::query, made) == Status::ok;
}

/* ---------------------------------------------------------------- binding */

bool D3dBackend::on_SetShader(uint32_t stage, uint32_t shader)
{
    AV_SCOPE("D3dBackend::on_SetShader");
    const D3dObjectKind kind = shader_kind(stage);
    if (!ready() || kind == D3dObjectKind::none) return false;
    IUnknown *const found = objects_.find(shader, kind);
    if (shader != 0u && found == nullptr) return false;
    ID3D11DeviceContext4 *const context = device11_->context();
    switch (stage) {
    case stage_vertex: context->VSSetShader(static_cast<ID3D11VertexShader *>(found), nullptr, 0u); break;
    case stage_hull: context->HSSetShader(static_cast<ID3D11HullShader *>(found), nullptr, 0u); break;
    case stage_domain: context->DSSetShader(static_cast<ID3D11DomainShader *>(found), nullptr, 0u); break;
    case stage_geometry: context->GSSetShader(static_cast<ID3D11GeometryShader *>(found), nullptr, 0u); break;
    case stage_pixel: context->PSSetShader(static_cast<ID3D11PixelShader *>(found), nullptr, 0u); break;
    default: context->CSSetShader(static_cast<ID3D11ComputeShader *>(found), nullptr, 0u); break;
    }
    return true;
}

bool D3dBackend::on_SetConstantBuffers(uint32_t stage, uint32_t start, uint32_t count,
                                       const d3dproto::ConstantBufferBinding *buffers)
{
    AV_SCOPE("D3dBackend::on_SetConstantBuffers");
    if (!ready() || stage >= stage_count || start + count > D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT)
        return false;
    ID3D11Buffer *bound[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT] = {};
    UINT first[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT] = {};
    UINT counts[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT] = {};
    for (uint32_t index = 0u; index < count; index++) {
        bound[index] = objects_.find_as<ID3D11Buffer>(buffers[index].buffer, D3dObjectKind::resource);
        if (buffers[index].buffer != 0u && bound[index] == nullptr) return false;
        first[index] = buffers[index].first_constant;
        counts[index] = buffers[index].num_constants == 0u ? D3D11_REQ_CONSTANT_BUFFER_ELEMENT_COUNT
                                                           : buffers[index].num_constants;
    }
    uint32_t ids[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT] = {};
    for (uint32_t index = 0u; index < count; index++) ids[index] = buffers[index].buffer;
    if (constants_.bind(states_.selected_id(), stage, start, count, ids, bound, first, counts) != Status::ok)
        return false;
    ID3D11DeviceContext4 *const context = device11_->context();
    switch (stage) {
    case stage_vertex: context->VSSetConstantBuffers1(start, count, bound, first, counts); break;
    case stage_hull: context->HSSetConstantBuffers1(start, count, bound, first, counts); break;
    case stage_domain: context->DSSetConstantBuffers1(start, count, bound, first, counts); break;
    case stage_geometry: context->GSSetConstantBuffers1(start, count, bound, first, counts); break;
    case stage_pixel: context->PSSetConstantBuffers1(start, count, bound, first, counts); break;
    default: context->CSSetConstantBuffers1(start, count, bound, first, counts); break;
    }
    return true;
}

bool D3dBackend::on_SetShaderResources(uint32_t stage, uint32_t start, uint32_t count, const uint32_t *views)
{
    AV_SCOPE("D3dBackend::on_SetShaderResources");
    if (!ready() || stage >= stage_count || start + count > D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT) return false;
    if (!all_of(views, count, D3dObjectKind::shader_resource_view)) return false;
    ID3D11ShaderResourceView *bound[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT] = {};
    for (uint32_t index = 0u; index < count; index++)
        bound[index] = objects_.find_as<ID3D11ShaderResourceView>(views[index], D3dObjectKind::shader_resource_view);
    ID3D11DeviceContext4 *const context = device11_->context();
    switch (stage) {
    case stage_vertex: context->VSSetShaderResources(start, count, bound); break;
    case stage_hull: context->HSSetShaderResources(start, count, bound); break;
    case stage_domain: context->DSSetShaderResources(start, count, bound); break;
    case stage_geometry: context->GSSetShaderResources(start, count, bound); break;
    case stage_pixel: context->PSSetShaderResources(start, count, bound); break;
    default: context->CSSetShaderResources(start, count, bound); break;
    }
    return true;
}

bool D3dBackend::on_SetSamplers(uint32_t stage, uint32_t start, uint32_t count, const uint32_t *samplers)
{
    AV_SCOPE("D3dBackend::on_SetSamplers");
    if (!ready() || stage >= stage_count || start + count > D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT) return false;
    if (!all_of(samplers, count, D3dObjectKind::sampler)) return false;
    ID3D11SamplerState *bound[D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT] = {};
    for (uint32_t index = 0u; index < count; index++)
        bound[index] = objects_.find_as<ID3D11SamplerState>(samplers[index], D3dObjectKind::sampler);
    ID3D11DeviceContext4 *const context = device11_->context();
    switch (stage) {
    case stage_vertex: context->VSSetSamplers(start, count, bound); break;
    case stage_hull: context->HSSetSamplers(start, count, bound); break;
    case stage_domain: context->DSSetSamplers(start, count, bound); break;
    case stage_geometry: context->GSSetSamplers(start, count, bound); break;
    case stage_pixel: context->PSSetSamplers(start, count, bound); break;
    default: context->CSSetSamplers(start, count, bound); break;
    }
    return true;
}

bool D3dBackend::on_SetComputeUnorderedAccessViews(uint32_t start, uint32_t count, const d3dproto::UavBinding *views)
{
    AV_SCOPE("D3dBackend::on_SetComputeUnorderedAccessViews");
    if (!ready() || start + count > D3D11_1_UAV_SLOT_COUNT) return false;
    ID3D11UnorderedAccessView *bound[D3D11_1_UAV_SLOT_COUNT] = {};
    UINT counts[D3D11_1_UAV_SLOT_COUNT] = {};
    for (uint32_t index = 0u; index < count; index++) {
        bound[index] = objects_.find_as<ID3D11UnorderedAccessView>(views[index].view, D3dObjectKind::unordered_access_view);
        if (views[index].view != 0u && bound[index] == nullptr) return false;
        counts[index] = views[index].initial_count;
    }
    device11_->context()->CSSetUnorderedAccessViews(start, count, bound, counts);
    return true;
}

bool D3dBackend::on_SetRenderTargets(uint32_t rtv_count, const uint32_t *rtvs, uint32_t dsv, uint32_t uav_start,
                                     uint32_t uav_count, const d3dproto::UavBinding *uavs)
{
    AV_SCOPE("D3dBackend::on_SetRenderTargets");
    if (!ready() || rtv_count > D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT || uav_start + uav_count > D3D11_1_UAV_SLOT_COUNT)
        return false;
    if (!all_of(rtvs, rtv_count, D3dObjectKind::render_target_view)) return false;
    ID3D11RenderTargetView *targets[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT] = {};
    for (uint32_t index = 0u; index < rtv_count; index++)
        targets[index] = objects_.find_as<ID3D11RenderTargetView>(rtvs[index], D3dObjectKind::render_target_view);
    ID3D11DepthStencilView *const depth = objects_.find_as<ID3D11DepthStencilView>(dsv, D3dObjectKind::depth_stencil_view);
    if (dsv != 0u && depth == nullptr) return false;
    ID3D11UnorderedAccessView *views[D3D11_1_UAV_SLOT_COUNT] = {};
    UINT counts[D3D11_1_UAV_SLOT_COUNT] = {};
    for (uint32_t index = 0u; index < uav_count; index++) {
        views[index] = objects_.find_as<ID3D11UnorderedAccessView>(uavs[index].view, D3dObjectKind::unordered_access_view);
        if (uavs[index].view != 0u && views[index] == nullptr) return false;
        counts[index] = uavs[index].initial_count;
    }
    if (uav_count == 0u) {
        device11_->context()->OMSetRenderTargetsAndUnorderedAccessViews(
            rtv_count, targets, depth, 0u, D3D11_KEEP_UNORDERED_ACCESS_VIEWS, nullptr, nullptr);
        return true;
    }
    device11_->context()->OMSetRenderTargetsAndUnorderedAccessViews(rtv_count, targets, depth, uav_start, uav_count,
                                                                    views, counts);
    return true;
}

bool D3dBackend::on_SetInputLayout(uint32_t layout)
{
    AV_SCOPE("D3dBackend::on_SetInputLayout");
    ID3D11InputLayout *const found = objects_.find_as<ID3D11InputLayout>(layout, D3dObjectKind::input_layout);
    if (!ready() || (layout != 0u && found == nullptr)) return false;
    device11_->context()->IASetInputLayout(found);
    return true;
}

bool D3dBackend::on_SetVertexBuffers(uint32_t start, uint32_t count, const d3dproto::VertexBufferBinding *buffers)
{
    AV_SCOPE("D3dBackend::on_SetVertexBuffers");
    if (!ready() || start + count > D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT) return false;
    ID3D11Buffer *bound[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT] = {};
    UINT strides[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT] = {};
    UINT offsets[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT] = {};
    for (uint32_t index = 0u; index < count; index++) {
        bound[index] = objects_.find_as<ID3D11Buffer>(buffers[index].buffer, D3dObjectKind::resource);
        if (buffers[index].buffer != 0u && bound[index] == nullptr) return false;
        strides[index] = buffers[index].stride;
        offsets[index] = buffers[index].offset;
    }
    device11_->context()->IASetVertexBuffers(start, count, bound, strides, offsets);
    return true;
}

bool D3dBackend::on_SetIndexBuffer(uint32_t buffer, uint32_t format, uint32_t offset)
{
    AV_SCOPE("D3dBackend::on_SetIndexBuffer");
    ID3D11Buffer *const found = objects_.find_as<ID3D11Buffer>(buffer, D3dObjectKind::resource);
    if (!ready() || (buffer != 0u && found == nullptr) || !DxgiFormatLayout(format).known()) return false;
    device11_->context()->IASetIndexBuffer(found, static_cast<DXGI_FORMAT>(format), offset);
    return true;
}

bool D3dBackend::on_SetTopology(uint32_t topology)
{
    AV_SCOPE("D3dBackend::on_SetTopology");
    if (!ready() || topology > D3D11_PRIMITIVE_TOPOLOGY_32_CONTROL_POINT_PATCHLIST) return false;
    device11_->context()->IASetPrimitiveTopology(static_cast<D3D11_PRIMITIVE_TOPOLOGY>(topology));
    return true;
}

bool D3dBackend::on_SetViewports(uint32_t count, const d3dproto::Viewport *viewports)
{
    AV_SCOPE("D3dBackend::on_SetViewports");
    if (!ready()) return false;
    D3D11_VIEWPORT bound[16] = {};
    for (uint32_t index = 0u; index < count; index++)
        bound[index] = D3D11_VIEWPORT{viewports[index].x,     viewports[index].y,         viewports[index].width,
                                      viewports[index].height, viewports[index].min_depth, viewports[index].max_depth};
    device11_->context()->RSSetViewports(count, bound);
    return true;
}

bool D3dBackend::on_SetScissorRects(uint32_t count, const d3dproto::Rect *rects)
{
    AV_SCOPE("D3dBackend::on_SetScissorRects");
    if (!ready()) return false;
    D3D11_RECT bound[16] = {};
    for (uint32_t index = 0u; index < count; index++)
        bound[index] = D3D11_RECT{rects[index].left, rects[index].top, rects[index].right, rects[index].bottom};
    device11_->context()->RSSetScissorRects(count, bound);
    return true;
}

bool D3dBackend::on_SetBlendState(uint32_t state, const d3dproto::Float4 &factor, uint32_t sample_mask)
{
    AV_SCOPE("D3dBackend::on_SetBlendState");
    ID3D11BlendState *const found = objects_.find_as<ID3D11BlendState>(state, D3dObjectKind::blend_state);
    if (!ready() || (state != 0u && found == nullptr)) return false;
    device11_->context()->OMSetBlendState(found, factor.v, sample_mask);
    return true;
}

bool D3dBackend::on_SetDepthStencilState(uint32_t state, uint32_t stencil_ref)
{
    AV_SCOPE("D3dBackend::on_SetDepthStencilState");
    ID3D11DepthStencilState *const found = objects_.find_as<ID3D11DepthStencilState>(state, D3dObjectKind::depth_stencil_state);
    if (!ready() || (state != 0u && found == nullptr)) return false;
    device11_->context()->OMSetDepthStencilState(found, stencil_ref);
    return true;
}

bool D3dBackend::on_SetRasterizerState(uint32_t state)
{
    AV_SCOPE("D3dBackend::on_SetRasterizerState");
    ID3D11RasterizerState *const found = objects_.find_as<ID3D11RasterizerState>(state, D3dObjectKind::rasterizer_state);
    if (!ready() || (state != 0u && found == nullptr)) return false;
    device11_->context()->RSSetState(found);
    return true;
}

bool D3dBackend::on_SetStreamOutTargets(uint32_t count, const d3dproto::StreamOutTarget *targets)
{
    AV_SCOPE("D3dBackend::on_SetStreamOutTargets");
    if (!ready()) return false;
    ID3D11Buffer *bound[4] = {};
    UINT offsets[4] = {};
    for (uint32_t index = 0u; index < count; index++) {
        bound[index] = objects_.find_as<ID3D11Buffer>(targets[index].buffer, D3dObjectKind::resource);
        if (targets[index].buffer != 0u && bound[index] == nullptr) return false;
        offsets[index] = targets[index].offset;
    }
    device11_->context()->SOSetTargets(count, bound, offsets);
    return true;
}

bool D3dBackend::on_SetPredication(uint32_t query, uint32_t value)
{
    AV_SCOPE("D3dBackend::on_SetPredication");
    ID3D11Query *const found = objects_.find_as<ID3D11Query>(query, D3dObjectKind::query);
    if (!ready() || (query != 0u && found == nullptr)) return false;
    ID3D11Predicate *predicate = nullptr;
    if (found != nullptr &&
        FAILED(found->QueryInterface(__uuidof(ID3D11Predicate), reinterpret_cast<void **>(&predicate))))
        return false;
    device11_->context()->SetPredication(predicate, value != 0u);
    if (predicate != nullptr) predicate->Release();
    return true;
}

bool D3dBackend::on_SetResourceMinLod(uint32_t resource, float min_lod)
{
    AV_SCOPE("D3dBackend::on_SetResourceMinLod");
    ID3D11Resource *const found = objects_.find_as<ID3D11Resource>(resource, D3dObjectKind::resource);
    if (!ready() || found == nullptr) return false;
    device11_->context()->SetResourceMinLOD(found, min_lod);
    return true;
}

/* ---------------------------------------------------------------- work */

bool D3dBackend::on_Draw(uint32_t vertex_count, uint32_t start_vertex)
{
    AV_SCOPE("D3dBackend::on_Draw");
    if (!ready()) return false;
    device11_->context()->Draw(vertex_count, start_vertex);
    return true;
}

bool D3dBackend::on_DrawIndexed(uint32_t index_count, uint32_t start_index, int32_t base_vertex)
{
    AV_SCOPE("D3dBackend::on_DrawIndexed");
    if (!ready()) return false;
    device11_->context()->DrawIndexed(index_count, start_index, base_vertex);
    return true;
}

bool D3dBackend::on_DrawInstanced(uint32_t vertex_count, uint32_t instance_count, uint32_t start_vertex,
                                  uint32_t start_instance)
{
    AV_SCOPE("D3dBackend::on_DrawInstanced");
    if (!ready()) return false;
    device11_->context()->DrawInstanced(vertex_count, instance_count, start_vertex, start_instance);
    return true;
}

bool D3dBackend::on_DrawIndexedInstanced(uint32_t index_count, uint32_t instance_count, uint32_t start_index,
                                         int32_t base_vertex, uint32_t start_instance)
{
    AV_SCOPE("D3dBackend::on_DrawIndexedInstanced");
    if (!ready()) return false;
    device11_->context()->DrawIndexedInstanced(index_count, instance_count, start_index, base_vertex, start_instance);
    return true;
}

bool D3dBackend::on_DrawAuto()
{
    AV_SCOPE("D3dBackend::on_DrawAuto");
    if (!ready()) return false;
    device11_->context()->DrawAuto();
    return true;
}

bool D3dBackend::on_DrawInstancedIndirect(uint32_t buffer, uint32_t offset)
{
    AV_SCOPE("D3dBackend::on_DrawInstancedIndirect");
    ID3D11Buffer *const found = objects_.find_as<ID3D11Buffer>(buffer, D3dObjectKind::resource);
    if (!ready() || found == nullptr) return false;
    device11_->context()->DrawInstancedIndirect(found, offset);
    return true;
}

bool D3dBackend::on_DrawIndexedInstancedIndirect(uint32_t buffer, uint32_t offset)
{
    AV_SCOPE("D3dBackend::on_DrawIndexedInstancedIndirect");
    ID3D11Buffer *const found = objects_.find_as<ID3D11Buffer>(buffer, D3dObjectKind::resource);
    if (!ready() || found == nullptr) return false;
    device11_->context()->DrawIndexedInstancedIndirect(found, offset);
    return true;
}

bool D3dBackend::on_Dispatch(uint32_t x, uint32_t y, uint32_t z)
{
    AV_SCOPE("D3dBackend::on_Dispatch");
    if (!ready()) return false;
    device11_->context()->Dispatch(x, y, z);
    return true;
}

bool D3dBackend::on_DispatchIndirect(uint32_t buffer, uint32_t offset)
{
    AV_SCOPE("D3dBackend::on_DispatchIndirect");
    ID3D11Buffer *const found = objects_.find_as<ID3D11Buffer>(buffer, D3dObjectKind::resource);
    if (!ready() || found == nullptr) return false;
    device11_->context()->DispatchIndirect(found, offset);
    return true;
}

/* ---------------------------------------------------------------- clears, copies, data */

bool D3dBackend::on_ClearRenderTargetView(uint32_t view, const d3dproto::Float4 &color)
{
    AV_SCOPE("D3dBackend::on_ClearRenderTargetView");
    ID3D11RenderTargetView *const found = objects_.find_as<ID3D11RenderTargetView>(view, D3dObjectKind::render_target_view);
    if (!ready() || found == nullptr) return false;
    device11_->context()->ClearRenderTargetView(found, color.v);
    return true;
}

bool D3dBackend::on_ClearDepthStencilView(uint32_t view, uint32_t flags, float depth, uint32_t stencil)
{
    AV_SCOPE("D3dBackend::on_ClearDepthStencilView");
    ID3D11DepthStencilView *const found = objects_.find_as<ID3D11DepthStencilView>(view, D3dObjectKind::depth_stencil_view);
    if (!ready() || found == nullptr) return false;
    device11_->context()->ClearDepthStencilView(found, flags & (D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL), depth,
                                                static_cast<UINT8>(stencil));
    return true;
}

bool D3dBackend::on_ClearUnorderedAccessViewUint(uint32_t view, const d3dproto::Uint4 &values)
{
    AV_SCOPE("D3dBackend::on_ClearUnorderedAccessViewUint");
    ID3D11UnorderedAccessView *const found =
        objects_.find_as<ID3D11UnorderedAccessView>(view, D3dObjectKind::unordered_access_view);
    if (!ready() || found == nullptr) return false;
    device11_->context()->ClearUnorderedAccessViewUint(found, values.v);
    return true;
}

bool D3dBackend::on_ClearUnorderedAccessViewFloat(uint32_t view, const d3dproto::Float4 &values)
{
    AV_SCOPE("D3dBackend::on_ClearUnorderedAccessViewFloat");
    ID3D11UnorderedAccessView *const found =
        objects_.find_as<ID3D11UnorderedAccessView>(view, D3dObjectKind::unordered_access_view);
    if (!ready() || found == nullptr) return false;
    device11_->context()->ClearUnorderedAccessViewFloat(found, values.v);
    return true;
}

bool D3dBackend::on_ClearView(uint32_t view, const d3dproto::Float4 &color, uint32_t rect_count,
                              const d3dproto::Rect *rects)
{
    AV_SCOPE("D3dBackend::on_ClearView");
    IUnknown *const found = objects_.find_view(view);
    if (!ready() || found == nullptr) return false;
    D3D11_RECT bound[16] = {};
    for (uint32_t index = 0u; index < rect_count; index++)
        bound[index] = D3D11_RECT{rects[index].left, rects[index].top, rects[index].right, rects[index].bottom};
    device11_->context()->ClearView(static_cast<ID3D11View *>(static_cast<void *>(found)), color.v,
                                    rect_count == 0u ? nullptr : bound, rect_count);
    return true;
}

bool D3dBackend::on_Discard(uint32_t id, uint32_t rect_count, const d3dproto::Rect *rects)
{
    AV_SCOPE("D3dBackend::on_Discard");
    D3dObjectKind kind = D3dObjectKind::none;
    IUnknown *const found = objects_.find_any(id, kind);
    if (!ready() || found == nullptr) return false;
    if (kind == D3dObjectKind::resource) {
        device11_->context()->DiscardResource(static_cast<ID3D11Resource *>(static_cast<void *>(found)));
        return true;
    }
    if (objects_.find_view(id) == nullptr) return false;
    D3D11_RECT bound[16] = {};
    for (uint32_t index = 0u; index < rect_count; index++)
        bound[index] = D3D11_RECT{rects[index].left, rects[index].top, rects[index].right, rects[index].bottom};
    device11_->context()->DiscardView1(static_cast<ID3D11View *>(static_cast<void *>(found)),
                                       rect_count == 0u ? nullptr : bound, rect_count);
    return true;
}

bool D3dBackend::on_CopyResource(uint32_t destination, uint32_t source)
{
    AV_SCOPE("D3dBackend::on_CopyResource");
    ID3D11Resource *const to = objects_.find_as<ID3D11Resource>(destination, D3dObjectKind::resource);
    ID3D11Resource *const from = objects_.find_as<ID3D11Resource>(source, D3dObjectKind::resource);
    if (!ready() || to == nullptr || from == nullptr) return false;
    constants_.release(device11_->context(), states_.selected_id(), destination, static_cast<ID3D11Buffer *>(to));
    constants_.release(device11_->context(), states_.selected_id(), source, static_cast<ID3D11Buffer *>(from));
    device11_->context()->CopyResource(to, from);
    counted(to);
    return true;
}

bool D3dBackend::on_CopySubresourceRegion(uint32_t destination, uint32_t destination_subresource, uint32_t x,
                                          uint32_t y, uint32_t z, uint32_t source, uint32_t source_subresource,
                                          uint32_t has_box, const d3dproto::Box &box, uint32_t copy_flags)
{
    AV_SCOPE("D3dBackend::on_CopySubresourceRegion");
    ID3D11Resource *const to = objects_.find_as<ID3D11Resource>(destination, D3dObjectKind::resource);
    ID3D11Resource *const from = objects_.find_as<ID3D11Resource>(source, D3dObjectKind::resource);
    if (!ready() || to == nullptr || from == nullptr) return false;
    constants_.release(device11_->context(), states_.selected_id(), destination, static_cast<ID3D11Buffer *>(to));
    constants_.release(device11_->context(), states_.selected_id(), source, static_cast<ID3D11Buffer *>(from));
    const D3D11_BOX region = {box.left, box.top, box.front, box.right, box.bottom, box.back};
    device11_->context()->CopySubresourceRegion1(to, destination_subresource, x, y, z, from, source_subresource,
                                                 has_box != 0u ? &region : nullptr,
                                                 copy_flags & (D3D11_COPY_NO_OVERWRITE | D3D11_COPY_DISCARD));
    counted(to);
    return true;
}

/* A copy into a surface the guest shared is counted, under the device lock
 * every command holds, so a process outside QEMU reading the surface can
 * tell which of the guest's copies the host has run (docs/external-export.md). */
void D3dBackend::counted(ID3D11Resource *destination)
{
    AV_SCOPE("D3dBackend::counted");
    shares_->written(SurfaceIdentity(destination).surface());
}

bool D3dBackend::on_ResolveSubresource(uint32_t destination, uint32_t destination_subresource, uint32_t source,
                                       uint32_t source_subresource, uint32_t format)
{
    AV_SCOPE("D3dBackend::on_ResolveSubresource");
    ID3D11Resource *const to = objects_.find_as<ID3D11Resource>(destination, D3dObjectKind::resource);
    ID3D11Resource *const from = objects_.find_as<ID3D11Resource>(source, D3dObjectKind::resource);
    if (!ready() || to == nullptr || from == nullptr || !DxgiFormatLayout(format).known()) return false;
    device11_->context()->ResolveSubresource(to, destination_subresource, from, source_subresource,
                                             static_cast<DXGI_FORMAT>(format));
    return true;
}

bool D3dBackend::on_GenerateMips(uint32_t view)
{
    AV_SCOPE("D3dBackend::on_GenerateMips");
    ID3D11ShaderResourceView *const found = objects_.find_as<ID3D11ShaderResourceView>(view, D3dObjectKind::shader_resource_view);
    if (!ready() || found == nullptr) return false;
    device11_->context()->GenerateMips(found);
    return true;
}

bool D3dBackend::on_CopyStructureCount(uint32_t destination, uint32_t offset, uint32_t source_view)
{
    AV_SCOPE("D3dBackend::on_CopyStructureCount");
    ID3D11Buffer *const to = objects_.find_as<ID3D11Buffer>(destination, D3dObjectKind::resource);
    ID3D11UnorderedAccessView *const from =
        objects_.find_as<ID3D11UnorderedAccessView>(source_view, D3dObjectKind::unordered_access_view);
    if (!ready() || to == nullptr || from == nullptr) return false;
    device11_->context()->CopyStructureCount(to, offset, from);
    return true;
}

bool D3dBackend::on_UpdateSubresource(uint32_t resource, uint32_t subresource, uint32_t has_box,
                                      const d3dproto::Box &box, uint32_t row_pitch, uint32_t depth_pitch,
                                      uint32_t copy_flags, const uint8_t *data, uint64_t data_size)
{
    AV_SCOPE("D3dBackend::on_UpdateSubresource");
    ID3D11Resource *const found = objects_.find_as<ID3D11Resource>(resource, D3dObjectKind::resource);
    if (!ready() || found == nullptr || data == nullptr) return false;
    /* The bytes DXVK will read are rows of row_pitch and slices of
     * depth_pitch over the box; the data that crossed must cover them, or
     * DXVK reads past what the guest sent. */
    uint32_t width = 0u, height = 0u, depth = 0u, format = 0u;
    if (!extent_of(found, subresource, width, height, depth, format)) return false;
    if (has_box != 0u) {
        if (box.right <= box.left || box.bottom <= box.top || box.back <= box.front || box.right > width ||
            box.bottom > height || box.back > depth)
            return false;
        width = box.right - box.left;
        height = box.bottom - box.top;
        depth = box.back - box.front;
    }
    const DxgiFormatLayout layout(format);
    const uint64_t         row = layout.copied_row_bytes(width);
    if (!PitchedExtent(row, layout.rows(height), depth, row_pitch, depth_pitch).within(data_size)) return false;
    if (update_dynamic_buffer(resource, found, has_box != 0u ? box.left : 0u, data, row, copy_flags)) return true;
    constants_.release(device11_->context(), states_.selected_id(), resource, static_cast<ID3D11Buffer *>(found));
    const D3D11_BOX region = {box.left, box.top, box.front, box.right, box.bottom, box.back};
    device11_->context()->UpdateSubresource1(found, subresource, has_box != 0u ? &region : nullptr, data, row_pitch,
                                             depth_pitch, copy_flags & (D3D11_COPY_NO_OVERWRITE | D3D11_COPY_DISCARD));
    return true;
}

/* A dynamic buffer takes its bytes the way the guest wrote them: mapped with
 * DISCARD, or with NO_OVERWRITE for a write beside data the GPU may still be
 * reading. False for anything else, which UpdateSubresource then handles. */
bool D3dBackend::update_dynamic_buffer(uint32_t id, ID3D11Resource *resource, uint32_t offset, const uint8_t *data,
                                       uint64_t bytes, uint32_t copy_flags)
{
    AV_SCOPE("D3dBackend::update_dynamic_buffer");
    D3D11_RESOURCE_DIMENSION dimension = D3D11_RESOURCE_DIMENSION_UNKNOWN;
    resource->GetType(&dimension);
    if (dimension != D3D11_RESOURCE_DIMENSION_BUFFER) return false;
    D3D11_BUFFER_DESC desc = {};
    static_cast<ID3D11Buffer *>(resource)->GetDesc(&desc);
    if (desc.Usage != D3D11_USAGE_DYNAMIC || offset + bytes > desc.ByteWidth) return false;
    ID3D11Buffer *const buffer = static_cast<ID3D11Buffer *>(resource);
    if ((copy_flags & D3D11_COPY_DISCARD) != 0u && offset == 0u &&
        constants_.update(device11_->device(), device11_->context(), states_.selected_id(), id, buffer, data,
                          static_cast<uint32_t>(bytes)))
        return true;
    constants_.release(device11_->context(), states_.selected_id(), id, buffer);
    const D3D11_MAP map = (copy_flags & D3D11_COPY_DISCARD) != 0u ? D3D11_MAP_WRITE_DISCARD : D3D11_MAP_WRITE_NO_OVERWRITE;
    D3D11_MAPPED_SUBRESOURCE mapped = {};
    ID3D11DeviceContext4 *const context = device11_->context();
    if (FAILED(context->Map(resource, 0u, map, 0u, &mapped)) || mapped.pData == nullptr) return false;
    __builtin_memcpy(static_cast<uint8_t *>(mapped.pData) + offset, data, bytes);
    context->Unmap(resource, 0u);
    return true;
}

/* A host that answers 1 draws a buffer bound to a slice of a slice store in
 * place; 0 leaves the guest copying what it writes, as before. The session
 * asks once, before it has a device of its own, so none need be selected. */
bool D3dBackend::on_QueryBufferSlices11(uint32_t &supported)
{
    AV_SCOPE("D3dBackend::on_QueryBufferSlices11");
    supported = device11_ != nullptr && device11_->slices() ? 1u : 0u;
    return true;
}

/* A buffer bound to a slice is the guest's memory from then on: whatever the
 * constant ring kept of it goes back to its own storage first, so that what
 * the guest bound last is what draws read. Only a dynamic buffer is bound -
 * the only kind the guest places on a slice, and the only kind DXVK can give
 * storage of its own again when the store goes (release_slice_storage). */
bool D3dBackend::on_BindBufferSlice(uint32_t resource, uint64_t memory, uint64_t offset, uint64_t bytes)
{
    AV_SCOPE("D3dBackend::on_BindBufferSlice");
    ID3D11Resource *const     found = objects_.find_as<ID3D11Resource>(resource, D3dObjectKind::resource);
    const SourceMemory *const store = imports_ != nullptr ? imports_->named(memory) : nullptr;
    uint8_t *const            slice = store != nullptr ? store->at(offset, bytes) : nullptr;
    if (!ready() || found == nullptr || slice == nullptr) return false;
    if (!dynamic_buffer(found) || !slice_storage_for(*store)) return false;
    auto *const buffer = static_cast<ID3D11Buffer *>(found);
    constants_.release(device11_->context(), states_.selected_id(), resource, buffer);
    return device11_->bind_storage(buffer, slice_storage_, offset, bytes, slice);
}

bool D3dBackend::dynamic_buffer(ID3D11Resource *resource) const
{
    AV_SCOPE("D3dBackend::dynamic_buffer");
    D3D11_RESOURCE_DIMENSION dimension = D3D11_RESOURCE_DIMENSION_UNKNOWN;
    resource->GetType(&dimension);
    if (dimension != D3D11_RESOURCE_DIMENSION_BUFFER) return false;
    D3D11_BUFFER_DESC desc = {};
    static_cast<ID3D11Buffer *>(resource)->GetDesc(&desc);
    return desc.Usage == D3D11_USAGE_DYNAMIC;
}

/* By the store's dma-buf: its memory is the GPU's own (docs/host-memory.md),
 * which DXVK's device imports as any device of the same driver does. A
 * session has one store: another, while one is held, is refused, since
 * letting the held one go would take storage from buffers still on it. */
bool D3dBackend::slice_storage_for(const SourceMemory &store)
{
    AV_SCOPE("D3dBackend::slice_storage_for");
    if (slice_memory_ == &store) return true;
    if (slice_memory_ != nullptr || slice_storage_refused_ || !device11_->slices()) return false;
    OwnedFd descriptor;
    if (store.share(descriptor) && device11_->import_to_draw(descriptor, store.size(), slice_storage_)) {
        slice_memory_ = &store;
        return true;
    }
    slice_storage_refused_ = true;
    lines_.write("the slice store could not be imported to draw from: mapped buffers are copied");
    return false;
}

/* DXVK renamed each buffer bound to a slice to the store's buffer without
 * owning it: a buffer left on it would draw from a buffer destroyed, and a
 * map of it on the host would write where the store's mapping was. So every
 * dynamic buffer - which buffers are on the store is not kept - is discarded
 * first, which renames it to storage of DXVK's own; the guest lets go of its
 * store only as its session goes, when what they held is moot. DXVK may
 * still have recorded draws that read the store, or the GPU be running them:
 * they finish before the buffer they read goes. */
void D3dBackend::release_slice_storage()
{
    AV_SCOPE("D3dBackend::release_slice_storage");
    if (!slice_storage_.made()) return;
    for (uint32_t slot = objects_.next_used(0u); slot != D3dObjectTable::no_slot; slot = objects_.next_used(slot + 1u)) {
        uint32_t      id = 0u;
        D3dObjectKind kind = D3dObjectKind::none;
        static_cast<void>(objects_.at(slot, id, kind));
        ID3D11Resource *const resource = objects_.find_as<ID3D11Resource>(id, D3dObjectKind::resource);
        if (resource != nullptr && dynamic_buffer(resource)) device11_->context()->DiscardResource(resource);
    }
    device11_->vulkan().idle();
    slice_storage_.release();
    slice_memory_ = nullptr;
}

/* What a read back waits for, without the device's lock: every session on the
 * GPU records into one immediate context, and DXVK's Map(READ) flushes it and
 * waits for the GPU to finish all of it, holding every other session - DWM's
 * composition among them - for as long. An application reading one back each
 * frame held DWM a frame at a time, and the compositor skipped captures. The
 * copy is marked on the session's fence instead, the lock let go while the GPU
 * runs it, and taken again - the session's state with it - to map what is by
 * then finished. A mark the device refuses leaves the Map to wait, as before. */
void D3dBackend::await_gpu_unlocked()
{
    AV_SCOPE("D3dBackend::await_gpu_unlocked");
    constexpr uint64_t pause_nanoseconds = 20000u;
    constexpr uint64_t deadline_nanoseconds = 2000000000u; /* past which the Map waits */
    if (progress11_.fence() == nullptr) progress11_.adopt(device11_->make_fence(progress11_.marked()));
    if (!device11_->signal(progress11_.fence(), progress11_.next())) return;
    progress11_.signalled();
    published_.flushed();
    device11_->unlock();
    PollBudget polls(clock_, 0u, pause_nanoseconds, deadline_nanoseconds);
    while (polls.next())
        if (!progress11_.pending() || device11_->lost()) break;
    device11_->lock(states_.selected());
}

/* A frame the host cannot stage - a resource never shared, not a 2D texture
 * of four-byte pixels, no frame free - is a present the engine refuses: the
 * session goes on, and the application sees at most a frame not shown. A
 * blt-model buffer's every present stages: Direct3D 11 knows its model. */
bool D3dBackend::on_PresentPoint(uint32_t resource, uint32_t subresource, uint32_t point)
{
    AV_SCOPE("D3dBackend::on_PresentPoint");
    ID3D11Resource *const found = objects_.find_as<ID3D11Resource>(resource, D3dObjectKind::resource);
    if (!ready() || found == nullptr) return false;
    const SurfacePoint frame(SurfaceIdentity(found).surface(), point);
    D3D11_RESOURCE_DIMENSION dimension = D3D11_RESOURCE_DIMENSION_UNKNOWN;
    found->GetType(&dimension);
    /* A flip names no frame: its present is the copy its compositor reads. */
    if (point == 0u) {
        if (dimension == D3D11_RESOURCE_DIMENSION_TEXTURE2D && frame.surface() != 0u &&
            !device11_->show(shares_->domain(), frame.surface(), static_cast<ID3D11Texture2D *>(found)) &&
            !show_refused_) {
            LogLine(lines_).text("a flip of surface ").hex(frame.surface(), 16u).text(" has no copy to show: DWM reads the buffer itself").write();
            show_refused_ = true;
        }
        return true;
    }
    if (surfaces_ == nullptr) return true;
    uint32_t   width = 0u, height = 0u, depth = 0u, format = 0u;
    const bool texture = dimension == D3D11_RESOURCE_DIMENSION_TEXTURE2D &&
                         extent_of(found, subresource, width, height, depth, format);
    PresentedTexture presented(*device11_, texture ? static_cast<ID3D11Texture2D *>(found) : nullptr, subresource,
                               width, height, format);
    surfaces_->stage(frame, SharedSurfaces::Staging::every_present, presented);
    return true;
}

bool D3dBackend::on_ReadSubresource(uint32_t resource, uint32_t subresource, uint32_t &hresult, uint32_t &row_pitch,
                                    uint32_t &depth_pitch, uint8_t *data, uint64_t data_capacity, uint64_t &data_size)
{
    AV_SCOPE("D3dBackend::on_ReadSubresource");
    hresult = static_cast<uint32_t>(E_FAIL);
    row_pitch = 0u;
    depth_pitch = 0u;
    data_size = 0u;
    ID3D11Resource *const found = objects_.find_as<ID3D11Resource>(resource, D3dObjectKind::resource);
    if (!ready() || found == nullptr) return false;
    constants_.release(device11_->context(), states_.selected_id(), resource, static_cast<ID3D11Buffer *>(found));
    uint32_t width = 0u, height = 0u, depth = 0u, format = 0u;
    if (!extent_of(found, subresource, width, height, depth, format)) return false;
    ID3D11Resource *const staging = staging_for(found, subresource, width, height, depth, format);
    if (staging == nullptr) return true;
    device11_->context()->CopySubresourceRegion(staging, 0u, 0u, 0u, 0u, found, subresource, nullptr);
    await_gpu_unlocked();
    /* Nothing is read back from a lost device: the guest's Map answers
     * removed, and the session is faulted after this command. */
    if (device11_->lost()) {
        staging->Release();
        hresult = static_cast<uint32_t>(DXGI_ERROR_DEVICE_REMOVED);
        return true;
    }
    ID3D11DeviceContext4 *const context = device11_->context();
    D3D11_MAPPED_SUBRESOURCE mapped = {};
    const HRESULT result = context->Map(staging, 0u, D3D11_MAP_READ, 0u, &mapped);
    if (FAILED(result)) {
        staging->Release();
        hresult = static_cast<uint32_t>(result);
        return true;
    }
    const DxgiFormatLayout layout(format);
    const uint32_t row = layout.copied_row_bytes(width);
    const uint32_t rows = layout.rows(height);
    const uint64_t total = static_cast<uint64_t>(row) * rows * depth;
    if (total <= data_capacity) {
        for (uint32_t slice = 0u; slice < depth; slice++)
            for (uint32_t line = 0u; line < rows; line++)
                __builtin_memcpy(data + (static_cast<uint64_t>(slice) * rows + line) * row,
                                 static_cast<const uint8_t *>(mapped.pData) +
                                     static_cast<uint64_t>(slice) * mapped.DepthPitch +
                                     static_cast<uint64_t>(line) * mapped.RowPitch,
                                 row);
        data_size = total;
        hresult = static_cast<uint32_t>(S_OK);
    } else {
        hresult = static_cast<uint32_t>(E_OUTOFMEMORY);
    }
    context->Unmap(staging, 0u);
    staging->Release();
    row_pitch = row;
    depth_pitch = row * rows;
    return true;
}

/* ---------------------------------------------------------------- queries */

bool D3dBackend::on_BeginQuery(uint32_t query)
{
    AV_SCOPE("D3dBackend::on_BeginQuery");
    ID3D11Query *const found = objects_.find_as<ID3D11Query>(query, D3dObjectKind::query);
    if (!ready() || found == nullptr) return false;
    device11_->context()->Begin(found);
    return true;
}

bool D3dBackend::on_EndQuery(uint32_t query)
{
    AV_SCOPE("D3dBackend::on_EndQuery");
    ID3D11Query *const found = objects_.find_as<ID3D11Query>(query, D3dObjectKind::query);
    const bool         usable = ready() && found != nullptr;
    if (usable) device11_->context()->End(found);
    /* Counted whether or not it ran: the guest counted it when it sent it. */
    if (published_.ended(query, usable ? found : nullptr) != Status::ok) return false;
    return usable;
}

bool D3dBackend::on_BindQueryResults(uint64_t offset, uint32_t slots)
{
    AV_SCOPE("D3dBackend::on_BindQueryResults");
    constexpr uint32_t most_slots = 4096u;
    if (slots > most_slots) return false;
    return published_.bind(offset, slots);
}

bool D3dBackend::on_GetQueryData(uint32_t query, uint32_t flags, uint32_t &ready_out, uint8_t *data,
                                 uint64_t data_capacity, uint64_t &data_size)
{
    AV_SCOPE("D3dBackend::on_GetQueryData");
    ready_out = 0u;
    data_size = 0u;
    ID3D11Query *const found = objects_.find_as<ID3D11Query>(query, D3dObjectKind::query);
    if (!ready() || found == nullptr) return false;
    const UINT size = found->GetDataSize();
    if (size > data_capacity) return false;
    /* The guest's thread is stopped on this answer, and each "not yet" costs
     * it a round trip through the kernel and back: a flushing poll waits here
     * for a result that is a millisecond away, and still answers "not yet"
     * after that, as GetData must. */
    constexpr uint64_t patience_ns = 1000000u;
    const UINT         donotflush = flags & D3D11_ASYNC_GETDATA_DONOTFLUSH;
    PollBudget         polls(clock_, 0u, 0u, patience_ns);
    UINT               asked = donotflush;
    HRESULT            result = S_FALSE;
    while (polls.next()) {
        result = device11_->context()->GetData(found, size == 0u ? nullptr : data, size, asked);
        if (result != S_FALSE || donotflush != 0u) break;
        asked = D3D11_ASYNC_GETDATA_DONOTFLUSH;
    }
    if (result == S_OK) {
        ready_out = 1u;
        data_size = size;
    }
    return true;
}

bool D3dBackend::invariants_hold() const
{
    AV_SCOPE("D3dBackend::invariants_hold");
    if (!objects_.invariants_hold()) return false;
    if (!states_.invariants_hold()) return false;
    if (device11_ == nullptr) return states_.count() == 0u && objects_.count() == 0u;
    return true;
}

}  // namespace av
