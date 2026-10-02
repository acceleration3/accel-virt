/* D3d9Session's surfaces, textures and buffers, and what is written into,
 * read out of and copied between them (d3d9_session.hpp). */
#include "core/support/scope_exit.hpp"
#include "core/trace/function_scope.hpp"
#include "executor/d3d/d3d9_session.hpp"
#include "executor/d3d/d3d9_texture_write.hpp"
#include "executor/shim/d3d9_host_device.hpp"
#include "executor/shim/d3d_headers.hpp"
#include "executor/d3d/shared_surfaces.hpp"
#include "executor/d3d/staged_frame.hpp"
#include "proto/resources/surface_point.hpp"
#include "proto/d3d/d3d9_format_layout.hpp"

#include <string.h>

namespace av {

namespace {

constexpr uint32_t kind_render_target = 1u;
constexpr uint32_t kind_depth_stencil = 2u;
constexpr uint32_t kind_offscreen_plain = 3u;
constexpr uint32_t flag_lockable_or_discard = 0x1u;
constexpr uint32_t type_2d = 1u;
constexpr uint32_t type_cube = 2u;
constexpr uint32_t type_volume = 3u;
constexpr uint32_t cube_faces = 6u;
constexpr uint32_t max_levels = 15u;                 /* a 16384 edge's chain */
constexpr uint32_t max_buffer_bytes = 256u << 20u;
constexpr uint32_t max_multisample = D3DMULTISAMPLE_16_SAMPLES;
constexpr uint32_t usage_kept = D3DUSAGE_RENDERTARGET | D3DUSAGE_DEPTHSTENCIL | D3DUSAGE_DYNAMIC |
                                D3DUSAGE_AUTOGENMIPMAP | D3DUSAGE_WRITEONLY | D3DUSAGE_POINTS |
                                D3DUSAGE_RTPATCHES | D3DUSAGE_NPATCHES | D3DUSAGE_DMAP;
/* What a write may ask of a dynamic resource's lock. */
constexpr uint32_t write_flags = D3DLOCK_DISCARD | D3DLOCK_NOOVERWRITE;

/* Where the host keeps a resource the guest made in `pool` (D3DPOOL_DEFAULT or
 * MANAGED): what renders or is drawn into stays default, what the guest
 * rewrites every frame is dynamic, and everything else is managed, which a
 * write reaches through a plain lock. */
bool host_pool(uint32_t pool, uint32_t usage, D3DPOOL &placed)
{
    AV_SCOPE("host_pool");
    if (pool != D3DPOOL_DEFAULT && pool != D3DPOOL_MANAGED) return false;
    placed = (usage & (D3DUSAGE_RENDERTARGET | D3DUSAGE_DEPTHSTENCIL | D3DUSAGE_DYNAMIC)) != 0u ? D3DPOOL_DEFAULT
                                                                                             : D3DPOOL_MANAGED;
    return true;
}

/* The lock flags a write carries to a resource in `pool`: a managed one's
 * lock takes neither discard nor no-overwrite. */
DWORD lock_flags(D3DPOOL pool, uint32_t flags)
{
    AV_SCOPE("lock_flags");
    return pool == D3DPOOL_DEFAULT ? flags & write_flags : 0u;
}

}  // namespace

bool D3d9Session::on_CreateSurface9(uint32_t surface, uint32_t kind, uint32_t width, uint32_t height,
                                    uint32_t format, uint32_t multisample, uint32_t quality, uint32_t flags)
{
    AV_SCOPE("D3d9Session::on_CreateSurface9");
    if (device() == nullptr || width == 0u || height == 0u || width > max_dimension || height > max_dimension ||
        multisample > max_multisample)
        return false;
    IDirect3DDevice9 *const host = device();
    const D3DFORMAT         as = static_cast<D3DFORMAT>(format);
    const auto              samples = static_cast<D3DMULTISAMPLE_TYPE>(multisample);
    const BOOL              flag = (flags & flag_lockable_or_discard) != 0u ? TRUE : FALSE;
    IDirect3DSurface9      *made = nullptr;
    HRESULT                 result = D3DERR_INVALIDCALL;
    switch (kind) {
    case kind_render_target:
        result = host->CreateRenderTarget(width, height, as, samples, quality, flag, &made, nullptr);
        break;
    case kind_depth_stencil:
        result = host->CreateDepthStencilSurface(width, height, as, samples, quality, flag, &made, nullptr);
        break;
    case kind_offscreen_plain:
        result = host->CreateOffscreenPlainSurface(width, height, as, D3DPOOL_DEFAULT, &made, nullptr);
        break;
    default: return false;
    }
    return SUCCEEDED(result) && keep(surface, D3dObjectKind::surface9, made);
}

bool D3d9Session::on_CreateTexture9(uint32_t texture, uint32_t type, uint32_t width, uint32_t height,
                                    uint32_t depth, uint32_t levels, uint32_t usage, uint32_t format, uint32_t pool)
{
    AV_SCOPE("D3d9Session::on_CreateTexture9");
    D3DPOOL placed = D3DPOOL_DEFAULT;
    if (device() == nullptr || width == 0u || height == 0u || depth == 0u || width > max_dimension ||
        height > max_dimension || depth > max_dimension || levels > max_levels || (usage & ~usage_kept) != 0u ||
        !host_pool(pool, usage, placed))
        return false;
    IDirect3DDevice9 *const host = device();
    const D3DFORMAT         as = static_cast<D3DFORMAT>(format);
    IUnknown               *made = nullptr;
    HRESULT                 result = D3DERR_INVALIDCALL;
    D3dObjectKind           kept = D3dObjectKind::texture9;
    if (type == type_2d && depth == 1u) {
        IDirect3DTexture9 *texture2d = nullptr;
        result = host->CreateTexture(width, height, levels, usage, as, placed, &texture2d, nullptr);
        made = texture2d;
    } else if (type == type_cube && width == height && depth == 1u) {
        IDirect3DCubeTexture9 *cube = nullptr;
        result = host->CreateCubeTexture(width, levels, usage, as, placed, &cube, nullptr);
        made = cube;
        kept = D3dObjectKind::cube_texture9;
    } else if (type == type_volume) {
        IDirect3DVolumeTexture9 *volume = nullptr;
        result = host->CreateVolumeTexture(width, height, depth, levels, usage, as, placed, &volume, nullptr);
        made = volume;
        kept = D3dObjectKind::volume_texture9;
    }
    return SUCCEEDED(result) && keep(texture, kept, made);
}

bool D3d9Session::on_CreateVertexBuffer9(uint32_t buffer, uint32_t length, uint32_t usage, uint32_t fvf,
                                         uint32_t pool)
{
    AV_SCOPE("D3d9Session::on_CreateVertexBuffer9");
    D3DPOOL placed = D3DPOOL_DEFAULT;
    if (device() == nullptr || length == 0u || length > max_buffer_bytes || (usage & ~usage_kept) != 0u ||
        !host_pool(pool, usage, placed))
        return false;
    IDirect3DVertexBuffer9 *made = nullptr;
    const HRESULT result = device()->CreateVertexBuffer(length, usage, fvf, placed, &made, nullptr);
    return SUCCEEDED(result) && keep(buffer, D3dObjectKind::vertex_buffer9, made);
}

bool D3d9Session::on_CreateIndexBuffer9(uint32_t buffer, uint32_t length, uint32_t usage, uint32_t format,
                                        uint32_t pool)
{
    AV_SCOPE("D3d9Session::on_CreateIndexBuffer9");
    D3DPOOL placed = D3DPOOL_DEFAULT;
    if (device() == nullptr || length == 0u || length > max_buffer_bytes || (usage & ~usage_kept) != 0u ||
        (format != D3DFMT_INDEX16 && format != D3DFMT_INDEX32) || !host_pool(pool, usage, placed))
        return false;
    IDirect3DIndexBuffer9 *made = nullptr;
    const HRESULT          result = device()->CreateIndexBuffer(
        length, usage, static_cast<D3DFORMAT>(format), placed, &made, nullptr);
    return SUCCEEDED(result) && keep(buffer, D3dObjectKind::index_buffer9, made);
}

bool D3d9Session::on_WriteTexture9(uint32_t texture, uint32_t subresource, const d3d9proto::Box &box,
                                   uint32_t row_pitch, uint32_t slice_pitch, uint32_t flags, const uint8_t *data,
                                   uint64_t data_size)
{
    AV_SCOPE("D3d9Session::on_WriteTexture9");
    if (mine<IDirect3DVolumeTexture9>(texture, D3dObjectKind::volume_texture9) != nullptr)
        return write_volume(texture, subresource, box, row_pitch, slice_pitch, flags, data, data_size);
    return write_level(texture, subresource, box, row_pitch, flags, data, data_size);
}

/* A 2D texture's level is its subresource; a cube's subresources go face by
 * face, every level of one face before the next. */
bool D3d9Session::write_level(uint32_t texture, uint32_t subresource, const d3d9proto::Box &box, uint32_t row_pitch,
                              uint32_t flags, const uint8_t *data, uint64_t data_size)
{
    AV_SCOPE("D3d9Session::write_level");
    auto *const flat = mine<IDirect3DTexture9>(texture, D3dObjectKind::texture9);
    auto *const cube = flat == nullptr ? mine<IDirect3DCubeTexture9>(texture, D3dObjectKind::cube_texture9) : nullptr;
    IDirect3DBaseTexture9 *const base = flat != nullptr ? static_cast<IDirect3DBaseTexture9 *>(flat) : cube;
    if (base == nullptr) return false;
    const uint32_t levels = base->GetLevelCount();
    const uint32_t level = subresource % levels;
    const uint32_t face = subresource / levels;
    if (face >= (cube != nullptr ? cube_faces : 1u)) return false;
    D3DSURFACE_DESC desc = {};
    if (FAILED(flat != nullptr ? flat->GetLevelDesc(level, &desc) : cube->GetLevelDesc(level, &desc))) return false;
    const D3d9FormatLayout layout(static_cast<uint32_t>(desc.Format));
    const D3d9TextureWrite write(box, row_pitch, 0u, data_size, layout);
    if (!write.fits(desc.Width, desc.Height, 1u)) return false;
    const RECT     area = {static_cast<LONG>(box.left), static_cast<LONG>(box.top), static_cast<LONG>(box.right),
                           static_cast<LONG>(box.bottom)};
    D3DLOCKED_RECT locked = {};
    const DWORD    lock = lock_flags(desc.Pool, flags);
    const HRESULT  opened = flat != nullptr ? flat->LockRect(level, &locked, &area, lock)
                                            : cube->LockRect(static_cast<D3DCUBEMAP_FACES>(face), level, &locked, &area,
                                                             lock);
    if (FAILED(opened)) return false;
    const bool fits = static_cast<uint32_t>(locked.Pitch) >= write.row_bytes();
    for (uint32_t row = 0u; fits && row < write.rows(); row++)
        memcpy(static_cast<uint8_t *>(locked.pBits) + uint64_t{row} * static_cast<uint32_t>(locked.Pitch),
               data + uint64_t{row} * row_pitch, write.row_bytes());
    if (flat != nullptr) flat->UnlockRect(level);
    else cube->UnlockRect(static_cast<D3DCUBEMAP_FACES>(face), level);
    return fits;
}

bool D3d9Session::write_volume(uint32_t texture, uint32_t subresource, const d3d9proto::Box &box,
                               uint32_t row_pitch, uint32_t slice_pitch, uint32_t flags, const uint8_t *data,
                               uint64_t data_size)
{
    AV_SCOPE("D3d9Session::write_volume");
    auto *const volume = mine<IDirect3DVolumeTexture9>(texture, D3dObjectKind::volume_texture9);
    D3DVOLUME_DESC desc = {};
    if (subresource >= volume->GetLevelCount() || FAILED(volume->GetLevelDesc(subresource, &desc))) return false;
    const D3d9FormatLayout layout(static_cast<uint32_t>(desc.Format));
    const D3d9TextureWrite write(box, row_pitch, slice_pitch, data_size, layout);
    if (!write.fits(desc.Width, desc.Height, desc.Depth)) return false;
    const D3DBOX  area = {box.left, box.top, box.right, box.bottom, box.front, box.back};
    D3DLOCKED_BOX locked = {};
    if (FAILED(volume->LockBox(subresource, &locked, &area, lock_flags(desc.Pool, flags)))) return false;
    const bool fits = static_cast<uint32_t>(locked.RowPitch) >= write.row_bytes() &&
                      static_cast<uint64_t>(locked.SlicePitch) >=
                          uint64_t{static_cast<uint32_t>(locked.RowPitch)} * write.rows();
    for (uint32_t slice = 0u; fits && slice < write.slices(); slice++)
        for (uint32_t row = 0u; row < write.rows(); row++)
            memcpy(static_cast<uint8_t *>(locked.pBits) + uint64_t{slice} * static_cast<uint32_t>(locked.SlicePitch) +
                       uint64_t{row} * static_cast<uint32_t>(locked.RowPitch),
                   data + uint64_t{slice} * slice_pitch + uint64_t{row} * row_pitch, write.row_bytes());
    volume->UnlockBox(subresource);
    return fits;
}

bool D3d9Session::on_WriteBuffer9(uint32_t buffer, uint32_t offset, uint32_t flags, const uint8_t *data,
                                  uint64_t data_size)
{
    AV_SCOPE("D3d9Session::on_WriteBuffer9");
    auto *const vertices = mine<IDirect3DVertexBuffer9>(buffer, D3dObjectKind::vertex_buffer9);
    auto *const indices = vertices == nullptr ? mine<IDirect3DIndexBuffer9>(buffer, D3dObjectKind::index_buffer9)
                                              : nullptr;
    if (vertices == nullptr && indices == nullptr) return false;
    D3DVERTEXBUFFER_DESC vertex_desc = {};
    D3DINDEXBUFFER_DESC  index_desc = {};
    if (vertices != nullptr) vertices->GetDesc(&vertex_desc);
    else indices->GetDesc(&index_desc);
    const uint64_t size = vertices != nullptr ? vertex_desc.Size : index_desc.Size;
    const DWORD    lock = lock_flags(vertices != nullptr ? vertex_desc.Pool : index_desc.Pool, flags);
    if (data_size == 0u || data_size > size || offset > size - data_size) return false;
    void *mapped = nullptr;
    const HRESULT opened =
        vertices != nullptr ? vertices->Lock(offset, static_cast<UINT>(data_size), &mapped, lock)
                            : indices->Lock(offset, static_cast<UINT>(data_size), &mapped, lock);
    if (FAILED(opened)) return false;
    memcpy(mapped, data, data_size);
    if (vertices != nullptr) vertices->Unlock();
    else indices->Unlock();
    return true;
}

bool D3d9Session::on_GenerateMips9(uint32_t texture, uint32_t filter)
{
    AV_SCOPE("D3d9Session::on_GenerateMips9");
    auto *const flat = mine<IDirect3DTexture9>(texture, D3dObjectKind::texture9);
    IDirect3DBaseTexture9 *const base =
        flat != nullptr ? static_cast<IDirect3DBaseTexture9 *>(flat)
                        : mine<IDirect3DCubeTexture9>(texture, D3dObjectKind::cube_texture9);
    if (base == nullptr || filter == D3DTEXF_NONE || filter > D3DTEXF_ANISOTROPIC ||
        FAILED(base->SetAutoGenFilterType(static_cast<D3DTEXTUREFILTERTYPE>(filter))))
        return false;
    base->GenerateMipSubLevels();
    return true;
}

IDirect3DSurface9 *D3d9Session::surface(uint32_t id, uint32_t subresource) const
{
    AV_SCOPE("D3d9Session::surface");
    auto *const plain = mine<IDirect3DSurface9>(id, D3dObjectKind::surface9);
    if (plain != nullptr) {
        if (subresource != 0u) return nullptr;
        plain->AddRef();
        return plain;
    }
    IDirect3DSurface9 *level = nullptr;
    auto *const        flat = mine<IDirect3DTexture9>(id, D3dObjectKind::texture9);
    if (flat != nullptr) {
        if (subresource >= flat->GetLevelCount() || FAILED(flat->GetSurfaceLevel(subresource, &level))) return nullptr;
        return level;
    }
    auto *const cube = mine<IDirect3DCubeTexture9>(id, D3dObjectKind::cube_texture9);
    if (cube == nullptr) return nullptr;
    const uint32_t levels = cube->GetLevelCount();
    const uint32_t face = subresource / levels;
    if (face >= cube_faces ||
        FAILED(cube->GetCubeMapSurface(static_cast<D3DCUBEMAP_FACES>(face), subresource % levels, &level)))
        return nullptr;
    return level;
}

bool D3d9Session::on_ReadSurface9(uint32_t surface_id, uint32_t subresource, uint32_t &hresult, uint32_t &width,
                                  uint32_t &height, uint32_t &format, uint32_t &pitch, uint8_t *pixels,
                                  uint64_t pixels_capacity, uint64_t &pixels_size)
{
    AV_SCOPE("D3d9Session::on_ReadSurface9");
    pixels_size = 0u;
    hresult = static_cast<uint32_t>(D3DERR_INVALIDCALL);
    IDirect3DSurface9 *const source = surface(surface_id, subresource);
    if (source == nullptr) return true;
    ScopeExit       release([source] { source->Release(); });
    D3DSURFACE_DESC desc = {};
    if (FAILED(source->GetDesc(&desc))) return true;
    width = desc.Width;
    height = desc.Height;
    format = static_cast<uint32_t>(desc.Format);
    hresult = static_cast<uint32_t>(read(source, pitch, pixels, pixels_capacity, pixels_size));
    return true;
}

/* A surface in memory the CPU sees - a managed texture's level - is locked
 * where it is; a render target is not lockable on the host, so it is read
 * through a system-memory surface of its size, which GetRenderTargetData
 * waits for. */
int32_t D3d9Session::read(IDirect3DSurface9 *source, uint32_t &pitch, uint8_t *pixels, uint64_t capacity,
                          uint64_t &written)
{
    AV_SCOPE("D3d9Session::read");
    D3DSURFACE_DESC desc = {};
    source->GetDesc(&desc);
    IDirect3DSurface9 *copy = nullptr;
    if (desc.Pool == D3DPOOL_DEFAULT) {
        const HRESULT made = device()->CreateOffscreenPlainSurface(desc.Width, desc.Height, desc.Format,
                                                                             D3DPOOL_SYSTEMMEM, &copy, nullptr);
        if (FAILED(made)) return made;
    }
    IDirect3DSurface9 *const locked_surface = copy != nullptr ? copy : source;
    ScopeExit release([copy] {
        if (copy != nullptr) copy->Release();
    });
    if (copy != nullptr) {
        const HRESULT copied = device()->GetRenderTargetData(source, copy);
        if (FAILED(copied)) return copied;
    }
    D3DLOCKED_RECT locked = {};
    const HRESULT  opened = locked_surface->LockRect(&locked, nullptr, D3DLOCK_READONLY);
    if (FAILED(opened)) return opened;
    pitch = static_cast<uint32_t>(locked.Pitch);
    const uint64_t rows = D3d9FormatLayout(static_cast<uint32_t>(desc.Format)).rows(desc.Height);
    /* All of it or none: a surface cut short at the guest's room would be
     * presented or copied as a whole one with its bottom rows stale. */
    if (uint64_t{pitch} * rows > capacity) {
        locked_surface->UnlockRect();
        return D3DERR_MOREDATA;
    }
    memcpy(pixels, locked.pBits, rows * pitch);
    written = rows * pitch;
    locked_surface->UnlockRect();
    return D3D_OK;
}

bool D3d9Session::on_StretchRect9(uint32_t source, uint32_t source_subresource, const d3d9proto::Rect &source_rect,
                                  uint32_t destination, uint32_t destination_subresource,
                                  const d3d9proto::Rect &destination_rect, uint32_t filter)
{
    AV_SCOPE("D3d9Session::on_StretchRect9");
    if (filter > D3DTEXF_LINEAR) return false;
    IDirect3DSurface9 *const from = surface(source, source_subresource);
    IDirect3DSurface9 *const to = surface(destination, destination_subresource);
    ScopeExit release([from, to] {
        if (from != nullptr) from->Release();
        if (to != nullptr) to->Release();
    });
    if (from == nullptr || to == nullptr) return false;
    const RECT  from_area = {source_rect.left, source_rect.top, source_rect.right, source_rect.bottom};
    const RECT  to_area = {destination_rect.left, destination_rect.top, destination_rect.right,
                           destination_rect.bottom};
    const RECT *from_named = source_rect.right > source_rect.left ? &from_area : nullptr;
    const RECT *to_named = destination_rect.right > destination_rect.left ? &to_area : nullptr;
    return SUCCEEDED(device()->StretchRect(from, from_named, to, to_named,
                                                     static_cast<D3DTEXTUREFILTERTYPE>(filter)));
}

bool D3d9Session::on_ColorFill9(uint32_t surface_id, uint32_t subresource, const d3d9proto::Rect &rect,
                                uint32_t color)
{
    AV_SCOPE("D3d9Session::on_ColorFill9");
    IDirect3DSurface9 *const target = surface(surface_id, subresource);
    if (target == nullptr) return false;
    ScopeExit  release([target] { target->Release(); });
    const RECT area = {rect.left, rect.top, rect.right, rect.bottom};
    return SUCCEEDED(device()->ColorFill(target, rect.right > rect.left ? &area : nullptr, color));
}

/* What D3dBackend::on_PresentPoint does for Direct3D 11: the frame staged
 * where the commands before it left the surface, kept under the guest's key
 * and point for the present engine. A surface it cannot stage is abandoned,
 * so the present naming it is refused rather than left waiting. */
bool D3d9Session::on_PresentPoint9(uint32_t surface_id, uint32_t subresource, uint64_t key, uint32_t point)
{
    AV_SCOPE("D3d9Session::on_PresentPoint9");
    if (device() == nullptr) return false;
    IDirect3DSurface9 *const source = surface(surface_id, subresource);
    if (source == nullptr) return false;
    ScopeExit             release([source] { source->Release(); });
    const SurfacePoint    frame(key, point);
    if (surfaces_ == nullptr || host_ == nullptr || !frame.names()) return true;
    D3DSURFACE_DESC desc = {};
    if (FAILED(source->GetDesc(&desc)) || (desc.Format != D3DFMT_X8R8G8B8 && desc.Format != D3DFMT_A8R8G8B8)) {
        surfaces_->abandon(frame);
        return true;
    }
    static_cast<void>(surfaces_->presented(frame));
    StagedFrame *const staged = host_->stage_frame(device(), source, desc.Width, desc.Height,
                                                   static_cast<uint32_t>(desc.Format),
                                                   desc.MultiSampleType != D3DMULTISAMPLE_NONE);
    if (staged == nullptr) {
        surfaces_->abandon(frame);
        return true;
    }
    if (!surfaces_->keep(frame, *staged)) staged->release();
    return true;
}

}  // namespace av
