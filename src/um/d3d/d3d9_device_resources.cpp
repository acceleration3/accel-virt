/* D3d9Device's resources, their locks, and the blts between them
 * (d3d9_device.hpp).
 *
 * A host resource is written, never mapped: a lock hands out a guest shadow
 * of the subresource, and its unlock sends what the lock covered. Discard
 * then needs no renaming - the data crosses in the command stream, and the
 * host's own discard keeps the GPU from waiting - and no-overwrite no wait.
 * The runtime keeps the managed pool itself and fills it with TexBlt and
 * BufBlt from system memory of its own, which is copied from where it lies. */
#include "core/platform/block_source.hpp"
#include "core/log/log_line.hpp"
#include "core/support/text_buffer.hpp"
#include "core/trace/function_scope.hpp"
#include "proto/client/transfer_room.hpp"
#include "core/log/keyed_log.hpp"
#include "icd/d3d/d3d_command_stream.hpp"
#include "icd/common/pieces.hpp"
#include "icd/d3d/d3d_device_channel.hpp"
#include "icd/d3d/texture_upload.hpp"
#include "proto/resources/pitched_rows.hpp"
#include "proto/d3d/d3d9_format_layout.hpp"
#include "proto/d3d/d3d9_present_format.hpp"
#include "proto/resources/allocation_descriptor.hpp"
#include "proto/generated/d3d9/d3d9_dispatch_client.h"
#include "um/d3d/d3d9_device.hpp"

#include <string.h>

