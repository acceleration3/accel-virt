/* The CPU's way into a resource: a map hands the application the resource's
 * shadow in guest memory, filled from the host first when it is read, and an
 * unmap after a write carries the subresource back to the host. The host's
 * UpdateSubresource runs in command order, so a draw recorded before the
 * unmap still reads the old contents - which is what NO_OVERWRITE promises
 * and DISCARD allows. */
#include "core/log/log_line.hpp"
#include "core/trace/function_scope.hpp"
#include "core/log/log_sink.hpp"
#include "icd/d3d/buffer_slices.hpp"
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
    /* A buffer on a slice is mapped in place: a discard renames it, anything
     * else writes where the host already reads. A deferred context records
     * its writes as copies, and a discard with no slice free to rename to is
     * one: from then on the buffer is copied, so the host's buffer and the
     * guest's never disagree about where it lives. */
    if (resource.on_slice != 0u && parent_ != nullptr) {
        lines_.once("a buffer on a slice mapped on a deferred context: copied from then on");
        if (!leave_slice_for_copies(resource)) {
            set_error(E_OUTOFMEMORY);
            return;
        }
    }
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
    if (type != D3D10_DDI_MAP_READ && !maps_.open(resource.id, subresource, flags)) {
        lines_.once("a map past every subresource a device can hold open for writing");
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
    d3d::BufferSlices *const slices = session_.slices();
    if (slices == nullptr || parent_ != nullptr || resource.dimension != D3D10DDIRESOURCE_BUFFER ||
        (resource.cpu_access & D3D10_DDI_CPU_ACCESS_WRITE) == 0u)
        return false;
    d3d::BufferSlices::Slice slice = {};
    if (slices->take_slice(bytes, slice) != Status::ok) return false;
    resource.slice_offset = slice.offset_;
    resource.slice_bytes = slice.bytes_;
    resource.on_slice = 1u;
    resource.shadow = session_.slice_view() + slice.offset_;
    return true;
}

bool D3d11Device::rename_slice(ResourceRecord &resource)
{
    AV_SCOPE("D3d11Device::rename_slice");
    d3d::BufferSlices *const slices = session_.slices();
    d3d::BufferSlices::Slice fresh = {};
    if (slices == nullptr || slices->take_slice(resource.shadow_bytes, fresh) != Status::ok) return false;
    slices->retire(d3d::BufferSlices::Slice{resource.slice_offset, resource.slice_bytes});
    resource.slice_offset = fresh.offset_;
    resource.slice_bytes = fresh.bytes_;
    resource.shadow = session_.slice_view() + fresh.offset_;
    return bind_slice(resource);
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
    return d3dproto::av_d3d_BindBufferSlice(wire(), resource.id, session_.slice_memory(), resource.slice_offset,
                                            resource.slice_bytes);
}

/* A deferred context's record is a copy of the immediate one's
 * (context_local), whose slice it names: the immediate record keeps it, and
 * retires it when it renames or destroys the buffer. Retired here too, it went
 * back free twice - once while the immediate buffer was still on it - and two
 * buffers took the same slice: the multithreading sample's per-thread constant
 * buffers left duplicates that computeparticles' constants, renamed a thousand
 * times a batch, then shared, one step reading another's. */
void D3d11Device::leave_slice(ResourceRecord &resource)
{
    AV_SCOPE("D3d11Device::leave_slice");
    d3d::BufferSlices *const slices = session_.slices();
    if (slices != nullptr && parent_ == nullptr)
        slices->retire(d3d::BufferSlices::Slice{resource.slice_offset, resource.slice_bytes});
    resource.slice_offset = 0u;
    resource.slice_bytes = 0u;
    resource.on_slice = 0u;
    resource.shadow = nullptr;
}

}  // namespace av::d3d11
