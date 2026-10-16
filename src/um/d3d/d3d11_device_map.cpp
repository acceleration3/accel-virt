/* The CPU's way into a resource: a map hands the application the resource's
 * shadow in guest memory, filled from the host first when it is read, and an
 * unmap after a write carries the subresource back to the host. The host's
 * UpdateSubresource runs in command order, so a draw recorded before the
 * unmap still reads the old contents - which is what NO_OVERWRITE promises
 * and DISCARD allows. */
#include "core/log/log_line.hpp"
#include "core/trace/function_scope.hpp"
#include "core/log/log_sink.hpp"
#include "proto/generated/d3d/d3d_dispatch_client.h"
#include "um/d3d/d3d11_device.hpp"
#include "um/d3d/d3d_session.hpp"

namespace av::d3d11 {

namespace {

/* D3D11_COPY_FLAGS, which the host's UpdateSubresource1 takes as the hint a
 * map gave. */
constexpr uint32_t copy_no_overwrite = 0x1u;
constexpr uint32_t copy_discard = 0x2u;

}  // namespace

void D3d11Device::map(D3D10DDI_HRESOURCE handle, UINT subresource, D3D10_DDI_MAP type, UINT,
                      D3D10DDI_MAPPED_SUBRESOURCE *mapped)
{
    AV_SCOPE("D3d11Device::map");
    ResourceRecord &resource = record(handle);
    if (resource.shadow == nullptr || subresource >= subresource_count(resource) || mapped == nullptr) {
        set_error(E_INVALIDARG);
        return;
    }
    /* A deferred context records its writes as copies, into a shadow of its
     * own. */
    if (parent_ != nullptr && resource.shadow_local == 0u && !own_shadow(resource)) {
        set_error(E_OUTOFMEMORY);
        return;
    }
    /* A buffer on a slice is mapped in place: a discard renames it, anything
     * else writes where the host already reads. A discard with no slice free
     * to rename to is a copy: from then on the buffer is copied, so the
     * host's buffer and the guest's never disagree about where it lives. */
    if (resource.on_slice != 0u && type == D3D10_DDI_MAP_WRITE_DISCARD && !rename_slice(resource)) {
        lines_.once("no slice free for a discarding map: the buffer is copied from then on");
        if (!leave_slice_for_copies(resource)) {
            set_error(E_OUTOFMEMORY);
            return;
        }
    }
    if (resource.on_slice != 0u) {
        mapped->pData = resource.shadow;
        mapped->RowPitch = static_cast<UINT>(resource.shadow_bytes);
        mapped->DepthPitch = static_cast<UINT>(resource.shadow_bytes);
        return;
    }
    const uint32_t flags = type == D3D10_DDI_MAP_WRITE_DISCARD      ? copy_discard
                           : type == D3D10_DDI_MAP_WRITE_NOOVERWRITE ? copy_no_overwrite
                                                                     : 0u;
    /* Refused before the application is handed memory: a write the unmap
     * could not find would never reach the host. */
    if (type != D3D10_DDI_MAP_READ && maps_.open(resource.id, subresource, flags) != Status::ok) {
        lines_.once("a map the device had no memory to hold open for writing");
        set_error(E_OUTOFMEMORY);
        return;
    }
    uint8_t *const at = resource.shadow + subresource_offset(resource, subresource);
    const size_t bytes = subresource_bytes(resource, subresource);
    if ((type == D3D10_DDI_MAP_READ || type == D3D10_DDI_MAP_READWRITE) && !read_back(resource, subresource, at, bytes)) {
        LogLine(lines_)
            .text("a map could not read back ")
            .number(bytes)
            .text(" bytes of format ")
            .number(resource.format)
            .write();
        set_error(D3DDDIERR_DEVICEREMOVED);
        return;
    }
    /* What the read-back wrote is the host's already. */
    if ((type == D3D10_DDI_MAP_READ || type == D3D10_DDI_MAP_READWRITE) && resource.shadow_watched != 0u)
        shadows_.forget_writes(at, bytes);
    uint32_t width = 0u, height = 0u, depth = 0u;
    extent(resource, subresource, width, height, depth);
    mapped->pData = at;
    mapped->RowPitch = row_bytes(resource, width);
    mapped->DepthPitch = mapped->RowPitch * rows(resource, height);
}

void D3d11Device::unmap(D3D10DDI_HRESOURCE handle, UINT subresource)
{
    AV_SCOPE("D3d11Device::unmap");
    ResourceRecord &resource = record(handle);
    uint32_t        flags = 0u;
    /* What the application wrote is already where the host GPU reads it. */
    if (resource.on_slice != 0u) return;
    if (resource.shadow == nullptr || !maps_.close(resource.id, subresource, flags)) return;
    if (resource.shadow_watched != 0u && upload_written(resource, flags)) return;
    uint32_t width = 0u, height = 0u, depth = 0u;
    extent(resource, subresource, width, height, depth);
    const uint32_t line = row_bytes(resource, width);
    upload(resource, subresource, nullptr, resource.shadow + subresource_offset(resource, subresource), line,
           line * rows(resource, height), flags);
}

/* The pages of a watched buffer the application wrote, a box each run; the
 * first carries the map's DISCARD, as a whole upload's first piece does.
 * False when the watch cannot tell, and the buffer then goes whole. */
bool D3d11Device::upload_written(const ResourceRecord &resource, uint32_t flags)
{
    AV_SCOPE("D3d11Device::upload_written");
    WatchedShadows::Run runs[WatchedShadows::max_runs];
    uint32_t            count = 0u;
    if (!shadows_.take_written(resource.shadow, resource.shadow_bytes, runs, count)) return false;
    for (uint32_t index = 0u; index < count; index++) {
        const D3D10_DDI_BOX box = {static_cast<LONG>(runs[index].offset), 0, 0,
                                   static_cast<LONG>(runs[index].offset + runs[index].bytes), 1, 1};
        upload(resource, 0u, &box, resource.shadow + runs[index].offset, 0u, 0u,
               index == 0u ? flags : flags & ~copy_discard);
    }
    return true;
}

VOID D3d11Device::resource_map(D3D10DDI_HRESOURCE resource, UINT sub, D3D10_DDI_MAP type, UINT flags,
                               D3D10DDI_MAPPED_SUBRESOURCE *mapped)
{
    AV_SCOPE("D3d11Device::resource_map");
    map(resource, sub, type, flags, mapped);
}

VOID D3d11Device::resource_unmap(D3D10DDI_HRESOURCE resource, UINT sub)
{
    AV_SCOPE("D3d11Device::resource_unmap");
    unmap(resource, sub);
}

VOID D3d11Device::staging_resource_map(D3D10DDI_HRESOURCE resource, UINT sub, D3D10_DDI_MAP type, UINT flags,
                                       D3D10DDI_MAPPED_SUBRESOURCE *mapped)
{
    AV_SCOPE("D3d11Device::staging_resource_map");
    map(resource, sub, type, flags, mapped);
}

VOID D3d11Device::staging_resource_unmap(D3D10DDI_HRESOURCE resource, UINT sub)
{
    AV_SCOPE("D3d11Device::staging_resource_unmap");
    unmap(resource, sub);
}

VOID D3d11Device::dynamic_ia_buffer_map_no_overwrite(D3D10DDI_HRESOURCE resource, UINT sub, D3D10_DDI_MAP type,
                                                     UINT flags, D3D10DDI_MAPPED_SUBRESOURCE *mapped)
{
    AV_SCOPE("D3d11Device::dynamic_ia_buffer_map_no_overwrite");
    map(resource, sub, type, flags, mapped);
}

VOID D3d11Device::dynamic_ia_buffer_map_discard(D3D10DDI_HRESOURCE resource, UINT sub, D3D10_DDI_MAP type, UINT flags,
                                                D3D10DDI_MAPPED_SUBRESOURCE *mapped)
{
    AV_SCOPE("D3d11Device::dynamic_ia_buffer_map_discard");
    map(resource, sub, type, flags, mapped);
}

VOID D3d11Device::dynamic_ia_buffer_unmap(D3D10DDI_HRESOURCE resource, UINT sub)
{
    AV_SCOPE("D3d11Device::dynamic_ia_buffer_unmap");
    unmap(resource, sub);
}

VOID D3d11Device::dynamic_constant_buffer_map_discard(D3D10DDI_HRESOURCE resource, UINT sub, D3D10_DDI_MAP type,
                                                      UINT flags, D3D10DDI_MAPPED_SUBRESOURCE *mapped)
{
    AV_SCOPE("D3d11Device::dynamic_constant_buffer_map_discard");
    map(resource, sub, type, flags, mapped);
}

VOID D3d11Device::dynamic_constant_buffer_map_no_overwrite(D3D10DDI_HRESOURCE resource, UINT sub, D3D10_DDI_MAP type,
                                                           UINT flags, D3D10DDI_MAPPED_SUBRESOURCE *mapped)
{
    AV_SCOPE("D3d11Device::dynamic_constant_buffer_map_no_overwrite");
    map(resource, sub, type, flags, mapped);
}

VOID D3d11Device::dynamic_constant_buffer_unmap(D3D10DDI_HRESOURCE resource, UINT sub)
{
    AV_SCOPE("D3d11Device::dynamic_constant_buffer_unmap");
    unmap(resource, sub);
}

VOID D3d11Device::dynamic_resource_map_discard(D3D10DDI_HRESOURCE resource, UINT sub, D3D10_DDI_MAP type, UINT flags,
                                               D3D10DDI_MAPPED_SUBRESOURCE *mapped)
{
    AV_SCOPE("D3d11Device::dynamic_resource_map_discard");
    map(resource, sub, type, flags, mapped);
}

VOID D3d11Device::dynamic_resource_unmap(D3D10DDI_HRESOURCE resource, UINT sub)
{
    AV_SCOPE("D3d11Device::dynamic_resource_unmap");
    unmap(resource, sub);
}

BOOL D3d11Device::resource_is_staging_busy(D3D10DDI_HRESOURCE)
{
    AV_SCOPE("D3d11Device::resource_is_staging_busy");
    return FALSE;
}

VOID D3d11Device::resource_update_subresource_up(D3D10DDI_HRESOURCE handle, UINT subresource, const D3D10_DDI_BOX *box,
                                                 const void *data, UINT row_pitch, UINT depth_pitch, UINT flags)
{
    AV_SCOPE("D3d11Device::resource_update_subresource_up");
    const ResourceRecord &resource = record(handle);
    upload(resource, subresource, box, data, row_pitch, depth_pitch, flags);
}

VOID D3d11Device::default_constant_buffer_update_subresource_up(D3D10DDI_HRESOURCE handle, UINT subresource,
                                                                const D3D10_DDI_BOX *box, const void *data,
                                                                UINT row_pitch, UINT depth_pitch, UINT flags)
{
    AV_SCOPE("D3d11Device::default_constant_buffer_update_subresource_up");
    resource_update_subresource_up(handle, subresource, box, data, row_pitch, depth_pitch, flags);
}

bool D3d11Device::place_on_slice(ResourceRecord &resource, size_t bytes)
{
    AV_SCOPE("D3d11Device::place_on_slice");
    if (resource.dimension != D3D10DDIRESOURCE_BUFFER || (resource.cpu_access & D3D10_DDI_CPU_ACCESS_WRITE) == 0u ||
        !slices_.place(bytes, resource.slice))
        return false;
    resource.on_slice = 1u;
    resource.shadow = session_.slice_view() + resource.slice.offset_;
    return true;
}

bool D3d11Device::rename_slice(ResourceRecord &resource)
{
    AV_SCOPE("D3d11Device::rename_slice");
    if (!slices_.rename(resource.shadow_bytes, resource.slice)) return false;
    resource.shadow = session_.slice_view() + resource.slice.offset_;
    return bind_slice(resource);
}

/* A deferred context's copy of a record names the immediate context's shadow:
 * a slice the host draws in place, or memory the immediate context's own maps
 * write and watch. Neither is this context's to write - its writes reach the
 * host only when the command list runs - so its first map of the copy takes a
 * shadow of its own, and the slice is only forgotten (leave_slice). Nothing is
 * copied into it: a deferred context's first map of a resource in a list
 * discards it, and NO_OVERWRITE after that writes beside what this context
 * wrote. The runtime destroys every copy a list used once the list is made,
 * and the copy then gives the shadow back (forget_copy): kept, Dark Souls
 * Remastered's one-MiB vertex buffer leaked a shadow a frame, until the guest
 * ran out of memory. */
bool D3d11Device::own_shadow(ResourceRecord &resource)
{
    AV_SCOPE("D3d11Device::own_shadow");
    bool     watched = false;
    uint8_t *shadow = resource.dimension == D3D10DDIRESOURCE_BUFFER
                          ? shadows_.allocate(resource.shadow_bytes, watched)
                          : static_cast<uint8_t *>(memory_.allocate(resource.shadow_bytes, Pool::paged));
    if (shadow == nullptr) return false;
    if (resource.on_slice != 0u) leave_slice(resource);
    resource.shadow = shadow;
    resource.shadow_watched = watched ? 1u : 0u;
    resource.shadow_local = 1u;
    return true;
}

/* What the runtime destroys on a deferred context is its copy of a record:
 * the object is the immediate context's, and only a shadow the copy took is
 * the copy's to give back. */
void D3d11Device::forget_copy(ResourceRecord &resource)
{
    AV_SCOPE("D3d11Device::forget_copy");
    maps_.forget(resource.id);
    if (resource.shadow_local != 0u) shadows_.release(resource.shadow, resource.shadow_watched != 0u);
    resource = ResourceRecord{};
}

/* The slice is left behind what was recorded, for the GPU may still read
 * it; the buffer's bytes go on in a shadow of its own, which the copies send
 * from. The host's buffer leaves the slice at the first copy's discard. */
bool D3d11Device::leave_slice_for_copies(ResourceRecord &resource)
{
    AV_SCOPE("D3d11Device::leave_slice_for_copies");
    bool     watched = false;
    uint8_t *shadow = shadows_.allocate(resource.shadow_bytes, watched);
    if (shadow == nullptr) return false;
    __builtin_memcpy(shadow, resource.shadow, resource.shadow_bytes);
    if (watched) shadows_.forget_writes(shadow, resource.shadow_bytes);
    leave_slice(resource);
    resource.shadow = shadow;
    resource.shadow_watched = watched ? 1u : 0u;
    return true;
}

bool D3d11Device::bind_slice(const ResourceRecord &resource)
{
    AV_SCOPE("D3d11Device::bind_slice");
    return d3dproto::av_d3d_BindBufferSlice(wire(), resource.id, session_.slice_memory(), resource.slice.offset_,
                                            resource.slice.bytes_);
}

/* A deferred context's record is a copy of the immediate one's
 * (context_local), whose slice it names: the immediate record keeps it, and
 * retires it when it renames or destroys the buffer, while a deferred
 * context owns no slices (ContextSlices) and only forgets it (own_shadow).
 * Retired there too, it went back free twice - once while the immediate
 * buffer was still on it - and two buffers took the same slice: the
 * multithreading sample's per-thread constant buffers left duplicates that
 * computeparticles' constants, renamed a thousand times a batch, then shared,
 * one step reading another's. */
void D3d11Device::leave_slice(ResourceRecord &resource)
{
    AV_SCOPE("D3d11Device::leave_slice");
    slices_.leave(resource.slice);
    resource.on_slice = 0u;
    resource.shadow = nullptr;
}

}  // namespace av::d3d11