namespace av::d3d9 {

namespace {

/* CreateSurface9's and CreateTexture9's kinds (d3d9.toml). */
/* What a relocked resource's line is logged once under. */
constexpr char relocked_key[] = "relocked";
constexpr uint32_t surface_render_target = 1u;
constexpr uint32_t surface_depth_stencil = 2u;
constexpr uint32_t surface_offscreen_plain = 3u;
constexpr uint32_t surface_lockable = 0x1u;
constexpr uint32_t texture_2d = 1u;
constexpr uint32_t texture_cube = 2u;
constexpr uint32_t texture_volume = 3u;
constexpr uint32_t cube_faces = 6u;
/* D3DPOOL_DEFAULT: the host places what the guest makes as it can draw it. */
constexpr uint32_t pool_default = 0u;
/* D3DUSAGE_*, from d3d9types.h's values. */
constexpr uint32_t usage_render_target = 0x1u;
constexpr uint32_t usage_depth_stencil = 0x2u;
constexpr uint32_t usage_write_only = 0x8u;
constexpr uint32_t usage_points = 0x40u;
constexpr uint32_t usage_rt_patches = 0x80u;
constexpr uint32_t usage_n_patches = 0x100u;
constexpr uint32_t usage_dynamic = 0x200u;
constexpr uint32_t usage_autogen_mipmap = 0x400u;
constexpr uint32_t usage_dmap = 0x4000u;
/* D3DLOCK_DISCARD and D3DLOCK_NOOVERWRITE, which a write carries. */
constexpr uint32_t write_discard = 0x2000u;
constexpr uint32_t write_no_overwrite = 0x1000u;
/* D3DTEXF_POINT and D3DTEXF_LINEAR. */
constexpr uint32_t filter_point = 1u;
constexpr uint32_t filter_linear = 2u;

}  // namespace

/* ---------------------------------------------------------------- creation */

HRESULT D3d9Device::create_resource(D3DDDIARG_CREATERESOURCE *args)
{
    AV_SCOPE("D3d9Device::create_resource");
    if (args->SurfCount == 0u || args->pSurfList == nullptr) return E_INVALIDARG;
    D3d9Resource *made = nullptr;
    if (resources_.create(made) != Status::ok) return E_OUTOFMEMORY;
    *made = D3d9Resource{};
    made->subresources =
        static_cast<D3d9Subresource *>(memory_.allocate(sizeof(D3d9Subresource) * args->SurfCount, Pool::paged));
    if (made->subresources == nullptr) {
        static_cast<void>(resources_.destroy(made));
        return E_OUTOFMEMORY;
    }
    memset(made->subresources, 0, sizeof(D3d9Subresource) * args->SurfCount);
    made->format = args->Format;
    made->pool = args->Pool;
    made->flags = args->Flags;
    made->count = args->SurfCount;
    made->runtime = args->hResource;
    for (UINT index = 0u; index < args->SurfCount; index++) {
        const D3DDDI_SURFACEINFO &surface = args->pSurfList[index];
        made->subresources[index] = D3d9Subresource{};
        made->subresources[index].width = surface.Width;
        made->subresources[index].height = surface.Height == 0u ? 1u : surface.Height;
        made->subresources[index].depth = surface.Depth == 0u ? 1u : surface.Depth;
        made->subresources[index].system = static_cast<const uint8_t *>(surface.pSysMem);
        made->subresources[index].system_pitch = surface.SysMemPitch;
        made->subresources[index].system_slice_pitch = surface.SysMemSlicePitch;
    }
    if (args->Flags.VertexBuffer || args->Flags.IndexBuffer)
        trace("buffer", args->Flags.Value, static_cast<uint32_t>(args->Pool), made->subresources[0].width,
              made->subresources[0].system != nullptr ? 1u : 0u);
    if (args->Flags.RenderTarget || args->Flags.ZBuffer || args->Flags.Primary)
        trace("resource", args->Flags.Value, static_cast<uint32_t>(args->Format), made->subresources[0].width,
              made->subresources[0].height);
    HRESULT host = args->Pool == D3DDDIPOOL_SYSTEMMEM ? S_OK : create_host_object(*made, *args);
    if (SUCCEEDED(host) && args->Flags.Primary) host = allocate_primary(*made, *args);
    if (FAILED(host)) {
        release_resource(*made);
        static_cast<void>(resources_.destroy(made));
        return host;
    }
    args->hResource = made;
    return S_OK;
}

/* What the runtime's resource flags say of the API's usage
 * (reverse-engineering.md §4.2 maps one to the other). */
HRESULT D3d9Device::create_host_object(D3d9Resource &resource, const D3DDDIARG_CREATERESOURCE &create)
{
    AV_SCOPE("D3d9Device::create_host_object");
    const D3DDDI_RESOURCEFLAGS flags = create.Flags;
    const uint32_t usage = (flags.RenderTarget ? usage_render_target : 0u) |
                           (flags.ZBuffer ? usage_depth_stencil : 0u) | (flags.Dynamic ? usage_dynamic : 0u) |
                           (flags.AutogenMipmap ? usage_autogen_mipmap : 0u) |
                           (flags.WriteOnly ? usage_write_only : 0u) | (flags.Points ? usage_points : 0u) |
                           (flags.RtPatches ? usage_rt_patches : 0u) | (flags.NPatches ? usage_n_patches : 0u) |
                           (flags.DMap ? usage_dmap : 0u);
    const D3d9Subresource &top = resource.subresources[0];
    resource.id = next_id();
    bool sent = false;
    if (flags.VertexBuffer) {
        resource.kind = D3d9Resource::Kind::vertex_buffer;
        sent = d3d9proto::av_d3d9_CreateVertexBuffer9(wire(), resource.id, top.width, usage, create.Fvf, pool_default);
    } else if (flags.IndexBuffer) {
        resource.kind = D3d9Resource::Kind::index_buffer;
        sent = d3d9proto::av_d3d9_CreateIndexBuffer9(wire(), resource.id, top.width, usage,
                                                     static_cast<uint32_t>(create.Format), pool_default);
    } else if (flags.Texture || flags.CubeMap || flags.Volume) {
        const uint32_t faces = flags.CubeMap ? cube_faces : 1u;
        resource.kind = flags.CubeMap  ? D3d9Resource::Kind::cube
                        : flags.Volume ? D3d9Resource::Kind::volume
                                       : D3d9Resource::Kind::texture;
        resource.levels = create.SurfCount / faces;
        const uint32_t type = flags.CubeMap ? texture_cube : flags.Volume ? texture_volume : texture_2d;
        /* An autogen texture shows the runtime its top level only, and the
         * host makes the chain below it. */
        const uint32_t levels = flags.AutogenMipmap ? 0u : resource.levels;
        sent = d3d9proto::av_d3d9_CreateTexture9(wire(), resource.id, type, top.width, top.height, top.depth, levels,
                                                 usage, static_cast<uint32_t>(create.Format), pool_default);
    } else {
        resource.kind = D3d9Resource::Kind::surface;
        resource.id = 0u;
        return create_surfaces(resource, create);
    }
    return sent ? S_OK : lost(__func__);
}

/* A surface resource is as many surfaces as it has subresources: a swap
 * chain's buffers, or one render target or depth surface. */
HRESULT D3d9Device::create_surfaces(D3d9Resource &resource, const D3DDDIARG_CREATERESOURCE &create)
{
    AV_SCOPE("D3d9Device::create_surfaces");
    const uint32_t kind = create.Flags.ZBuffer        ? surface_depth_stencil
                          : create.Flags.RenderTarget ? surface_render_target
                                                      : surface_offscreen_plain;
    const uint32_t flags = kind == surface_render_target && !create.Flags.NotLockable ? surface_lockable : 0u;
    for (uint32_t index = 0u; index < resource.count; index++) {
        D3d9Subresource &surface = resource.subresources[index];
        surface.id = next_id();
        if (!d3d9proto::av_d3d9_CreateSurface9(wire(), surface.id, kind, surface.width, surface.height,
                                               static_cast<uint32_t>(create.Format),
                                               static_cast<uint32_t>(create.MultisampleType),
                                               create.MultisampleQuality, flags))
            return lost(__func__);
    }
    return S_OK;
}

HRESULT D3d9Device::destroy_resource(HANDLE handle)
{
    AV_SCOPE("D3d9Device::destroy_resource");
    D3d9Resource *const gone = resource(handle);
    if (gone == nullptr) return E_INVALIDARG;
    if (gone->flags.RenderTarget || gone->flags.ZBuffer)
        trace("destroy", gone->flags.Value, static_cast<uint32_t>(gone->format), gone->subresources[0].width,
              gone->subresources[0].height);
    for (uint32_t stream = 0u; stream < streams; stream++)
        if (streams_[stream].buffer == gone) streams_[stream] = Stream{};
    if (indices_ == gone) indices_ = nullptr;
    release_resource(*gone);
    static_cast<void>(resources_.destroy(gone));
    return S_OK;
}

/* The host objects, shadows and kernel allocation go; the record stays for
 * the caller to destroy. */
void D3d9Device::release_resource(D3d9Resource &resource)
{
    AV_SCOPE("D3d9Device::release_resource");
    if (resource.id != 0u) static_cast<void>(d3d9proto::av_d3d9_Destroy9(wire(), resource.id));
    for (uint32_t index = 0u; resource.subresources != nullptr && index < resource.count; index++) {
        D3d9Subresource &subresource = resource.subresources[index];
        if (subresource.id != 0u) static_cast<void>(d3d9proto::av_d3d9_Destroy9(wire(), subresource.id));
        if (subresource.shadow != nullptr) memory_.release(subresource.shadow);
    }
    if (resource.subresources != nullptr) memory_.release(resource.subresources);
    resource.subresources = nullptr;
    if (!resource.opened) allocations_.give_back(resource.allocation, resource.allocation_resource);
}

/* The redirection surface of a window, which d3d9.dll opens into the device
 * before it presents into it (reverse-engineering.md §4.3). Another process's
 * resource would open through the host's object, which nothing here shares. */
HRESULT D3d9Device::open_resource(D3DDDIARG_OPENRESOURCE *args)
{
    AV_SCOPE("D3d9Device::open_resource");
    if (args->NumAllocations != 1u || args->pOpenAllocationInfo == nullptr) return refuse_open(*args, 0u);
    const D3DDDI_OPENALLOCATIONINFO &info = args->pOpenAllocationInfo[0];
    AllocationDescriptor             descriptor;
    if (info.pPrivateDriverData == nullptr ||
        descriptor.decode(static_cast<const uint8_t *>(info.pPrivateDriverData), info.PrivateDriverDataSize) !=
            Status::ok)
        return refuse_open(*args, 0u);
    const SurfaceLayout &layout = descriptor.layout();
    D3d9PresentFormat    presented(0u);
    if (descriptor.kind() != AllocationKind::gdi || layout.width() == 0u || layout.height() == 0u ||
        !presented.take_ddi(layout.format()))
        return refuse_open(*args, static_cast<uint32_t>(descriptor.kind()));
    D3d9Resource *made = nullptr;
    if (resources_.create(made) != Status::ok) return E_OUTOFMEMORY;
    *made = D3d9Resource{};
    made->subresources = static_cast<D3d9Subresource *>(memory_.allocate(sizeof(D3d9Subresource), Pool::paged));
    if (made->subresources == nullptr) {
        static_cast<void>(resources_.destroy(made));
        return E_OUTOFMEMORY;
    }
    memset(made->subresources, 0, sizeof(D3d9Subresource));
    made->subresources[0].width = layout.width();
    made->subresources[0].height = layout.height();
    made->subresources[0].depth = 1u;
    made->kind = D3d9Resource::Kind::surface;
    made->format = static_cast<D3DDDIFORMAT>(layout.format());
    made->count = 1u;
    made->runtime = args->hResource;
    made->allocation = info.hAllocation;
    made->allocation_pitch = layout.pitch();
    made->opened = true;
    /* A present names it as its destination in the blt dxgkrnl builds, and
     * dxgkrnl puts the device in error for an allocation this device never
     * asked to be resident (PagingQueue). */
    if (FAILED(allocations_.keep_resident(made->allocation))) {
        release_resource(*made);
        static_cast<void>(resources_.destroy(made));
        return E_FAIL;
    }
    trace("opened", layout.width(), layout.height(), layout.format(), layout.pitch());
    args->hResource = made;
    return S_OK;
}

HRESULT D3d9Device::refuse_open(const D3DDDIARG_OPENRESOURCE &args, uint32_t kind)
{
    AV_SCOPE("D3d9Device::refuse_open");
    if (lines_.first("a Direct3D 9 OpenResource it refused", kind))
        LogLine(lines_)
            .text("a Direct3D 9 OpenResource it cannot serve: allocation kind ")
            .number(kind)
            .text(" allocations ")
            .number(args.NumAllocations)
            .text(" resource data ")
            .hex(args.PrivateDriverDataSize, 4u)
            .write();
    return E_INVALIDARG;
}

bool D3d9Device::refuses_opened(const D3d9Resource &resource, const char *call)
{
    AV_SCOPE("D3d9Device::refuses_opened");
    if (!resource.opened) return false;
    not_implemented(call);
    return true;
}

bool D3d9Device::surface_of(const D3d9Resource &resource, uint32_t index, uint32_t &id, uint32_t &subresource) const
{
    AV_SCOPE("D3d9Device::surface_of");
    if (index >= resource.count) return false;
    switch (resource.kind) {
    case D3d9Resource::Kind::surface:
        id = resource.subresources[index].id;
        subresource = 0u;
        return true;
    case D3d9Resource::Kind::texture:
    case D3d9Resource::Kind::cube:
        id = resource.id;
        subresource = index;
        return true;
    default: return false;
    }
}

/* ---------------------------------------------------------------- locks */

uint32_t D3d9Device::row_pitch(const D3d9Resource &resource, const D3d9Subresource &subresource) const
{
    AV_SCOPE("D3d9Device::row_pitch");
    if (resource.kind == D3d9Resource::Kind::vertex_buffer || resource.kind == D3d9Resource::Kind::index_buffer)
        return subresource.width;
    return D3d9FormatLayout(static_cast<uint32_t>(resource.format)).row_bytes(subresource.width);
}

size_t D3d9Device::subresource_bytes(const D3d9Resource &resource, const D3d9Subresource &subresource) const
{
    AV_SCOPE("D3d9Device::subresource_bytes");
    if (resource.kind == D3d9Resource::Kind::vertex_buffer || resource.kind == D3d9Resource::Kind::index_buffer)
        return subresource.width;
    const uint32_t rows = D3d9FormatLayout(static_cast<uint32_t>(resource.format)).rows(subresource.height);
    return size_t{row_pitch(resource, subresource)} * rows * subresource.depth;
}

size_t D3d9Device::lock_offset(const D3d9Resource &resource, const D3DDDIARG_LOCK &lock, uint32_t pitch,
                               uint32_t slice_pitch) const
{
    AV_SCOPE("D3d9Device::lock_offset");
    if (lock.Flags.RangeValid) return lock.Range.Offset;
    const D3d9FormatLayout layout(static_cast<uint32_t>(resource.format));
    if (lock.Flags.BoxValid)
        return size_t{lock.Box.Front} * slice_pitch +
               static_cast<size_t>(layout.offset_of(lock.Box.Left, lock.Box.Top, pitch));
    if (lock.Flags.AreaValid)
        return static_cast<size_t>(layout.offset_of(static_cast<uint32_t>(lock.Area.left),
                                                    static_cast<uint32_t>(lock.Area.top), pitch));
    return 0u;
}

bool D3d9Device::make_shadow(const D3d9Resource &resource, D3d9Subresource &subresource)
{
    AV_SCOPE("D3d9Device::make_shadow");
    if (subresource.shadow != nullptr) return true;
    const size_t bytes = subresource_bytes(resource, subresource);
    if (bytes == 0u) return false;
    subresource.shadow = static_cast<uint8_t *>(memory_.allocate(bytes, Pool::paged));
    if (subresource.shadow == nullptr) return false;
    memset(subresource.shadow, 0, bytes);
    subresource.shadow_bytes = bytes;
    subresource.shadow_pitch = row_pitch(resource, subresource);
    const uint32_t rows = resource.kind == D3d9Resource::Kind::vertex_buffer ||
                                  resource.kind == D3d9Resource::Kind::index_buffer
                              ? 1u
                              : D3d9FormatLayout(static_cast<uint32_t>(resource.format)).rows(subresource.height);
    subresource.shadow_slice_pitch = subresource.shadow_pitch * rows;
    return true;
}

/* System memory is the runtime's own and handed back where it lies. A host
 * subresource hands out its shadow, which for a surface the host draws into
 * is read back first unless the lock throws it away. */
HRESULT D3d9Device::lock(D3DDDIARG_LOCK *args)
{
    AV_SCOPE("D3d9Device::lock");
    D3d9Resource *const locked = resource(args->hResource);
    if (locked == nullptr || args->SubResourceIndex >= locked->count) return E_INVALIDARG;
    if (args->Flags.NotifyOnly) return S_OK;
    if (refuses_opened(*locked, "a Direct3D 9 lock of an opened surface")) return E_NOTIMPL;
    D3d9Subresource &subresource = locked->subresources[args->SubResourceIndex];
    if (locked->kind == D3d9Resource::Kind::system) {
        auto *const base = const_cast<uint8_t *>(subresource.system);
        args->pSurfData = base + lock_offset(*locked, *args, subresource.system_pitch, subresource.system_slice_pitch);
        args->Pitch = subresource.system_pitch;
        args->SlicePitch = subresource.system_slice_pitch;
        return S_OK;
    }
    if (!make_shadow(*locked, subresource)) return E_OUTOFMEMORY;
    if (subresource.locked && !subresource.lock_flags.ReadOnly) {
        char       said[160] = {};
        TextBuffer text(said, sizeof(said));
        text.append("a Direct3D 9 resource locked again before its unlock, kind ");
        text.append_decimal(static_cast<uint32_t>(locked->kind));
        text.append(" flags 0x");
        text.append_hex(args->Flags.Value, 4u);
        text.append(" after 0x");
        text.append_hex(subresource.lock_flags.Value, 4u);
        if (lines_.first(relocked_key, 0)) lines_.write(said);
        static_cast<void>(unlock_subresource(*locked, subresource, args->SubResourceIndex));
    }
    if (locked->kind == D3d9Resource::Kind::surface && !args->Flags.Discard && !args->Flags.WriteOnly) {
        uint32_t      pitch = 0u, rows = 0u;
        const HRESULT read = read_surface(*locked, args->SubResourceIndex, pitch, rows);
        /* A device the host lost hands out no bytes: the application is told
         * the device is gone rather than shown what the shadow held before. */
        if (loss_.lost()) return read;
        if (SUCCEEDED(read))
            static_cast<void>(PitchedRows(subresource.shadow, subresource.shadow_pitch, subresource.shadow_bytes)
                                  .copy_from(PitchedRows(readback_, pitch, size_t{rows} * pitch), rows));
    }
    args->pSurfData =
        subresource.shadow + lock_offset(*locked, *args, subresource.shadow_pitch, subresource.shadow_slice_pitch);
    args->Pitch = subresource.shadow_pitch;
    args->SlicePitch = subresource.shadow_slice_pitch;
    subresource.locked = true;
    subresource.lock_flags = args->Flags;
    subresource.lock = *args;
    return S_OK;
}

HRESULT D3d9Device::unlock(const D3DDDIARG_UNLOCK *args)
{
    AV_SCOPE("D3d9Device::unlock");
    D3d9Resource *const locked = resource(args->hResource);
    if (locked == nullptr || args->SubResourceIndex >= locked->count) return E_INVALIDARG;
    D3d9Subresource &subresource = locked->subresources[args->SubResourceIndex];
    if (locked->kind == D3d9Resource::Kind::system || !subresource.locked) return S_OK;
    subresource.locked = false;
    if (subresource.lock_flags.ReadOnly) return S_OK;
    return unlock_subresource(*locked, subresource, args->SubResourceIndex);
}

/* What the lock covered crosses to the host, with the lock's discard or
 * no-overwrite. */
HRESULT D3d9Device::unlock_subresource(const D3d9Resource &resource, D3d9Subresource &subresource, uint32_t index)
{
    AV_SCOPE("D3d9Device::unlock_subresource");
    const D3DDDIARG_LOCK &lock = subresource.lock;
    const uint32_t flags = (lock.Flags.Discard ? write_discard : 0u) | (lock.Flags.NoOverwrite ? write_no_overwrite : 0u);
    const size_t offset = lock_offset(resource, lock, subresource.shadow_pitch, subresource.shadow_slice_pitch);
    if (resource.kind == D3d9Resource::Kind::vertex_buffer || resource.kind == D3d9Resource::Kind::index_buffer) {
        const uint32_t start = lock.Flags.RangeValid ? lock.Range.Offset : 0u;
        const uint32_t bytes = lock.Flags.RangeValid && lock.Range.Size != 0u ? lock.Range.Size : subresource.width - start;
        if (start >= subresource.width) return E_INVALIDARG;
        const uint32_t fit = bytes < subresource.width - start ? bytes : subresource.width - start;
        return write_buffer(resource.id, start, subresource.shadow + start, fit, flags);
    }
    D3DDDIBOX box = {0u, 0u, subresource.width, subresource.height, 0u, subresource.depth};
    if (lock.Flags.BoxValid) box = lock.Box;
    if (lock.Flags.AreaValid)
        box = D3DDDIBOX{static_cast<UINT>(lock.Area.left), static_cast<UINT>(lock.Area.top),
                        static_cast<UINT>(lock.Area.right), static_cast<UINT>(lock.Area.bottom), 0u, 1u};
    return write_texture(resource, index, box, subresource.shadow + offset, subresource.shadow_pitch,
                         subresource.shadow_slice_pitch, flags);
}

/* ---------------------------------------------------------------- writes and reads */

HRESULT D3d9Device::write_buffer(uint32_t id, uint32_t offset, const void *data, uint32_t bytes, uint32_t flags)
{
    AV_SCOPE("D3d9Device::write_buffer");
    const auto *const from = static_cast<const uint8_t *>(data);
    written_ += bytes;
    icd::Pieces pieces(bytes, TransferRoom::lent_bytes);
    while (pieces.next()) {
        const uint32_t done = static_cast<uint32_t>(pieces.offset());
        const uint32_t piece = static_cast<uint32_t>(pieces.count());
        if (!wire().make_room_for_blobs(piece)) return lost(__func__);
        if (!d3d9proto::av_d3d9_WriteBuffer9(wire(), id, offset + done, pieces.first() ? flags : flags & ~write_discard,
                                             from + done, piece))
            return lost(__func__);
    }
    return S_OK;
}

D3d9Device::TextureWriter::TextureWriter(D3d9Device &device, uint32_t id, uint32_t index, uint32_t flags)
    : device_(device), id_(id), index_(index), flags_(flags)
{
    AV_SCOPE("D3d9Device::TextureWriter::TextureWriter");
}

bool D3d9Device::TextureWriter::piece(const d3dproto::Box &box, const uint8_t *data, uint64_t bytes,
                                      uint32_t row_pitch, uint32_t slice_pitch, bool first)
{
    AV_SCOPE("D3d9Device::TextureWriter::piece");
    const d3d9proto::Box part = {box.left, box.top, box.front, box.right, box.bottom, box.back};
    return device_.write_piece(id_, index_, part, data, static_cast<size_t>(bytes), row_pitch, slice_pitch,
                               first ? flags_ : flags_ & ~write_discard);
}

/* A Direct3D 9 box with Back on Front is a 2D one: one slice. A surface
 * resource's subresource is a host surface of its own; a texture's is its
 * subresource of the one host texture. */
HRESULT D3d9Device::write_texture(const D3d9Resource &resource, uint32_t index, const D3DDDIBOX &box,
                                  const uint8_t *data, uint32_t row_pitch, uint32_t slice_pitch, uint32_t flags)
{
    AV_SCOPE("D3d9Device::write_texture");
    if (box.Right <= box.Left || box.Bottom <= box.Top) return S_OK;
    uint32_t id = resource.id, subresource = index;
    if (resource.kind != D3d9Resource::Kind::volume && !surface_of(resource, index, id, subresource))
        return E_INVALIDARG;
    const uint32_t           back = box.Back > box.Front ? box.Back : box.Front + 1u;
    const d3dproto::Box      region = {box.Left, box.Top, box.Front, box.Right, box.Bottom, back};
    const d3d::TextureUpload upload(D3d9FormatLayout(static_cast<uint32_t>(resource.format)), region, row_pitch,
                                    slice_pitch, TransferRoom::lent_bytes);
    if (!upload.readable()) return E_INVALIDARG;
    TextureWriter writer(*this, id, subresource, flags);
    return upload.send(writer, data) ? S_OK : lost(__func__);
}

bool D3d9Device::write_piece(uint32_t id, uint32_t index, const d3d9proto::Box &box, const uint8_t *data,
                             size_t bytes, uint32_t row_pitch, uint32_t slice_pitch, uint32_t flags)
{
    AV_SCOPE("D3d9Device::write_piece");
    written_ += bytes;
    const uint64_t started = clock_.microseconds();
    const bool     room = wire().make_room_for_blobs(bytes);
    trace_wait("wait-room", started, static_cast<uint32_t>(bytes));
    if (room && d3d9proto::av_d3d9_WriteTexture9(wire(), id, index, box, row_pitch, slice_pitch, flags, data, bytes))
        return true;
    LogLine(lines_)
        .text("a texture write of ")
        .number(bytes)
        .text(" bytes, pitch ")
        .number(row_pitch)
        .text(room ? " was refused by the encoder" : " found no room in the arena")
        .write();
    return false;
}

bool D3d9Device::readback_room(size_t bytes)
{
    AV_SCOPE("D3d9Device::readback_room");
    if (bytes <= readback_bytes_) return true;
    if (readback_ != nullptr) memory_.release(readback_);
    readback_bytes_ = 0u;
    readback_ = static_cast<uint8_t *>(memory_.allocate(bytes, Pool::paged));
    if (readback_ == nullptr) return false;
    readback_bytes_ = bytes;
    return true;
}

/* A round trip: the host has run everything before it once it answers. */
HRESULT D3d9Device::read_surface(const D3d9Resource &resource, uint32_t index, uint32_t &pitch, uint32_t &rows)
{
    AV_SCOPE("D3d9Device::read_surface");
    uint32_t id = 0u, subresource = 0u;
    if (!surface_of(resource, index, id, subresource)) return E_INVALIDARG;
    const D3d9Subresource &level = resource.subresources[index];
    const D3d9FormatLayout layout(static_cast<uint32_t>(resource.format));
    /* The host's pitch may be wider than packed rows: room for a 256-byte
     * aligned row. */
    constexpr uint32_t pitch_slack = 256u;
    const size_t capacity = (size_t{layout.row_bytes(level.width)} + pitch_slack) * layout.rows(level.height);
    if (capacity == 0u || !readback_room(capacity)) return E_OUTOFMEMORY;
    if (!wire().make_room_for_blobs(capacity)) return lost(__func__);
    uint32_t hresult = 1u, width = 0u, height = 0u, format = 0u;
    uint64_t read = 0u;
    const uint64_t started = clock_.microseconds();
    const bool     sent = d3d9proto::av_d3d9_ReadSurface9(wire(), id, subresource, &hresult, &width, &height, &format, &pitch,
                                         readback_, capacity, &read);
    trace_wait("wait-read", started, static_cast<uint32_t>(capacity));
    if (!sent) return lost(__func__);
    if (hresult != 0u) return static_cast<HRESULT>(hresult);
    rows = pitch == 0u ? 0u : static_cast<uint32_t>(read / pitch);
    return S_OK;
}

/* ---------------------------------------------------------------- blts */

/* The runtime fills the managed pool level by level from its system-memory
 * copy: the whole chain of one face, from the source level that is the
 * destination's top level (a source may have more levels), each level's
 * rectangle and point halved from the one above. */
HRESULT D3d9Device::tex_blt(const D3DDDIARG_TEXBLT *args)
{
    AV_SCOPE("D3d9Device::tex_blt");
    const D3d9Resource *const destination = resource(args->hDstResource);
    const D3d9Resource *const source = resource(args->hSrcResource);
    if (destination == nullptr || source == nullptr || source->kind != D3d9Resource::Kind::system ||
        destination->kind == D3d9Resource::Kind::system || destination->levels == 0u)
        return E_INVALIDARG;
    if (refuses_opened(*destination, "a Direct3D 9 TexBlt into an opened surface")) return E_NOTIMPL;
    const uint32_t faces = destination->kind == D3d9Resource::Kind::cube ? cube_faces : 1u;
    const uint32_t source_levels = source->count / faces;
    const uint32_t face = faces == 1u ? 0u : static_cast<uint32_t>(args->CubeMapFace);
    if (face >= faces || source_levels == 0u) return E_INVALIDARG;
    uint32_t first = 0u;
    while (first < source_levels && source->subresources[face * source_levels + first].width >
                                        destination->subresources[face * destination->levels].width)
        first++;
    for (uint32_t level = 0u; level < destination->levels && first + level < source_levels; level++) {
        const RECT  area = {args->SrcRect.left >> level, args->SrcRect.top >> level,
                            (args->SrcRect.right + (1 << level) - 1) >> level,
                            (args->SrcRect.bottom + (1 << level) - 1) >> level};
        const POINT at = {args->DstPoint.x >> level, args->DstPoint.y >> level};
        const HRESULT copied = blt_level(*source, face * source_levels + first + level, *destination,
                                         face * destination->levels + level, area, at);
        if (FAILED(copied)) return copied;
    }
    return S_OK;
}

/* A compressed format's rectangle widens to whole blocks, and stays inside
 * both levels. */
HRESULT D3d9Device::blt_level(const D3d9Resource &source, uint32_t source_index, const D3d9Resource &destination,
                              uint32_t destination_index, const RECT &area, POINT at)
{
    AV_SCOPE("D3d9Device::blt_level");
    const D3d9Subresource &from = source.subresources[source_index];
    const D3d9Subresource &to = destination.subresources[destination_index];
    if (from.system == nullptr) return E_INVALIDARG;
    const D3d9FormatLayout layout(static_cast<uint32_t>(source.format));
    const uint32_t         block_width = layout.element_width();
    const uint32_t         block_height = layout.element_height();
    uint32_t left = static_cast<uint32_t>(area.left) / block_width * block_width;
    uint32_t top = static_cast<uint32_t>(area.top) / block_height * block_height;
    uint32_t right = static_cast<uint32_t>(area.right) < from.width ? static_cast<uint32_t>(area.right) : from.width;
    uint32_t bottom = static_cast<uint32_t>(area.bottom) < from.height ? static_cast<uint32_t>(area.bottom) : from.height;
    const uint32_t x = static_cast<uint32_t>(at.x) / block_width * block_width;
    const uint32_t y = static_cast<uint32_t>(at.y) / block_height * block_height;
    if (right <= left || bottom <= top || x >= to.width || y >= to.height) return S_OK;
    if (x + (right - left) > to.width) right = left + (to.width - x);
    if (y + (bottom - top) > to.height) bottom = top + (to.height - y);
    const uint8_t *const data = from.system + layout.offset_of(left, top, from.system_pitch);
    const D3DDDIBOX box = {x, y, x + (right - left), y + (bottom - top), 0u, 1u};
    return write_texture(destination, destination_index, box, data, from.system_pitch, from.system_slice_pitch, 0u);
}

/* A volume's levels, each box halved from the one above. */
HRESULT D3d9Device::vol_blt(const D3DDDIARG_VOLUMEBLT *args)
{
    AV_SCOPE("D3d9Device::vol_blt");
    const D3d9Resource *const destination = resource(args->hDstResource);
    const D3d9Resource *const source = resource(args->hSrcResource);
    if (destination == nullptr || source == nullptr || source->kind != D3d9Resource::Kind::system ||
        destination->kind != D3d9Resource::Kind::volume)
        return E_INVALIDARG;
    const D3d9FormatLayout layout(static_cast<uint32_t>(source->format));
    for (uint32_t level = 0u; level < destination->levels && level < source->count; level++) {
        const D3d9Subresource &from = source->subresources[level];
        const D3d9Subresource &to = destination->subresources[level];
        const D3DDDIBOX &whole = args->SrcBox;
        const uint32_t left = whole.Left >> level, top = whole.Top >> level, front = whole.Front >> level;
        const uint32_t right = (whole.Right >> level) > from.width ? from.width : (whole.Right >> level);
        const uint32_t bottom = (whole.Bottom >> level) > from.height ? from.height : (whole.Bottom >> level);
        const uint32_t back = (whole.Back >> level) > from.depth ? from.depth : (whole.Back >> level);
        if (right <= left || bottom <= top || back <= front || from.system == nullptr) break;
        const uint32_t x = args->DstX >> level, y = args->DstY >> level, z = args->DstZ >> level;
        if (x + (right - left) > to.width || y + (bottom - top) > to.height || z + (back - front) > to.depth) break;
        /* A block format's box starts on a block: the texel's element is
         * where its bytes are. */
        const uint8_t *const data = from.system + size_t{front} * from.system_slice_pitch +
                                    layout.offset_of(left, top, from.system_pitch);
        const D3DDDIBOX box = {x, y, x + (right - left), y + (bottom - top), z, z + (back - front)};
        const HRESULT copied =
            write_texture(*destination, level, box, data, from.system_pitch, from.system_slice_pitch, 0u);
        if (FAILED(copied)) return copied;
    }
    return S_OK;
}

/* A managed buffer's dirty range, from the runtime's copy. A lock's shadow
 * takes it too, since a later lock that keeps the contents reads it. */
HRESULT D3d9Device::buf_blt(const D3DDDIARG_BUFFERBLT *args)
{
    AV_SCOPE("D3d9Device::buf_blt");
    D3d9Resource *const       destination = resource(args->hDstResource);
    const D3d9Resource *const source = resource(args->hSrcResource);
    if (destination == nullptr || source == nullptr || source->kind != D3d9Resource::Kind::system ||
        source->subresources[0].system == nullptr)
        return E_INVALIDARG;
    const D3d9Subresource &from = source->subresources[0];
    D3d9Subresource       &to = destination->subresources[0];
    const uint32_t         start = args->SrcRange.Offset;
    if (start >= from.width || args->Offset >= to.width) return E_INVALIDARG;
    uint32_t bytes = args->SrcRange.Size < from.width - start ? args->SrcRange.Size : from.width - start;
    if (bytes == 0u) lines_.once("a Direct3D 9 BufBlt of no bytes");
    if (bytes > to.width - args->Offset) bytes = to.width - args->Offset;
    if (to.shadow != nullptr) memcpy(to.shadow + args->Offset, from.system + start, bytes);
    return write_buffer(destination->id, args->Offset, from.system + start, bytes, 0u);
}

/* Between host surfaces a StretchRect; into system memory a read-back the
 * runtime asked for (GetRenderTargetData, GetFrontBufferData); from system
 * memory a write; between two system-memory surfaces nothing, since the
 * runtime copies those itself after the call (prior-art.md §8). */
HRESULT D3d9Device::blt(const D3DDDIARG_BLT *args)
{
    AV_SCOPE("D3d9Device::blt");
    const D3d9Resource *const source = resource(args->hSrcResource);
    const D3d9Resource *const destination = resource(args->hDstResource);
    if (source == nullptr || destination == nullptr || args->SrcSubResourceIndex >= source->count ||
        args->DstSubResourceIndex >= destination->count)
        return E_INVALIDARG;
    if (refuses_opened(*source, "a Direct3D 9 blt from an opened surface") ||
        refuses_opened(*destination, "a Direct3D 9 blt into an opened surface"))
        return E_NOTIMPL;
    const bool from_system = source->kind == D3d9Resource::Kind::system;
    const bool to_system = destination->kind == D3d9Resource::Kind::system;
    if (from_system && to_system) return S_OK;
    if (!send_states()) return lost(__func__);
    if (to_system) return blt_to_system(*source, *destination, *args);
    if (from_system) {
        const POINT at = {args->DstRect.left, args->DstRect.top};
        return blt_level(*source, args->SrcSubResourceIndex, *destination, args->DstSubResourceIndex, args->SrcRect,
                         at);
    }
    uint32_t from_id = 0u, from_sub = 0u, to_id = 0u, to_sub = 0u;
    if (!surface_of(*source, args->SrcSubResourceIndex, from_id, from_sub) ||
        !surface_of(*destination, args->DstSubResourceIndex, to_id, to_sub))
        return E_INVALIDARG;
    const d3d9proto::Rect from_rect = {args->SrcRect.left, args->SrcRect.top, args->SrcRect.right, args->SrcRect.bottom};
    const d3d9proto::Rect to_rect = {args->DstRect.left, args->DstRect.top, args->DstRect.right, args->DstRect.bottom};
    const uint32_t filter = args->Flags.Linear ? filter_linear : args->Flags.Point ? filter_point : 0u;
    return d3d9proto::av_d3d9_StretchRect9(wire(), from_id, from_sub, from_rect, to_id, to_sub, to_rect, filter)
               ? S_OK
               : lost(__func__);
}

HRESULT D3d9Device::blt_to_system(const D3d9Resource &source, const D3d9Resource &destination,
                                  const D3DDDIARG_BLT &blt)
{
    AV_SCOPE("D3d9Device::blt_to_system");
    const D3d9Subresource &to = destination.subresources[blt.DstSubResourceIndex];
    if (to.system == nullptr) return E_INVALIDARG;
    uint32_t pitch = 0u, rows = 0u;
    const HRESULT read = read_surface(source, blt.SrcSubResourceIndex, pitch, rows);
    if (FAILED(read)) return read;
    /* Rows of elements, not texels: a block format's rectangle is whole rows
     * of blocks, each the line of its blocks. */
    const D3d9FormatLayout layout(static_cast<uint32_t>(source.format));
    const uint32_t         left = static_cast<uint32_t>(blt.SrcRect.left), top = static_cast<uint32_t>(blt.SrcRect.top);
    const uint32_t width = static_cast<uint32_t>(blt.SrcRect.right - blt.SrcRect.left);
    const uint32_t height = static_cast<uint32_t>(blt.SrcRect.bottom - blt.SrcRect.top);
    auto *const    out = const_cast<uint8_t *>(to.system);
    const size_t   line = layout.row_bytes(width);
    const size_t   first = static_cast<size_t>(layout.offset_of(left, top, pitch));
    const size_t   start = static_cast<size_t>(layout.offset_of(static_cast<uint32_t>(blt.DstRect.left),
                                                                static_cast<uint32_t>(blt.DstRect.top),
                                                                to.system_pitch));
    for (uint32_t row = 0u; row < layout.rows(height) && top / layout.element_height() + row < rows; row++) {
        const size_t from = first + size_t{row} * pitch;
        const size_t into = start + size_t{row} * to.system_pitch;
        if (from + line > readback_bytes_ || line > to.system_pitch) break;
        memcpy(out + into, readback_ + from, line);
    }
    return S_OK;
}

HRESULT D3d9Device::color_fill(const D3DDDIARG_COLORFILL *args)
{
    AV_SCOPE("D3d9Device::color_fill");
    const D3d9Resource *const target = resource(args->hResource);
    uint32_t                  id = 0u, subresource = 0u;
    if (target == nullptr) return S_OK;
    if (refuses_opened(*target, "a Direct3D 9 colour fill of an opened surface")) return E_NOTIMPL;
    if (!surface_of(*target, args->SubResourceIndex, id, subresource)) return E_INVALIDARG;
    const d3d9proto::Rect rect = {args->DstRect.left, args->DstRect.top, args->DstRect.right, args->DstRect.bottom};
    return d3d9proto::av_d3d9_ColorFill9(wire(), id, subresource, rect, args->Color) ? S_OK : lost(__func__);
}

/* Asked for only by the runtime's own clears of depth it cannot reach with
 * Clear; there is no host command for a fill in a native depth value. */
HRESULT D3d9Device::depth_fill(const D3DDDIARG_DEPTHFILL *)
{
    AV_SCOPE("D3d9Device::depth_fill");
    not_implemented("DepthFill");
    return S_OK;
}

HRESULT D3d9Device::generate_mip_sub_levels(const D3DDDIARG_GENERATEMIPSUBLEVELS *args)
{
    AV_SCOPE("D3d9Device::generate_mip_sub_levels");
    const D3d9Resource *const texture = resource(args->hResource);
    if (texture == nullptr || texture->id == 0u) return E_INVALIDARG;
    const uint32_t filter = args->Filter == D3DDDITEXF_NONE ? filter_linear : static_cast<uint32_t>(args->Filter);
    return d3d9proto::av_d3d9_GenerateMips9(wire(), texture->id, filter) ? S_OK : lost(__func__);
}

}  // namespace av::d3d9
